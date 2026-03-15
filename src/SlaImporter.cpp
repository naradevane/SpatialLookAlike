#include "SlaImporter.h"
#include "SlaMath.h"
#include "pugixml.hpp"
#include <windows.h>
#include <vector>
#include <sstream>

// Header ARX untuk nulis objek ke CAD
#include <dbents.h>
#include <dbapserv.h>
#include <acutads.h>
#include <aced.h>
#include <adscodes.h>

namespace SlaImporter { // <--- INI KURUNG BUKA YANG ERROR TADI

    // Helper misahin koordinat string jadi angka
    std::vector<AcGePoint2d> parseCoordinates(const std::string& text, int zone, bool is_south, bool use_3857) {
        std::vector<AcGePoint2d> pts;
        std::stringstream ss(text);
        std::string token;
        
        while (std::getline(ss, token, ' ')) {
            if (token.empty() || token == "\n" || token == "\r" || token == "\t") continue;
            
            size_t c1 = token.find(',');
            if (c1 != std::string::npos) {
                double lon = std::stod(token.substr(0, c1));
                size_t c2 = token.find(',', c1 + 1);
                double lat = std::stod(token.substr(c1 + 1, c2 - c1 - 1));
                
                double x = 0, y = 0;
                SlaMath::wgs84_to_cad(lat, lon, zone, is_south, use_3857, x, y);
                pts.emplace_back(x, y);
            }
        }
        return pts;
    }

    // Fungsi inject XData Spasial
    void attachXData(AcDbEntity* pEnt, const wchar_t* name) {
        if (!acdbRegApp(_T("$SPM-[KML_DATA]"))) return;

        struct resbuf* pRb = acutBuildList(
            AcDb::kDxfRegAppName, _T("$SPM-[KML_DATA]"),
            AcDb::kDxfXdControlString, _T("{"),
            AcDb::kDxfXdAsciiString, _T("[name]"),
            AcDb::kDxfXdAsciiString, name,
            AcDb::kDxfXdControlString, _T("}"),
            0);

        if (pRb) {
            pEnt->setXData(pRb);
            acutRelRb(pRb);
        }
    }

    void ImportKMZ() {
        // 1. Baca semua settingan dari LISP via Environment Variable
        wchar_t pathBuf[512] = {0};
        acedGetEnv(_T("KMZ_FILE_PATH"), pathBuf, 512);
        std::wstring wFilePath = pathBuf;
        if (wFilePath.empty()) return;

        wchar_t crsBuf[32] = {0};
        acedGetEnv(_T("KMZ_CRS_TYPE"), crsBuf, 32);
        bool use3857 = (_tcscmp(crsBuf, _T("3857")) == 0);

        wchar_t zoneBuf[32] = {0};
        acedGetEnv(_T("KMZ_UTM_ZONE"), zoneBuf, 32);
        int zone = _ttoi(zoneBuf);
        if (zone == 0) zone = 49; // Fallback aman

        wchar_t hemiBuf[32] = {0};
        acedGetEnv(_T("KMZ_HEMISPHERE"), hemiBuf, 32);
        bool isSouth = (_tcscmp(hemiBuf, _T("N")) != 0);

        wchar_t typeBuf[32] = {0};
        acedGetEnv(_T("KMZ_IMPORT_TYPE"), typeBuf, 32);
        bool useBlock = (_tcscmp(typeBuf, _T("BLOCK")) == 0);

        wchar_t blockBuf[128] = {0};
        acedGetEnv(_T("KMZ_BLOCK_NAME"), blockBuf, 128);
        std::wstring wBlockName = blockBuf;

        wchar_t labelBuf[32] = {0};
        acedGetEnv(_T("KMZ_USE_LABEL"), labelBuf, 32);
        bool useLabel = (_tcscmp(labelBuf, _T("1")) == 0);

        // 2. Ekstrak KMZ kalau file-nya .kmz
        std::wstring wExt = wFilePath.substr(wFilePath.find_last_of(L".") + 1);
        std::string kmlPath;
        std::wstring tempDir = wFilePath.substr(0, wFilePath.find_last_of(L"\\")) + L"\\_temp_kmz";

        if (wExt == L"kmz" || wExt == L"KMZ") {
            _wmkdir(tempDir.c_str());
            std::wstring cmd = L"cmd.exe /c tar -xf \"" + wFilePath + L"\" -C \"" + tempDir + L"\"";
            _wsystem(cmd.c_str());
            kmlPath = std::string(tempDir.begin(), tempDir.end()) + "\\doc.kml";
        } else {
            kmlPath = std::string(wFilePath.begin(), wFilePath.end());
        }

        // 3. Baca KML pakai PugiXML
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(kmlPath.c_str());
        if (!result) { acutPrintf(_T("\nError: Gagal ngebaca file KML brok!")); return; }

        // 4. Siapin Database DWG
        AcDbBlockTable* pBlockTable;
        acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead);
        
        AcDbBlockTableRecord* pSpace;
        pBlockTable->getAt(ACDB_MODEL_SPACE, pSpace, AcDb::kForWrite);

        // Cari ID Block kalau user pilih mode "AutoCAD Block"
        AcDbObjectId blockId = AcDbObjectId::kNull;
        if (useBlock && !wBlockName.empty() && wBlockName != L"NONE") {
            if (pBlockTable->has(wBlockName.c_str())) {
                pBlockTable->getAt(wBlockName.c_str(), blockId);
            } else {
                acutPrintf(_T("\nWarning: Block '%s' ga ketemu di drawing, otomatis ganti ke Point!"), wBlockName.c_str());
            }
        }
        pBlockTable->close();

        int objCount = 0;

        // 5. Looping data spasial
        auto placemarks = doc.select_nodes("//Placemark");
        for (pugi::xpath_node node : placemarks) {
            pugi::xml_node pm = node.node();
            std::string name = pm.child("name").text().as_string();
            std::wstring wName(name.begin(), name.end());

            if (pm.child("Point")) {
                std::string coords = pm.child("Point").child("coordinates").text().as_string();
                auto pts = parseCoordinates(coords, zone, isSouth, use3857);
                if (!pts.empty()) {
                    AcDbEntity* pEnt = nullptr;

                    // Gambar sebagai BLOCK atau POINT
                    if (useBlock && blockId != AcDbObjectId::kNull) {
                        AcDbBlockReference* pBlkRef = new AcDbBlockReference();
                        pBlkRef->setPosition(AcGePoint3d(pts[0].x, pts[0].y, 0.0));
                        pBlkRef->setBlockTableRecord(blockId);
                        pEnt = pBlkRef;
                    } else {
                        pEnt = new AcDbPoint(AcGePoint3d(pts[0].x, pts[0].y, 0.0));
                    }
                    
                    attachXData(pEnt, wName.c_str());
                    AcDbObjectId entId;
                    pSpace->appendAcDbEntity(entId, pEnt);
                    pEnt->close();
                    objCount++;

                    // Gambar TEKS LABEL
                    if (useLabel && !wName.empty()) {
                        AcDbText* pText = new AcDbText();
                        pText->setTextString(wName.c_str());
                        pText->setHeight(2.0); // Tinggi teks
                        
                        // --- TAMBAHAN BARU: MIDDLE CENTER JUSTIFY ---
                        pText->setHorizontalMode(AcDb::kTextCenter);
                        // INI YANG BENER: Pake kTextVertMid (bukan Middle)
                        pText->setVerticalMode(AcDb::kTextVertMid); 
                        // KUNCI: Wajib pakai setAlignmentPoint kalau justify bukan di kiri-bawah
                        pText->setAlignmentPoint(AcGePoint3d(pts[0].x, pts[0].y, 0.0));
                        // --------------------------------------------
                        
                        AcDbObjectId txtId;
                        pSpace->appendAcDbEntity(txtId, pText);
                        pText->close();
                    }
                }
            }
            else if (pm.child("LineString") || pm.child("Polygon")) {
                std::string coords = pm.child("LineString") ? 
                    pm.child("LineString").child("coordinates").text().as_string() : 
                    pm.child("Polygon").child("outerBoundaryIs").child("LinearRing").child("coordinates").text().as_string();

                auto pts = parseCoordinates(coords, zone, isSouth, use3857);
                if (pts.size() > 1) {
                    AcDbPolyline* pPoly = new AcDbPolyline(static_cast<unsigned int>(pts.size()));
                    for (unsigned int i = 0; i < static_cast<unsigned int>(pts.size()); i++) {
                        pPoly->addVertexAt(i, pts[i]);
                    }
                    if (pm.child("Polygon")) pPoly->setClosed(true);

                    attachXData(pPoly, wName.c_str());
                    AcDbObjectId entId;
                    pSpace->appendAcDbEntity(entId, pPoly);
                    pPoly->close();
                    objCount++;
                }
            }
        }

        pSpace->close();

        // 6. Cleanup
        if (wExt == L"kmz" || wExt == L"KMZ") {
            std::wstring cmd = L"cmd.exe /c rmdir /q /s \"" + tempDir + L"\"";
            _wsystem(cmd.c_str());
        }

        acutPrintf(_T("\nBOOM!! %d Objek FTTH (Web Mercator/UTM) berhasil ditarik masuk pake kecepatan dewa C++!"), objCount);
    }

} // <--- NAH INI DIA SI KURUNG TUTUP YANG HILANG TADI WKWK