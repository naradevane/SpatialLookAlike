/**
 * @file TileManager.h
 * @brief Thread-safe LRU tile cache and asynchronous WinHTTP tile downloader.
 *
 * DESIGN NOTES
 * ─────────────
 *  TileCache   – LRU map<TileKey, TileDataPtr> protected by a shared_mutex.
 *                Readers (render thread) take a shared lock; writers
 *                (download threads) take an exclusive lock.  This minimises
 *                contention because renders are far more frequent than
 *                cache writes.
 *
 *  TileDownloader – Manages a pool of WinHTTP sessions.  Each tile gets its
 *                own HINTERNET request handle so downloads are truly
 *                parallel.  On completion, the decoded TileData is inserted
 *                into TileCache and a callback fires to trigger a redraw.
 *
 *  Why WinHTTP?  It ships with every Windows installation (winhttp.dll) and
 *  supports both synchronous (simple) and asynchronous (WINHTTP_FLAG_ASYNC)
 *  modes.  We use the synchronous mode on background std::threads to keep the
 *  code readable.  libcurl would also work but adds a deployment dependency.
 */

#pragma once
#include "LiveMapCommon.h"
#include "RasterImageWrapper.h"
#include <list>
#include <shared_mutex>
#include <thread>
#include <queue>
#include <unordered_set>    // FIX: was <set> — std::set needs operator<,
                            //      std::unordered_set uses our TileKeyHash

// ─────────────────────────────────────────────────────────────────────────────
//  TileCache   —  thread-safe LRU cache
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class TileCache
 * @brief LRU cache that stores decoded TileData objects.
 *
 * Eviction policy:  when m_lru.size() > kMaxCacheEntries, the
 * least-recently-used entry is removed.  Because each TileData holds its
 * pixel vector (256×256×4 ≈ 256 KB), the default capacity (64 tiles) caps
 * peak memory at ~16 MB of decoded pixels, a modest cost.
 */
class TileCache
{
public:
    explicit TileCache(int maxEntries = kMaxCacheEntries)
        : m_maxEntries(maxEntries) {}

    // ── Observers ────────────────────────────────────────────────────────────

    /**
     * @brief Look up a tile (shared/read lock).
     * @return Non-null TileDataPtr if found; nullptr otherwise.
     */
    TileDataPtr get(const TileKey& key) const
    {
        std::shared_lock<std::shared_mutex> lk(m_mtx);
        auto it = m_map.find(key);
        if (it == m_map.end()) return nullptr;

        // Promote to MRU position (cast away const — LRU state is logical)
        const_cast<TileCache*>(this)->promote_nolock(it->second.lruIt);
        return it->second.data;
    }

    /// Returns true if the tile is already cached (no lock needed beyond get()).
    bool contains(const TileKey& key) const
    {
        std::shared_lock<std::shared_mutex> lk(m_mtx);
        return m_map.count(key) > 0;
    }

    // ── Mutators ─────────────────────────────────────────────────────────────

    /**
     * @brief Insert a decoded tile (exclusive/write lock).
     * Evicts the LRU entry if the cache is full.
     */
    void put(TileDataPtr tile)
    {
        if (!tile || !tile->valid) return;
        std::unique_lock<std::shared_mutex> lk(m_mtx);

        // Overwrite if already exists
        auto it = m_map.find(tile->key);
        if (it != m_map.end())
        {
            it->second.data = std::move(tile);
            promote_nolock(it->second.lruIt);
            return;
        }

        // Evict LRU if needed
        if (static_cast<int>(m_lru.size()) >= m_maxEntries)
        {
            const TileKey& lruKey = m_lru.back();
            m_map.erase(lruKey);
            m_lru.pop_back();
        }

        // Insert at MRU front
        m_lru.push_front(tile->key);
        Entry e;
        e.data  = std::move(tile);
        e.lruIt = m_lru.begin();
        m_map.emplace(m_lru.front(), std::move(e));
    }

    /// Remove all entries (called on plugin unload).
    void clear()
    {
        std::unique_lock<std::shared_mutex> lk(m_mtx);
        m_map.clear();
        m_lru.clear();
    }

    /// Number of cached tiles (for diagnostics).
    std::size_t size() const
    {
        std::shared_lock<std::shared_mutex> lk(m_mtx);
        return m_map.size();
    }

    /**
     * @brief Collect all currently valid tiles (for the draw call).
     * Returns a snapshot so the caller is not holding the lock during drawing.
     */
    std::vector<TileDataPtr> snapshot() const
    {
        std::shared_lock<std::shared_mutex> lk(m_mtx);
        std::vector<TileDataPtr> result;
        result.reserve(m_map.size());
        for (auto& [key, entry] : m_map)
            result.push_back(entry.data);
        return result;
    }

private:
    struct Entry {
        TileDataPtr                      data;
        std::list<TileKey>::iterator     lruIt;
    };

    void promote_nolock(std::list<TileKey>::iterator it)
    {
        m_lru.splice(m_lru.begin(), m_lru, it);
    }

    mutable std::shared_mutex                           m_mtx;
    std::unordered_map<TileKey, Entry, TileKeyHash>     m_map;
    std::list<TileKey>                                  m_lru;
    int                                                 m_maxEntries;
};

// ─────────────────────────────────────────────────────────────────────────────
//  TileDownloader  —  async WinHTTP download worker pool
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class TileDownloader
 * @brief Downloads map tiles asynchronously using WinHTTP on a fixed-size
 *        worker thread pool and decodes them to BGRA via GDI+.
 *
 * USAGE
 * ─────
 *  1. Call start() once (creates WinHTTP session, launches workers).
 *  2. Call enqueue(keys, onDone) to schedule tiles.  onDone is invoked on
 *     a worker thread—not the AutoCAD main thread.  The callback should
 *     only set an atomic flag; the actual AcGi call happens on the main
 *     thread via AcEditorReactor::commandEnded or PostMessage.
 *  3. Call stop() on plugin unload; joins all workers cleanly.
 */
class TileDownloader
{
public:
    using DoneCallback = std::function<void(TileDataPtr)>;

    explicit TileDownloader(TileCache& cache, int numWorkers = 4)
        : m_cache(cache), m_numWorkers(numWorkers) {}

    ~TileDownloader() { stop(); }

    // ── Lifecycle ────────────────────────────────────────────────────────────

    /**
     * @brief Initialise WinHTTP session and start worker threads.
     * Call once from acrxEntryPoint on plugin load.
     */
    bool start()
    {
        // Open a single WinHTTP session shared by all workers
        m_hSession = ::WinHttpOpen(
            L"AutoCAD-LiveMap/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!m_hSession)
        {
            acutPrintf(_T("\nLiveMap: WinHttpOpen failed, error %lu"), ::GetLastError());
            return false;
        }

        // Set reasonable timeouts (ms): resolve, connect, send, recv
        ::WinHttpSetTimeouts(m_hSession, 5000, 5000, 10000, 15000);

        m_running = true;
        for (int i = 0; i < m_numWorkers; ++i)
            m_workers.emplace_back(&TileDownloader::workerLoop, this);

        return true;
    }

    /**
     * @brief Signal all workers to drain their queues and exit.
     * Blocks until all threads have joined.
     */
    void stop()
    {
        {
            std::lock_guard<std::mutex> lk(m_queueMtx);
            m_running = false;
        }
        m_cv.notify_all();

        for (auto& t : m_workers)
            if (t.joinable()) t.join();
        m_workers.clear();

        if (m_hSession)
        {
            ::WinHttpCloseHandle(m_hSession);
            m_hSession = nullptr;
        }
    }

    // ── Tile enqueue ─────────────────────────────────────────────────────────

    /**
     * @brief Enqueue a batch of tile keys for download.
     *
     * Keys already in the cache are silently skipped.
     * Keys already in-flight (m_inflight) are also skipped.
     * @param onDone  Called on a worker thread once each tile is decoded.
     */
    void enqueue(const std::vector<TileKey>& keys, DoneCallback onDone)
    {
        std::lock_guard<std::mutex> lk(m_queueMtx);
        for (const auto& k : keys)
        {
            // Skip if cached or already downloading
            if (m_cache.contains(k)) continue;
            if (m_inflight.count(k))  continue;

            m_inflight.insert(k);
            m_queue.push({ k, onDone });
        }
        m_cv.notify_all();
    }

    /// Cancel all pending (not yet started) downloads.
    void cancelPending()
    {
        std::lock_guard<std::mutex> lk(m_queueMtx);
        while (!m_queue.empty()) m_queue.pop();
        m_inflight.clear();
    }

private:
    // ── Work item ────────────────────────────────────────────────────────────
    struct WorkItem { TileKey key; DoneCallback cb; };

    // ── Worker thread body ───────────────────────────────────────────────────
    void workerLoop()
    {
        while (true)
        {
            WorkItem item{};
            {
                std::unique_lock<std::mutex> lk(m_queueMtx);
                m_cv.wait(lk, [this] {
                    return !m_running || !m_queue.empty();
                });
                if (!m_running && m_queue.empty()) break;
                item = m_queue.front();
                m_queue.pop();
            }

            // Download + decode
            TileDataPtr tileData = downloadAndDecode(item.key);

            // Cache it (even if !valid, so we don't retry immediately)
            if (tileData && tileData->valid)
                m_cache.put(tileData);

            // Remove from in-flight set
            {
                std::lock_guard<std::mutex> lk(m_queueMtx);
                m_inflight.erase(item.key);
            }

            // Notify caller
            if (item.cb) item.cb(tileData);
        }
    }

    // ── HTTP download ────────────────────────────────────────────────────────
    /**
     * @brief Synchronously download one tile and decode it to BGRA.
     *
     * Runs entirely on a worker thread.  All WinHTTP handles are opened and
     * closed within this call, so there is no handle leakage even if an
     * exception occurs (using RAII wrapper below).
     */
    TileDataPtr downloadAndDecode(const TileKey& key)
    {
        // ── 1. Build the URL path ─────────────────────────────────────────────
        wchar_t path[256];
        _snwprintf_s(path, _countof(path), _TRUNCATE,
                     kTilePath, key.x, key.y, key.z);

        // ── 2. Open per-request connection handle ─────────────────────────────
        HINTERNET hConnect = ::WinHttpConnect(
            m_hSession, kTileHost,
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) return nullptr;

        // RAII wrapper — ensures WinHttpCloseHandle on all exit paths
        auto closeConnect = [](HINTERNET h){ if (h) ::WinHttpCloseHandle(h); };
        std::unique_ptr<void, decltype(closeConnect)> connectGuard(hConnect, closeConnect);

        // ── 3. Open request handle ────────────────────────────────────────────
        HINTERNET hRequest = ::WinHttpOpenRequest(
            hConnect, L"GET", path,
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (!hRequest) return nullptr;

        std::unique_ptr<void, decltype(closeConnect)> requestGuard(hRequest, closeConnect);

        // ── 4. Add headers (User-Agent is set at session level; add others here)
        // Google Maps tiles may require a realistic User-Agent string.
        ::WinHttpAddRequestHeaders(hRequest,
            L"Accept: image/png,image/*",
            static_cast<DWORD>(-1L),
            WINHTTP_ADDREQ_FLAG_ADD);

        // ── 5. Send request ───────────────────────────────────────────────────
        if (!::WinHttpSendRequest(hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
            return nullptr;

        if (!::WinHttpReceiveResponse(hRequest, nullptr))
            return nullptr;

        // ── 6. Check HTTP status ──────────────────────────────────────────────
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(DWORD);
        ::WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (statusCode != 200) return nullptr;

        // ── 7. Read response body ─────────────────────────────────────────────
        std::vector<uint8_t> pngBytes;
        pngBytes.reserve(32 * 1024);   // tiles are typically 10–40 KB

        DWORD available = 0;
        while (::WinHttpQueryDataAvailable(hRequest, &available) && available > 0)
        {
            const std::size_t prev = pngBytes.size();
            pngBytes.resize(prev + available);
            DWORD read = 0;
            if (!::WinHttpReadData(hRequest,
                    pngBytes.data() + prev, available, &read))
            {
                pngBytes.resize(prev);
                break;
            }
            pngBytes.resize(prev + read);
        }

        if (pngBytes.empty()) return nullptr;

        // ── 8. Decode PNG → BGRA ──────────────────────────────────────────────
        auto tile = std::make_shared<TileData>();
        tile->key  = key;

        if (!TileDecoder::DecodePngToBGRA(pngBytes, *tile))
            return nullptr;   // GDI+ error; tile->valid remains false

        return tile;   // shared_ptr returned to workerLoop
    }

    // ── Data members ─────────────────────────────────────────────────────────
    TileCache&              m_cache;
    HINTERNET               m_hSession = nullptr;
    int                     m_numWorkers;

    std::vector<std::thread>                        m_workers;
    std::mutex                                      m_queueMtx;
    std::condition_variable                         m_cv;
    std::queue<WorkItem>                            m_queue;
    std::unordered_set<TileKey, TileKeyHash>        m_inflight; ///< in-progress keys
    bool                                            m_running = false;
};
