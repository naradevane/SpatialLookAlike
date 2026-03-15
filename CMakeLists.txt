cmake_minimum_required(VERSION 3.20)
project(LiveMapPlugin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find AutoCAD install and ARX SDK ─────────────────────────────────────────

if(NOT DEFINED ACAD_INSTALL_DIR)
    set(ACAD_INSTALL_DIR "C:/Program Files/Autodesk/AutoCAD 2024")
endif()
if(NOT DEFINED ARX_SDK_DIR)
    set(ARX_SDK_DIR "C:/ARX_SDK_2024")
endif()

set(ACAD_INC "${ARX_SDK_DIR}/inc")
set(ACAD_LIB "${ARX_SDK_DIR}/lib-x64")

if(NOT EXISTS "${ACAD_INC}/acdb.h")
    message(FATAL_ERROR
        "ARX SDK not found at ${ARX_SDK_DIR}.\n"
        "Set -DARX_SDK_DIR=<path> to your ObjectARX SDK root.\n"
        "Download from: https://www.autodesk.com/developer-network/platform-technologies/autocad")
endif()

# ── Source files ──────────────────────────────────────────────────────────────

# --- TAMBAHIN INI DI BAGIAN SOURCES ---
set(SOURCES
    src/acrxEntryPoint.cpp
    src/MapDrawable.cpp
    src/ViewportReactor.cpp
    src/SlaMath.cpp
    src/SlaImporter.cpp
    src/SlaExporter.cpp
    vendor/pugixml/pugixml.cpp  # Masukin parser XML-nya ke build
)


# --- TAMBAHIN INI DI BAGIAN HEADERS ---
set(HEADERS
    include/LiveMapCommon.h
    include/RasterImageWrapper.h
    include/TileManager.h
    include/MapDrawable.h
    include/ViewportReactor.h
    include/SlaMath.h
    include/SlaImporter.h
    include/SlaExporter.h
    vendor/pugixml/pugixml.hpp
)

# ── DLL target (INI HARUS DULUAN) ──────────────────────────────────────────────
add_library(LiveMapPlugin SHARED ${SOURCES} ${HEADERS})

# Change output extension from .dll to .arx
set_target_properties(LiveMapPlugin PROPERTIES
    OUTPUT_NAME   "LiveMapPlugin"
    SUFFIX        ".arx"
    PREFIX        ""
)

# Module definition file (exports acrxEntryPoint and acrxGetApiVersion)
set_target_properties(LiveMapPlugin PROPERTIES
    LINK_FLAGS "/DEF:\"${CMAKE_CURRENT_SOURCE_DIR}/LiveMapPlugin.def\""
)

# ── Include directories (BARU TARUH INI DI SINI) ──────────────────────────────
target_include_directories(LiveMapPlugin PRIVATE
    include/
    vendor/pugixml/   # <--- PugiXML masuk sini
    "${ACAD_INC}"
    "${ACAD_INC}/gs"
    "${ACAD_INC}/gi"
    "${ACAD_INC}/grx"
)

# ── Linker directories ────────────────────────────────────────────────────────
target_link_directories(LiveMapPlugin PRIVATE "${ACAD_LIB}")

# ── Preprocessor definitions ──────────────────────────────────────────────────
target_compile_definitions(LiveMapPlugin PRIVATE
    WIN32
    _WINDOWS
    UNICODE
    _UNICODE
    _USRDLL
    LIVEMAP_EXPORTS
    AC_NO_CHECK_FILE_SIZE  # suppress ARX size-check nag
    # NDEBUG is set automatically for Release configs
)

# ── Compiler options (MSVC) ───────────────────────────────────────────────────
if(MSVC)
    target_compile_options(LiveMapPlugin PRIVATE
        /W4          # high warning level
        /WX-         # warnings-as-errors OFF (ARX headers have warnings)
        /wd4100      # unreferenced formal parameter (common in ARX callbacks)
        /wd4127      # conditional expression is constant
        /MD          # MultiThreadedDLL runtime (MUST match AutoCAD)
        /Zc:__cplusplus  # enable correct __cplusplus macro
    )
    # Disable incremental linking (can cause issues with ARX plugins)
    set_target_properties(LiveMapPlugin PROPERTIES
        LINK_FLAGS_DEBUG   "/INCREMENTAL:NO"
        LINK_FLAGS_RELEASE "/INCREMENTAL:NO /OPT:REF /OPT:ICF"
    )
endif()

# ── Windows SDK libraries ─────────────────────────────────────────────────────
target_link_libraries(LiveMapPlugin PRIVATE
    # Windows system (no path needed — resolved by linker automatically)
    gdiplus.lib
    winhttp.lib
    ole32.lib
    kernel32.lib
    user32.lib
)

# ── ObjectARX SDK libraries ───────────────────────────────────────────────────
set(ACAD_VER_SUFFIX "24")

target_link_libraries(LiveMapPlugin PRIVATE
    accore.lib
    acdb${ACAD_VER_SUFFIX}.lib
    acge${ACAD_VER_SUFFIX}.lib
    rxapi.lib
    acad.lib
    ac1st${ACAD_VER_SUFFIX}.lib
)

# ─────────────────────────────────────────────────────────────────────────────
#  Post-build: Copy .arx next to the AutoCAD executable for easy testing
# ─────────────────────────────────────────────────────────────────────────────
add_custom_command(TARGET LiveMapPlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:LiveMapPlugin>"
        "${ACAD_INSTALL_DIR}/LiveMapPlugin.arx"
    COMMENT "Deploying LiveMapPlugin.arx to AutoCAD directory"
)

# ─────────────────────────────────────────────────────────────────────────────
#  IDE source grouping (Visual Studio Solution Explorer)
# ─────────────────────────────────────────────────────────────────────────────
source_group("Headers" FILES ${HEADERS})
source_group("Source"  FILES ${SOURCES})
