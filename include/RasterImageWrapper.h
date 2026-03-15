/**
 * @file RasterImageWrapper.h
 * @brief  Wraps a TileData BGRA buffer for consumption by
 *         AcGiGeometry::rasterImageDc(), plus the GDI+ PNG decoder.
 *
 * WHY AcGiImageBGRA32 INSTEAD OF AcGiRasterImage
 * ─────────────────────────────────────────────────
 *  Previous versions of this file subclassed AcGiRasterImage and attempted
 *  to include <acgiimage.h> or <imgent.h>.  Both approaches failed in ARX 2021:
 *
 *  • <acgiimage.h>  — does NOT exist as a standalone file in the ARX 2021 SDK
 *                     directory.  Causes C1083 "cannot open include file"
 *                     which cascades into 100+ C2504 errors.
 *
 *  • <imgent.h>     — defines AcDbRasterImage, a DWG *entity* (database object).
 *                     It does NOT define AcGiRasterImage.  Including it requires
 *                     prerequisite headers (dbimage.h, gepnt3d.h, ARX class
 *                     macros) to arrive first; without them it fires C3646
 *                     ("classVersion: unknown override specifier") on every
 *                     ACRX_DECLARE_MEMBERS macro inside the header.
 *
 *  THE CORRECT APPROACH  (ARX 2021-2024)
 *  ───────────────────────────────────────
 *  Both AcGiRasterImage and AcGiImageBGRA32 are declared directly in <acgi.h>.
 *  No extra include is needed or should be used.
 *
 *  AcGiImageBGRA32 is preferred here because:
 *    1. It is a CONCRETE class — no pure-virtual methods to override.
 *       The old AcGiRasterImage subclass required overriding 9 pure-virtual
 *       methods and was the source of multiple C2259 / C3668 errors.
 *    2. It takes a raw pixel pointer directly — no copyFrom() overhead.
 *       AcGi reads the buffer once synchronously inside rasterImageDc().
 *    3. AcGiGeometry has a dedicated rasterImageDc() overload that accepts
 *       AcGiImageBGRA32*, with a simpler signature (no uvBoundary needed).
 *
 * PIXEL FORMAT CONTRACT
 * ──────────────────────
 *  GDI+ PixelFormat32bppARGB stores bytes in memory as B, G, R, A (BGRA).
 *  AcGiImageBGRA32 documents the same layout.  AutoCAD's DX11 and OpenGL
 *  rasterizers on Windows read this format natively.  If tiles appear with
 *  swapped red/blue channels on a specific GPU driver, swap B↔R inside
 *  TileDecoder::DecodePngToBGRA() by changing PixelFormat32bppARGB to
 *  PixelFormat32bppRGB and manually writing the alpha channel.
 *
 * MEMORY OWNERSHIP
 * ──────────────────
 *  AcGiImageBGRA32 stores a RAW (non-owning) Adesk::UInt8* pointer.
 *  TileBgraImage wraps both:
 *    • m_tile  (shared_ptr<TileData>) — keeps the bgra vector alive
 *    • m_img   (AcGiImageBGRA32)      — non-owning view into m_tile->bgra
 *  As long as TileBgraImage exists, the pixel buffer is guaranteed live.
 *  AutoCAD calls rasterImageDc() synchronously on the main thread, so no
 *  lifetime race is possible.
 */

#pragma once
#include "LiveMapCommon.h"
// AcGiImageBGRA32 is in acgi.h (included via LiveMapCommon.h) — NO extra header.

// ─────────────────────────────────────────────────────────────────────────────
//  TileBgraImage
//  Wraps TileData + AcGiImageBGRA32 for a single rasterImageDc() call.
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class TileBgraImage
 * @brief RAII holder that pairs a TileDataPtr (pixel ownership) with an
 *        AcGiImageBGRA32 (AcGi-visible non-owning view).
 *
 * Usage in drawTile():
 * @code
 *   TileBgraImage img(tile);        // tile is TileDataPtr
 *   vd->geometry().rasterImageDc(
 *       origin, uVec, vVec,
 *       img.acgiImage(),            // AcGiImageBGRA32*
 *       false);                     // transparency
 * @endcode
 */
class TileBgraImage
{
public:
    // ── Construction ─────────────────────────────────────────────────────────

    /**
     * @param tile  Shared ownership of the decoded tile.
     *              The bgra vector must be non-empty and valid.
     */
    explicit TileBgraImage(TileDataPtr tile)
        : m_tile(std::move(tile))
        , m_img(
            static_cast<Adesk::UInt32>(m_tile->width),
            static_cast<Adesk::UInt32>(m_tile->height),
            reinterpret_cast<AcGiPixelBGRA32*>(m_tile->bgra.data())
          )
    {
    }

    ~TileBgraImage() = default;

    // Non-copyable (AcGiImageBGRA32 holds a raw pointer; copying would alias it)
    TileBgraImage(const TileBgraImage&)            = delete;
    TileBgraImage& operator=(const TileBgraImage&) = delete;

    // ── Accessor ─────────────────────────────────────────────────────────────

    /**
     * @brief Return a pointer to the AcGiImageBGRA32 for use with
     *        AcGiGeometry::rasterImageDc().
     *
     * The pointer is valid for the lifetime of this TileBgraImage object.
     * Since rasterImageDc() is synchronous, this is always safe inside
     * a subViewportDraw() call.
     */
    const AcGiImageBGRA32* acgiImage() const noexcept { return &m_img; }

private:
    TileDataPtr      m_tile;   ///< owns the bgra pixel data
    AcGiImageBGRA32  m_img;    ///< non-owning AcGi view into m_tile->bgra
};


// ─────────────────────────────────────────────────────────────────────────────
//  GDI+ PNG → BGRA32 decoder
//
//  Runs on worker threads (TileDownloader::workerLoop).
//  GdiplusStartup() must have been called before first use (acrxEntryPoint.cpp).
// ─────────────────────────────────────────────────────────────────────────────
extern std::atomic<int> g_mapOpacity; // Panggil variabel dari acrxEntryPoint

namespace TileDecoder
{
    /**
     * @brief Decode a PNG byte stream into a 32-bit BGRA pixel buffer.
     *
     * Uses GDI+ (gdiplus.lib — ships with every Windows installation).
     * Produces bytes in B, G, R, A order (GDI+ PixelFormat32bppARGB in-memory
     * layout), which is exactly what AcGiImageBGRA32 expects.
     *
     * @param pngBytes  Raw PNG file bytes as received from WinHTTP.
     * @param outTile   Populated on success: width, height, bgra vector, valid=true.
     * @return true on success; false on any GDI+ error (outTile.valid stays false).
     */
    inline bool DecodePngToBGRA(const std::vector<uint8_t>& pngBytes,
                                TileData&                   outTile)
    {
        if (pngBytes.empty()) return false;

        // ── 1. Wrap raw bytes in a COM IStream ────────────────────────────────
        //  GDI+ Bitmap(IStream*) requires a seekable stream.
        //  GlobalAlloc/CreateStreamOnHGlobal is the standard Windows approach.
        HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, pngBytes.size());
        if (!hMem) return false;

        {
            void* pMem = ::GlobalLock(hMem);
            if (!pMem) { ::GlobalFree(hMem); return false; }
            std::memcpy(pMem, pngBytes.data(), pngBytes.size());
            ::GlobalUnlock(hMem);
        }

        IStream* pStream = nullptr;
        // fDeleteOnRelease=TRUE: pStream takes ownership of hMem
        if (FAILED(::CreateStreamOnHGlobal(hMem, TRUE, &pStream)))
        {
            ::GlobalFree(hMem);
            return false;
        }
        // hMem is now owned by pStream; do NOT call GlobalFree after this.

        // ── 2. Decode PNG via GDI+ ────────────────────────────────────────────
        Gdiplus::Bitmap bmp(pStream);
        pStream->Release();   // also releases hMem (fDeleteOnRelease=TRUE)

        if (bmp.GetLastStatus() != Gdiplus::Ok) return false;

        const int w = static_cast<int>(bmp.GetWidth());
        const int h = static_cast<int>(bmp.GetHeight());
        if (w <= 0 || h <= 0) return false;

        // ── 3. Lock bits as 32bppARGB (in-memory = BGRA) ─────────────────────
        Gdiplus::BitmapData bmpData{};
        Gdiplus::Rect       rect(0, 0, w, h);

        if (bmp.LockBits(&rect,
                         Gdiplus::ImageLockModeRead,
                         PixelFormat32bppARGB,
                         &bmpData) != Gdiplus::Ok)
            return false;

        // ── 4. Copy into owned vector ─────────────────────────────────────────
        outTile.width  = w;
        outTile.height = h;
        outTile.bgra.resize(static_cast<std::size_t>(w) * h * 4);

        // bmpData.Stride may be negative (bottom-up DIB) — copy row by row
        for (int row = 0; row < h; ++row)
        {
            const uint8_t* src =
                reinterpret_cast<const uint8_t*>(bmpData.Scan0)
                + static_cast<std::ptrdiff_t>(row) * bmpData.Stride;

            uint8_t* dst =
                outTile.bgra.data()
                + static_cast<std::ptrdiff_t>(row) * w * 4;

            std::memcpy(dst, src, static_cast<std::size_t>(w) * 4);
        }

        bmp.UnlockBits(&bmpData);

        // --- HACK OPACITY DINAMIS ---
        int currentAlpha = g_mapOpacity.load();
        for (std::size_t i = 3; i < outTile.bgra.size(); i += 4)
        {
            outTile.bgra[i] = static_cast<uint8_t>(currentAlpha); 
        }
        // --------------------------------------------------

        outTile.valid = true;
        return true;
    }

} // namespace TileDecoder
