#include "MapDrawable.h"
#include <atomic>
#include <acdocman.h>
#include <set> // Trik Claude butuh ini

extern TileCache       g_tileCache;
extern TileDownloader  g_tileDownloader;
extern HWND            g_hAcadFrame;
extern UINT            WM_LIVEMAP_REDRAW;
extern MapDrawable* g_pDrawable; // <-- OBAT ERROR C2065

MapDrawable::MapDrawable()  = default;
MapDrawable::~MapDrawable() { unregisterTransient(); }

bool MapDrawable::registerTransient()
{
    if (m_registered) return true;
    AcGiTransientManager* pTM = acgiGetTransientManager();
    if (!pTM) return false;
    AcArray<int> allVp;
    if (!pTM->addTransient(this, AcGiTransientDrawingMode::kAcGiMain, 128, allVp)) return false;
    m_registered = true;
    return true;
}

void MapDrawable::unregisterTransient()
{
    if (!m_registered) return;
    AcGiTransientManager* pTM = acgiGetTransientManager();
    if (pTM) { AcArray<int> allVp; pTM->eraseTransient(this, allVp); }
    m_registered = false;
}

void MapDrawable::scheduleRedraw()
{
    if (!m_registered) return;
    AcGiTransientManager* pTM = acgiGetTransientManager();
    if (pTM) { AcArray<int> allVp; pTM->updateTransient(this, allVp); }
}

/*static*/
void MapDrawable::getViewExtentsWCS(AcGiViewportDraw* vd, AcGePoint3d& minPt, AcGePoint3d& maxPt)
{
    AcGePoint2d dcLL, dcUR;
    vd->viewport().getViewportDcCorners(dcLL, dcUR);

    const AcGePoint3d corners[4] = {
        AcGePoint3d(dcLL.x, dcLL.y, 0.0), AcGePoint3d(dcUR.x, dcLL.y, 0.0),
        AcGePoint3d(dcUR.x, dcUR.y, 0.0), AcGePoint3d(dcLL.x, dcUR.y, 0.0)
    };

    constexpr double kBig = 1.0e30;
    minPt = AcGePoint3d( kBig,  kBig,  0.0);
    maxPt = AcGePoint3d(-kBig, -kBig,  0.0);

    for (int i = 0; i < 4; ++i) {
        AcGePoint3d pt = corners[i];
        
        vd->viewport().doInversePerspective(pt);

        // Safety Net beneran (Pake AND)
        if (abs(pt.x) < 100.0 && abs(pt.y) < 100.0) continue;

        double latDeg = 0.0, lonDeg = 0.0;
        WebMercatorMath::MercatorToLL(pt.x, pt.y, latDeg, lonDeg);
        if (lonDeg < minPt.x) minPt.x = lonDeg;
        if (latDeg < minPt.y) minPt.y = latDeg;
        if (lonDeg > maxPt.x) maxPt.x = lonDeg;
        if (latDeg > maxPt.y) maxPt.y = latDeg;
    }
}

void MapDrawable::subViewportDraw(AcGiViewportDraw* vd)
{
    if (!m_visible || !vd) return;

    AcGePoint3d wcsMin, wcsMax;
    getViewExtentsWCS(vd, wcsMin, wcsMax);

    const double viewWidthDeg = wcsMax.x - wcsMin.x;
    if (viewWidthDeg <= 0.0 || viewWidthDeg > 10.0) return;

    const int vpWidthPx = 1920; 
    const int zoom = (m_zoomOverride > 0) ? m_zoomOverride : TileMath::estimateZoom(viewWidthDeg, vpWidthPx);

    const int maxTile = (1 << zoom) - 1;
    const int txMin = std::max(0, TileMath::lon2tileX(wcsMin.x, zoom) - 1);
    const int txMax = std::min(maxTile, TileMath::lon2tileX(wcsMax.x, zoom) + 1);
    const int tyMin = std::max(0, TileMath::lat2tileY(wcsMax.y, zoom) - 1);
    const int tyMax = std::min(maxTile, TileMath::lat2tileY(wcsMin.y, zoom) + 1);

    constexpr int kMaxSpan = 18; 
    if ((txMax - txMin) > kMaxSpan || (tyMax - tyMin) > kMaxSpan) return;

    std::vector<TileKey> missing;
    std::set<TileKey> parentsPrefetched; 

    for (int tx = txMin; tx <= txMax; ++tx) {
        for (int ty = tyMin; ty <= tyMax; ++ty) {
            const TileKey key{ zoom, tx, ty };
            TileDataPtr tile = g_tileCache.get(key);
            
            if (tile && tile->valid) {
                drawTile(vd, tile);
            } else {
                drawTileFallback(vd, key); 
                missing.push_back(key);

                if (zoom > 0) {
                    TileKey parent{ zoom - 1, tx >> 1, ty >> 1 };
                    if (!g_tileCache.get(parent) && parentsPrefetched.insert(parent).second) {
                        missing.push_back(parent);
                    }
                }
            }
        }
    }

    if (!missing.empty()) {
        g_tileDownloader.enqueue(missing, [](TileDataPtr tile) {
            if (!tile || !tile->valid) return;
            // Kirim surat ke Main Thread pakai OS Windows (1000% Aman dari Crash)
            if (g_hAcadFrame) {
                ::PostMessage(g_hAcadFrame, WM_LIVEMAP_REDRAW, 0, 0);
            }
        });
    }
}

void MapDrawable::drawTile(AcGiViewportDraw* vd, const TileDataPtr& tile) const
{
    if (!tile || !tile->valid) return;

    const TileKey& k = tile->key;
    const double lonW = TileMath::tileX2lon(k.x,     k.z);
    const double lonE = TileMath::tileX2lon(k.x + 1, k.z);
    const double latN = TileMath::tileY2lat(k.y,     k.z);
    const double latS = TileMath::tileY2lat(k.y + 1, k.z);

    double nwE = 0.0, nwN = 0.0; 
    double neE = 0.0, neN = 0.0; 
    double swE = 0.0, swN = 0.0; 

    WebMercatorMath::LLtoMercator(latN, lonW, nwE, nwN);
    WebMercatorMath::LLtoMercator(latN, lonE, neE, neN);
    WebMercatorMath::LLtoMercator(latS, lonW, swE, swN);

    const AcGePoint3d  origin(nwE,        nwN,    -1000.0);
    const AcGeVector3d uVec  (neE - nwE,  neN - nwN,  0.0);
    const AcGeVector3d vVec  (swE - nwE,  swN - nwN,  0.0);

    TileBgraImage img(tile);
    vd->geometry().image(*img.acgiImage(), origin, uVec, vVec);
}

void MapDrawable::drawTileFallback(AcGiViewportDraw* vd, const TileKey& key) const
{
    for (int dz = 1; dz <= 2; ++dz) {
        const int pz = key.z - dz;
        if (pz < 0) break;

        const TileKey parentKey{ pz, key.x >> dz, key.y >> dz };
        TileDataPtr parent = g_tileCache.get(parentKey);
        if (!parent || !parent->valid) continue;

        const int divisions = 1 << dz;  
        const int qx = key.x & (divisions - 1);
        const int qy = key.y & (divisions - 1);

        const int subW = parent->width  / divisions;
        const int subH = parent->height / divisions;
        if (subW <= 0 || subH <= 0) break;

        auto cropped = std::make_shared<TileData>();
        cropped->key    = key;
        cropped->width  = subW;
        cropped->height = subH;
        cropped->bgra.resize(static_cast<size_t>(subW) * subH * 4);
        cropped->valid  = true;

        for (int row = 0; row < subH; ++row) {
            const uint8_t* src = parent->bgra.data()
                + ((qy * subH + row) * parent->width + qx * subW) * 4;
            uint8_t* dst = cropped->bgra.data() + row * subW * 4;
            std::memcpy(dst, src, static_cast<size_t>(subW) * 4);
        }

        drawTile(vd, cropped);
        return; 
    }
}