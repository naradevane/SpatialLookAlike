/**
 * @file MapDrawable.h
 * @brief Custom AcGiDrawable that renders geo-referenced map tiles to the
 *        AutoCAD viewport background using the Transient Graphics system.
 *
 * KEY DESIGN CHOICES
 * ───────────────────
 *  • Derives from AcGiDrawable (NOT AcDbEntity).
 *    → No DWG database involvement, no database locking, no UNDO entries.
 *
 *  • Registered with AcGiTransientManager in AcGiTransientDrawingMode::kAcGiMain
 *    so it is redrawn every time the view regenerates.
 *
 *  • subViewportDraw() is the only draw method implemented.
 *    subWorldDraw() returns false, forcing the system to always call
 *    subViewportDraw(), which gives us access to view-plane coordinates
 *    needed to map screen pixels back to WGS84.
 *
 *  • For each visible tile, a TileBgraImage wraps the TileDataPtr with an
 *    AcGiImageBGRA32 view.  The AcGePoint3d origin and u/v vectors are computed so the tile exactly covers its geographic
 *    extent in World Coordinate System units (assumed = decimal degrees).
 *
 * COORDINATE SYSTEM ASSUMPTION
 * ──────────────────────────────
 *  Drawing WCS = WGS84 decimal degrees (X = longitude, Y = latitude).
 *  This is the most common setup for GIS-sourced DWG files.
 *  For UTM or other CRS you must add a coordinate projection transform
 *  before calling TileMath functions.
 */

#pragma once
#include "LiveMapCommon.h"
#include "RasterImageWrapper.h"
#include "TileManager.h"

// ─────────────────────────────────────────────────────────────────────────────

class MapDrawable : public AcGiDrawable
{
public:
    MapDrawable();
    ~MapDrawable() override;

    MapDrawable(const MapDrawable&)            = delete;
    MapDrawable& operator=(const MapDrawable&) = delete;

    // ── AcGiDrawable — ALL five pure-virtual methods MUST be overridden ───────
    //
    //  Omitting isPersistent() or id() causes error C2259
    //  ("cannot instantiate abstract class") because they are declared as
    //  pure virtual in AcGiDrawable (ARX 2021+).

    /**
     * @brief Returns false — this drawable is NOT stored in the DWG database.
     * A persistent drawable would be an AcDbEntity subclass with an AcDbStub
     * handle.  Returning false tells AutoCAD not to try to save/reload us.
     */
    bool isPersistent() const override { return false; }

    /**
     * @brief Returns nullptr — non-persistent drawables have no database ID.
     * AcGiTransientManager tracks us by raw pointer, not by AcDbStub handle.
     */
    AcDbObjectId id() const override { return AcDbObjectId::kNull; }

    /**
     * @brief Return the drawable type.
     * kGeometry = we represent geometry, not a light or camera.
     */
    DrawableType drawableType() const override
    { return AcGiDrawable::kGeometry; }

    /**
     * @brief Set rendering attributes on the trait object.
     *
     * For a background overlay we need:
     *  • kDrawableNone (0) — we are not a database entity
     *
     * Returning kDrawableIsAnEntity (1) would cause AutoCAD to try to query
     * selection / layer state on us, causing null-pointer crashes.
     */
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits* traits) override
    {
        (void)traits;
        return kDrawableNone;   // 0 — non-entity, no special flags
    }

    /**
     * @brief subWorldDraw — return false to force subViewportDraw every time.
     * We need viewport-specific eye-to-world transforms, which are only
     * available in subViewportDraw.
     */
    bool subWorldDraw(AcGiWorldDraw* /*wd*/) override { return false; }

    /**
     * @brief subViewportDraw — the core rendering method (see MapDrawable.cpp).
     */
    void subViewportDraw(AcGiViewportDraw* vd) override;

    // ── Public API ────────────────────────────────────────────────────────────

    /** Trigger an AcGi transient update (safe to call from main thread only). */
    void scheduleRedraw();

    void setVisible(bool v) { m_visible = v; scheduleRedraw(); }
    bool isVisible() const noexcept { return m_visible; }

    void setZoomOverride(int z) noexcept { m_zoomOverride = z; }

    /** Register with AcGiTransientManager for ALL viewports (empty-array API). */
    bool registerTransient();

    /** Remove from transient manager; safe to call multiple times. */
    void unregisterTransient();

private:
    /**
     * @brief Render one tile via rasterImageDc() (corrected method name).
     * Takes TileDataPtr (shared ownership) to avoid the 256 KB deep-copy
     * that the previous const-ref signature required.
     */
    void drawTile(AcGiViewportDraw* vd, const TileDataPtr& tile) const;
    void drawTileFallback(AcGiViewportDraw* vd, const TileKey& key) const;

    /**
     * @brief Compute the SW/NE WCS bounding box of the current view.
     * Uses AcGiViewport::getViewportDcCorners + getEyeToWorldTransform.
     */
    static void getViewExtentsWCS(AcGiViewportDraw* vd,
                                  AcGePoint3d&      minPt,
                                  AcGePoint3d&      maxPt);

    bool  m_visible      = true;
    int   m_zoomOverride = 0;   ///< 0 = auto-compute from view width
    bool  m_registered   = false;
    // NOTE: No m_viewportIds needed. The empty-array overload of
    // addTransient/updateTransient/eraseTransient covers ALL viewports.
};

