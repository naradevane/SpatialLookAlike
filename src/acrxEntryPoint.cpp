#pragma comment(lib, "accore.lib")
#pragma comment(lib, "rxapi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

#include <fstream>
#include "acdocman.h"
#include "MapDrawable.h"
#include "TileManager.h"
#include "ViewportReactor.h"
#include "SlaImporter.h"
#include "SlaExporter.h"
#include <adscodes.h>
#include <rxregsvc.h>    // acrxDynamicLinker, acrxLoadApp
#include <aced.h>        // acedEditor, acedRegCmds, acedGetAcadFrame
#include <acdocman.h>    // acDocManager
#include <atomic>

// --- INI MESIN INJEKSI LISP KE DALEM C++ ---
const char* lspCode = R"LISP(
(vl-load-com)
(defun hex2dec (h / d i c n) (setq d 0 i 1 h (strcase h)) (while (<= i (strlen h)) (setq c (ascii (substr h i 1))) (setq n (if (<= c 57) (- c 48) (- c 55))) (setq d (+ (* d 16) n)) (setq i (1+ i))) d)
(defun sync-spm-crs ( / nod dict rec hex_str len val b1 b2 b3 b4 srid zone hemi) (setq nod (namedobjdict)) (setq dict (dictsearch nod "$SPM_Srid")) (if dict (progn (setq rec (assoc 1004 dict)) (if rec (progn (setq hex_str (cdr rec) len (strlen hex_str)) (setq val (substr hex_str (- len 9) 8)) (setq b1 (hex2dec (substr val 1 2)) b2 (hex2dec (substr val 3 2))) (setq b3 (hex2dec (substr val 5 2)) b4 (hex2dec (substr val 7 2))) (setq srid (+ b1 (* 256 b2) (* 65536 b3) (* 16777216 b4))) (cond ((and (>= srid 32701) (<= srid 32760)) (setq zone (- srid 32700) hemi "S") (setenv "KMZ_UTM_ZONE" (itoa zone)) (setenv "KMZ_HEMISPHERE" hemi) (setenv "KMZ_CRS_TYPE" "UTM")) ((and (>= srid 32601) (<= srid 32660)) (setq zone (- srid 32600) hemi "N") (setenv "KMZ_UTM_ZONE" (itoa zone)) (setenv "KMZ_HEMISPHERE" hemi) (setenv "KMZ_CRS_TYPE" "UTM")) ((= srid 3857) (setenv "KMZ_CRS_TYPE" "3857"))))))) (princ))
(defun build-temp-dcl ( / tmp_file fp dcl_str ) (setq tmp_file (vl-filename-mktemp "sla_ui.dcl")) (setq fp (open tmp_file "w")) (setq dcl_str "kmz_import_dialog : dialog { label = \"SPATIALLOOKALIKE - Importer\"; : boxed_row { label = \"1. Pilih File Data\"; : edit_box { key = \"file_path\"; label = \"Path:\"; width = 50; } : button { key = \"btn_browse\"; label = \"Browse...\"; width = 12; } } : boxed_column { label = \"2. Opsi Import\"; : radio_row { key = \"import_type\"; : radio_button { key = \"opt_point\"; label = \"AutoCAD Point\"; value = \"1\"; } : radio_button { key = \"opt_block\"; label = \"AutoCAD Block\"; } } : popup_list { key = \"block_list\"; label = \"Pilih Block:\"; width = 35; } : toggle { key = \"chk_label\"; label = \"Bikin Teks Label dari tag <name>\"; value = \"1\"; } } : row { : button { key = \"btn_settings\"; label = \"CRS Settings\"; width = 25; } : spacer { width = 10; } ok_cancel; } } kmz_settings_dialog : dialog { label = \"CRS Settings\"; : boxed_column { label = \"Coordinate Reference System (CRS)\"; : radio_row { key = \"crs_type\"; : radio_button { key = \"opt_utm\"; label = \"UTM (Lokal)\"; } : radio_button { key = \"opt_3857\"; label = \"Web Mercator (Global)\"; } } : edit_box { key = \"set_utm_zone\"; label = \"UTM Zone (Ketik AUTO / Angka):\"; width = 15; } : radio_row { key = \"set_hemisphere\"; : radio_button { key = \"opt_south\"; label = \"South (S)\"; } : radio_button { key = \"opt_north\"; label = \"North (N)\"; } } : text { label = \"*UTM Zone diabaikan jika memilih Web Mercator.\"; } } ok_only; }") (write-line dcl_str fp) (close fp) tmp_file)
(defun get_drawing_blocks ( / blk blk_name blk_list ) (setq blk (tblnext "BLOCK" T)) (while blk (setq blk_name (cdr (assoc 2 blk))) (if (/= (substr blk_name 1 1) "*") (setq blk_list (append blk_list (list blk_name)))) (setq blk (tblnext "BLOCK"))) blk_list)
(defun c:SLAIMPORT ( / dcl_file dcl_id file_path import_type block_name use_label result my_blocks selected_idx *kmz_utm_zone* *kmz_hemisphere* *crs_type*)
  (sync-spm-crs) (setq *kmz_utm_zone* (if (and (getenv "KMZ_UTM_ZONE") (/= (getenv "KMZ_UTM_ZONE") "")) (getenv "KMZ_UTM_ZONE") "49")) (setq *kmz_hemisphere* (if (getenv "KMZ_HEMISPHERE") (getenv "KMZ_HEMISPHERE") "S")) (setq *crs_type* (if (getenv "KMZ_CRS_TYPE") (getenv "KMZ_CRS_TYPE") "UTM"))
  (setq dcl_file (build-temp-dcl)) (setq dcl_id (load_dialog dcl_file)) (if (not (new_dialog "kmz_import_dialog" dcl_id)) (progn (alert "Gagal nampilin UI.") (exit)))
  (set_tile "file_path" "") (set_tile "opt_point" "1") (mode_tile "block_list" 1) (set_tile "chk_label" "1") (setq my_blocks (get_drawing_blocks)) (if my_blocks (progn (start_list "block_list") (mapcar 'add_list my_blocks) (end_list) (set_tile "block_list" "0")) (progn (start_list "block_list") (add_list "-- TIDAK ADA BLOCK --") (end_list) (mode_tile "opt_block" 1)))
  (action_tile "opt_point" "(mode_tile \"block_list\" 1)") (action_tile "opt_block" "(mode_tile \"block_list\" 0)") (action_tile "btn_browse" "(setq file_path (getfiled \"Pilih File KMZ/KML\" \"\" \"kml;kmz\" 4)) (if file_path (set_tile \"file_path\" file_path))")
  (action_tile "btn_settings" "(if (new_dialog \"kmz_settings_dialog\" dcl_id) (progn (if (= *crs_type* \"3857\") (set_tile \"opt_3857\" \"1\") (set_tile \"opt_utm\" \"1\")) (set_tile \"set_utm_zone\" *kmz_utm_zone*) (if (= *kmz_hemisphere* \"S\") (set_tile \"opt_south\" \"1\") (set_tile \"opt_north\" \"1\")) (action_tile \"accept\" \"(progn (setq *crs_type* (if (= (get_tile \\\"opt_3857\\\") \\\"1\\\") \\\"3857\\\" \\\"UTM\\\")) (setq *kmz_utm_zone* (strcase (get_tile \\\"set_utm_zone\\\"))) (setq *kmz_hemisphere* (if (= (get_tile \\\"opt_south\\\") \\\"1\\\") \\\"S\\\" \\\"N\\\")) (setenv \\\"KMZ_CRS_TYPE\\\" *crs_type*) (setenv \\\"KMZ_UTM_ZONE\\\" *kmz_utm_zone*) (setenv \\\"KMZ_HEMISPHERE\\\" *kmz_hemisphere*) (done_dialog 1))\") (start_dialog)))")
  (action_tile "accept" "(progn (setq file_path (get_tile \"file_path\")) (setq import_type (if (= (get_tile \"opt_point\") \"1\") \"POINT\" \"BLOCK\")) (if my_blocks (progn (setq selected_idx (atoi (get_tile \"block_list\"))) (setq block_name (nth selected_idx my_blocks))) (setq block_name \"NONE\")) (setq use_label (get_tile \"chk_label\")) (if (= file_path \"\") (alert \"Pilih filenya dulu brok!\") (done_dialog 1)))")
  (action_tile "cancel" "(done_dialog 0)")
  (setq result (start_dialog)) (unload_dialog dcl_id) (vl-file-delete dcl_file)
  (if (= result 1) (progn (setenv "KMZ_FILE_PATH" file_path) (setenv "KMZ_IMPORT_TYPE" import_type) (setenv "KMZ_BLOCK_NAME" block_name) (setenv "KMZ_USE_LABEL" use_label) (command "SLA_CPP_IMPORT"))) (princ)
)
(defun c:SLAEXPORT () (command "SLA_CPP_EXPORT") (princ))
(princ "\n=============================================")
(princ "\n[SPATIALLOOKALIKE (SLA) Master Engine - Loaded]")
(princ "\nCommands: SLAIMPORT, SLAEXPORT, SLAMAP_ON, SLAMAP_OFF, SLAMAP_OPACITY")
(princ "\n=============================================")
(princ)
)LISP";

void InjectSlaLisp() {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring lspPath = std::wstring(tempPath) + L"SlaMasterUI.lsp";

    std::ofstream out(lspPath);
    out << lspCode;
    out.close();

    std::wstring loadStr = L"(load \"";
    for (auto c : lspPath) {
        if (c == L'\\') loadStr += L"/";
        else loadStr += c;
    }
    loadStr += L"\")\n";

    if (acDocManager->curDocument()) {
        acDocManager->sendStringToExecute(acDocManager->curDocument(), loadStr.c_str(), false, true);
    }
}
// ----------------------------------------------

static ViewportReactor* g_pReactor = nullptr;
MapDrawable* g_pDrawable = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
//  Global singletons
// ─────────────────────────────────────────────────────────────────────────────

TileCache       g_tileCache;
// --- TAMBAHAN BARU: Variabel Global Opacity ---
// Default kita set 153 (sekitar 60% opacity)
std::atomic<int> g_mapOpacity{ 153 }; 
// ----------------------------------------------
TileDownloader  g_tileDownloader(g_tileCache, /*numWorkers=*/8);

static ULONG_PTR g_gdiplusToken = 0;

// ─────────────────────────────────────────────────────────────────────────────
//  Cross-thread notification
//
//  FIX-MT / FIX-4b:
//  g_hAcadFrame replaces g_mainThreadId.  PostMessage to an HWND is the
//  correct Windows mechanism for cross-thread UI notification.
//  The frame HWND is captured once at load time from acedGetAcadFrame().
// ─────────────────────────────────────────────────────────────────────────────

HWND g_hAcadFrame   = nullptr;   ///< AutoCAD main frame window (captured at load)
UINT WM_LIVEMAP_REDRAW = 0;      ///< Custom registered message

// ─────────────────────────────────────────────────────────────────────────────
//  Window subclass hook
//
//  Intercepts WM_LIVEMAP_REDRAW posted by worker threads (tile downloaded)
//  or by the debounce thread (viewport changed).  We are guaranteed to be on
//  AutoCAD's main thread inside this WndProc → AcGi calls are safe here.
// ─────────────────────────────────────────────────────────────────────────────

static WNDPROC g_origWndProc = nullptr;

// --- TAMBAHAN BARU: Global Mouse Hook ---
HHOOK g_hMouseHook = nullptr;

LRESULT CALLBACK LiveMapMouseHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        MSG* pMsg = reinterpret_cast<MSG*>(lParam);
        // Tangkap Wheel (Zoom) dan MButtonUp (Selesai Pan) di jendela mana pun
        if (pMsg->message == WM_MOUSEWHEEL || pMsg->message == WM_MBUTTONUP)
        {
            if (g_pReactor) {
                g_pReactor->nudge(); // Panggil debounce
            }
        }
    }
    return ::CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}
// ----------------------------------------

LRESULT CALLBACK LiveMapWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_LIVEMAP_REDRAW)
    {
        if (g_pDrawable) {
            g_pDrawable->scheduleRedraw();
            
            HWND hDoc = adsw_acadDocWnd();
            if (hDoc) ::InvalidateRect(hDoc, NULL, FALSE);
        }
        return 0;
    }
    return ::CallWindowProc(g_origWndProc, hWnd, uMsg, wParam, lParam);
}

static void installWindowHook()
{
    g_hAcadFrame = adsw_acadMainWnd();
    if (!g_hAcadFrame) return;

    g_origWndProc = reinterpret_cast<WNDPROC>(
        ::SetWindowLongPtr(g_hAcadFrame, GWLP_WNDPROC,
                           reinterpret_cast<LONG_PTR>(LiveMapWndProc)));

    // --- PASANG SENSOR MOUSE DEWA ---
    if (!g_hMouseHook) {
        g_hMouseHook = ::SetWindowsHookEx(WH_GETMESSAGE, LiveMapMouseHook, NULL, ::GetCurrentThreadId());
    }
}

static void removeWindowHook()
{
    if (g_hAcadFrame && g_origWndProc)
    {
        ::SetWindowLongPtr(g_hAcadFrame, GWLP_WNDPROC,
                           reinterpret_cast<LONG_PTR>(g_origWndProc));
        g_origWndProc = nullptr;
    }
    g_hAcadFrame = nullptr;

    // --- COPOT SENSOR MOUSE ---
    if (g_hMouseHook) {
        ::UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  AutoCAD commands
// ─────────────────────────────────────────────────────────────────────────────

static void cmdLiveMapOn()
{
    if (!g_pDrawable) {
        g_pDrawable = new MapDrawable();
        g_pDrawable->registerTransient();
    }

    // --- PASANG SENSOR MATA ---
    if (!g_pReactor) {
        g_pReactor = new ViewportReactor([]() {
            if (g_hAcadFrame) ::PostMessage(g_hAcadFrame, WM_LIVEMAP_REDRAW, 0, 0);
        }, 50); 
        g_pReactor->attach(); // Nyalain sensornya
    }

    g_pDrawable->setVisible(true);
    g_pDrawable->scheduleRedraw();
    acutPrintf(_T("\nLiveMap: Satellite background ON."));
}

static void cmdLiveMapOff()
{
    if (g_pDrawable) {
        g_pDrawable->setVisible(false);
        g_pDrawable->scheduleRedraw();
    }

    // --- MATIIN SENSOR ---
    if (g_pReactor) {
        g_pReactor->detach();
        delete g_pReactor;
        g_pReactor = nullptr;
    }
    
    acutPrintf(_T("\nLiveMap: Satellite background OFF."));
}

static void cmdLiveMapOpacity()
{
    int val = 0;
    // Minta input dari user di command line AutoCAD (10-100)
    int ret = acedGetInt(_T("\nMasukkan nilai Opacity Maps (10-100) <60>: "), &val);
    
    if (ret == RTCAN) return; // Kalau user pencet ESC
    if (ret == RTNONE) val = 60; // Kalau user cuma pencet Enter/Spasi, set default 60%

    // Batasin biar ga masukin angka aneh-aneh
    if (val < 10) val = 10;
    if (val > 100) val = 100;

    // Konversi skala persen (0-100) ke skala byte (0-255) buat engine GDI+
    int alpha = (val * 255) / 100;
    g_mapOpacity.store(alpha);

    acutPrintf(_T("\nLiveMap Opacity diset ke %d%%."), val);

    // KUNCI PENTING: Bersihkan cache biar tile di-decode ulang pake alpha baru
    g_tileCache.clear();
    
    // Minta AutoCAD gambar ulang layarnya
    if (g_pDrawable) {
        g_pDrawable->scheduleRedraw();
        HWND hDoc = adsw_acadDocWnd();
        if (hDoc) ::InvalidateRect(hDoc, NULL, FALSE);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  onLoad
// ─────────────────────────────────────────────────────────────────────────────

static void onLoad()
{
    acutPrintf(_T("\nLiveMap: Loading..."));

    // ── 1. Register custom window message ────────────────────────────────────
    //  RegisterWindowMessage is idempotent — safe to call from hot-reload.
    WM_LIVEMAP_REDRAW = ::RegisterWindowMessage(L"WM_LIVEMAP_REDRAW_B7F2A1");

    // ── 2. GDI+ ──────────────────────────────────────────────────────────────
    Gdiplus::GdiplusStartupInput si;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &si, nullptr);

    // ── 3. WinHTTP worker pool ────────────────────────────────────────────────
    if (!g_tileDownloader.start())
    {
        acutPrintf(_T("\nLiveMap: WinHTTP init failed — plugin will not download tiles."));
        // Non-fatal: plugin loads but shows no tiles.
    }

    // ── 4. Window hook (must come before reactor, which may PostMessage) ──────
    installWindowHook();

    // ── 5. Create and register the drawable ──────────────────────────────────
    g_pDrawable = new MapDrawable();
    if (!g_pDrawable->registerTransient())
    {
        acutPrintf(_T("\nLiveMap: registerTransient failed."));
        delete g_pDrawable;
        g_pDrawable = nullptr;
        return;
    }

    // ── 6. Viewport change reactor with debounce ──────────────────────────────
    //  The lambda posts to g_hAcadFrame (captured by value).
    //  It runs on the debounce background thread — no AcGi calls allowed.
    const HWND  hF = g_hAcadFrame;
    const UINT  wm = WM_LIVEMAP_REDRAW;

    g_pReactor = new ViewportReactor(
        [hF, wm]() {
            if (hF) ::PostMessage(hF, wm, 0, 0);
        },
        500u);   // 500 ms debounce

    g_pReactor->attach();

    // ── 7. Register LIVEMAP commands ─────────────────────────────────────────
    acedRegCmds->addCommand(_T("SLA_TOOLS"), _T("SLAMAP_ON"),
        _T("SLAMAP_ON"),     ACRX_CMD_TRANSPARENT, cmdLiveMapOn);
    
    acedRegCmds->addCommand(_T("SLA_TOOLS"), _T("SLAMAP_OFF"),
        _T("SLAMAP_OFF"),    ACRX_CMD_TRANSPARENT, cmdLiveMapOff);
        
    acedRegCmds->addCommand(_T("SLA_TOOLS"), _T("SLAMAP_OPACITY"),
        _T("SLAMAP_OPACITY"), ACRX_CMD_TRANSPARENT, cmdLiveMapOpacity);

    acedRegCmds->addCommand(_T("SLA_TOOLS"), _T("SLA_CPP_IMPORT"),
        _T("SLA_CPP_IMPORT"), ACRX_CMD_TRANSPARENT, SlaImporter::ImportKMZ);
        
    acedRegCmds->addCommand(_T("SLA_TOOLS"), _T("SLA_CPP_EXPORT"),
        _T("SLA_CPP_EXPORT"), ACRX_CMD_TRANSPARENT, SlaExporter::ExportKML);

    // PANGGIL INJEKSI LISP BIAR OTOMATIS JALAN PAS ARX DI LOAD
    InjectSlaLisp();
}

// ─────────────────────────────────────────────────────────────────────────────
//  onUnload  —  reverse of onLoad; order matters
// ─────────────────────────────────────────────────────────────────────────────

static void onUnload()
{
    acutPrintf(_T("\nLiveMap: Unloading..."));

    // 1. Commands first (stop new commands arriving during teardown)
    acedRegCmds->removeGroup(_T("LIVEMAP"));

    // 2. Reactor (stops debounce thread; no more PostMessage after this)
    if (g_pReactor) { g_pReactor->detach(); delete g_pReactor; g_pReactor = nullptr; }

    // 3. Drawable (calls unregisterTransient internally)
    if (g_pDrawable) { delete g_pDrawable; g_pDrawable = nullptr; }

    // 4. Stop downloader (joins worker threads, closes WinHTTP session)
    g_tileDownloader.stop();

    // 5. Release all cached pixel data (~16 MB)
    g_tileCache.clear();

    // 6. Window hook (after reactor so no stray PostMessages arrive)
    removeWindowHook();

    // 7. GDI+ shutdown
    if (g_gdiplusToken) { Gdiplus::GdiplusShutdown(g_gdiplusToken); g_gdiplusToken = 0; }

    acutPrintf(_T("\nLiveMap: Unloaded."));
}

// ─────────────────────────────────────────────────────────────────────────────
//  acrxEntryPoint  —  exported by LiveMapPlugin.def
// ─────────────────────────────────────────────────────────────────────────────

extern "C" AcRx::AppRetCode __declspec(dllexport)
acrxEntryPoint(AcRx::AppMsgCode msg, void* pPkt)
{
    switch (msg)
    {
    case AcRx::kInitAppMsg:
        acrxDynamicLinker->unlockApplication(pPkt);
        acrxDynamicLinker->registerAppMDIAware(pPkt);
        onLoad();
        break;

    // --- TAMBAHIN BLOK INI BROK! ---
    case AcRx::kUnloadAppMsg:
        onUnload(); // Panggil cleaning service pas plugin di-unload
        break;
    // -------------------------------

    case AcRx::kLoadDwgMsg:
        // New drawing opened — re-register transient for its viewports
        if (g_pDrawable)
        {
            g_pDrawable->unregisterTransient();
            g_pDrawable->registerTransient();
        }
        break;

    case AcRx::kUnloadDwgMsg:
        // Drawing closing — unregister to avoid dangling viewport references
        if (g_pDrawable)
            g_pDrawable->unregisterTransient();
        break;

    default:
        break;
    }

    return AcRx::kRetOK;
}