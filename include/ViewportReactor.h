#pragma once
#include "LiveMapCommon.h"

// ─────────────────────────────────────────────────────────────────────────────
class ViewportReactor : public AcEditorReactor
{
public:
    // --- TAMBAHAN BARU: Pindah ke public paling atas ---
    void nudge();          ///< Record event timestamp atomically.

    using ChangeCallback = std::function<void()>;

    explicit ViewportReactor(ChangeCallback callback,
                             unsigned int   debounceMs = 500);
    ~ViewportReactor() override;

    ViewportReactor(const ViewportReactor&)            = delete;
    ViewportReactor& operator=(const ViewportReactor&) = delete;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    void attach();
    void detach();

    // ── AcEditorReactor overrides  (ARX 2021-2024 confirmed signatures) ──────
    void commandWillStart(const ACHAR* cmdStr) override;
    void commandEnded(const ACHAR* cmdStr) override;
    void commandCancelled(const ACHAR* cmdStr) override;
    void commandFailed(const ACHAR* cmdStr) override;

private:
    // void nudge();  <--- YANG LAMA DI SINI UDAH DIHAPUS
    void debounceLoop();   ///< Background thread: wait for quiet, fire callback.

    ChangeCallback   m_callback;
    unsigned int     m_debounceMs;
    bool             m_attached = false;

    std::atomic<bool>       m_running{ false };
    std::thread             m_thread;
    std::mutex              m_mtx;
    std::condition_variable m_cv;

    std::atomic<std::chrono::steady_clock::time_point::rep> m_lastEventTime{ 0 };
};