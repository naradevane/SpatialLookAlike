/**
 * @file ViewportReactor.cpp
 * @brief ViewportReactor implementation.
 *
 * v3 Changes
 * ───────────
 *  FIX-3  Removed viewportDisplayChanged(), beginRegenAll(), endRegenAll().
 *         All three caused C3668 because they do not match the signatures
 *         of any virtual in AcEditorReactor for ARX 2021.  See header for
 *         full explanation.  Added commandWillStart() and commandFailed()
 *         which ARE confirmed ARX 2021 overrides.
 *
 *  FIX-4  Added file-scope 'extern TileDownloader g_tileDownloader'.
 *         The previous version declared the extern only inside the body of
 *         beginRegenAll() — a function that no longer exists.  Placing it
 *         at file scope makes it available to any method in this TU and
 *         prevents C2065 "undeclared identifier" if future methods need it.
 */

#include "ViewportReactor.h"

// ── File-scope extern for the global TileDownloader singleton ─────────────────
//  FIX-4: This must be at file scope, NOT buried inside a method body.
//         Declared here so any method in this translation unit can use it.
//         Defined in acrxEntryPoint.cpp.
#include "TileManager.h"
extern TileDownloader g_tileDownloader;

// ─────────────────────────────────────────────────────────────────────────────
//  Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

ViewportReactor::ViewportReactor(ChangeCallback callback, unsigned int debounceMs)
    : m_callback(std::move(callback))
    , m_debounceMs(debounceMs)
{}

ViewportReactor::~ViewportReactor()
{
    detach();
}

// ─────────────────────────────────────────────────────────────────────────────
//  attach / detach
// ─────────────────────────────────────────────────────────────────────────────

void ViewportReactor::attach()
{
    if (m_attached) return;

    acedEditor->addReactor(this);

    m_running = true;
    m_thread  = std::thread(&ViewportReactor::debounceLoop, this);

    m_attached = true;
    acutPrintf(_T("\nLiveMap: Viewport reactor attached."));
}

void ViewportReactor::detach()
{
    if (!m_attached) return;

    m_running = false;
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();

    acedEditor->removeReactor(this);
    m_attached = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AcEditorReactor overrides
//
//  SIGNATURE NOTE: Every override below uses the EXACT parameter types from
//  AcEditorReactor in ARX 2021 (aced.h).  Adding or removing const, changing
//  ACHAR* to TCHAR*, or altering the parameter list will produce C3668.
// ─────────────────────────────────────────────────────────────────────────────

void ViewportReactor::commandWillStart(const ACHAR* cmdStr)
{
    // We don't nudge on WillStart because the view hasn't changed yet.
    // However, if the command is REGEN/REGENALL we cancel pending tile
    // downloads to avoid downloading tiles for the about-to-change view.
    if (!cmdStr) return;

    // _tcsicmp is the case-insensitive wide-string compare from <tchar.h>
    if (_tcsicmp(cmdStr, _T("REGEN"))    == 0 ||
        _tcsicmp(cmdStr, _T("REGENALL")) == 0)
    {
        g_tileDownloader.cancelPending();
    }
}

void ViewportReactor::commandEnded(const ACHAR* cmdStr)
{
    // Fires when ZOOM, PAN, 3DORBIT, VIEW, DVIEW, REGEN, etc. complete.
    // Nudge the debounce timer regardless of which command it was —
    // false positives are absorbed cheaply by the 500 ms quiet window.
    (void)cmdStr;
    nudge();
}

void ViewportReactor::commandCancelled(const ACHAR* cmdStr)
{
    // User pressed Escape or right-clicked to cancel.  The view may have
    // moved partially (e.g., cancelled mid-pan), so we still update.
    (void)cmdStr;
    nudge();
}

void ViewportReactor::commandFailed(const ACHAR* cmdStr)
{
    // Command threw an exception or otherwise aborted abnormally.
    // Update the view to reflect wherever the user currently is.
    (void)cmdStr;
    nudge();
}

// ─────────────────────────────────────────────────────────────────────────────
//  nudge  —  thread-safe event timestamp update
// ─────────────────────────────────────────────────────────────────────────────

void ViewportReactor::nudge()
{
    using Clock = std::chrono::steady_clock;
    const auto now = Clock::now().time_since_epoch().count();
    m_lastEventTime.store(now, std::memory_order_relaxed);
    m_cv.notify_one();
}

// ─────────────────────────────────────────────────────────────────────────────
//  debounceLoop  —  background thread
//
//  Wakes every (debounceMs/4) ms.  When the elapsed time since the last
//  nudge() exceeds debounceMs, fires m_callback() once.
// ─────────────────────────────────────────────────────────────────────────────

void ViewportReactor::debounceLoop()
{
    using Clock    = std::chrono::steady_clock;
    using Duration = std::chrono::milliseconds;

    const auto pollInterval = Duration(m_debounceMs / 4 + 1);
    const auto quietPeriod  = Duration(m_debounceMs);

    while (m_running.load(std::memory_order_relaxed))
    {
        {
            std::unique_lock<std::mutex> lk(m_mtx);
            m_cv.wait_for(lk, pollInterval);
        }

        if (!m_running) break;

        const auto lastRaw = m_lastEventTime.load(std::memory_order_relaxed);
        if (lastRaw == 0) continue;

        const auto lastTime = Clock::time_point(Clock::duration(lastRaw));
        if ((Clock::now() - lastTime) >= quietPeriod)
        {
            m_lastEventTime.store(0, std::memory_order_relaxed);

            if (m_callback)
            {
                try   { m_callback(); }
                catch (...) { /* never propagate out of debounce thread */ }
            }
        }
    }
}
