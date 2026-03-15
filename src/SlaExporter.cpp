#include "SlaExporter.h"
#include "SlaMath.h"
#include "pugixml.hpp"
#include <windows.h>
#include <string>
#include <map>
#include <vector>

#include <dbents.h>
#include <dbapserv.h>
#include <acutads.h>
#include <aced.h>
#include <adscodes.h>

namespace SlaExporter {

    // Helper buat narik nama dari XData atau Teks
    std::string getEntityName(AcDbEntity* pEnt) {
        std::string name = "Tanpa_Nama";
        struct resbuf* pRb = pEnt->xData(_T("$SPM-[KML_DATA]"));
        if (pRb) {
            bool foundTag = false;
            for (struct resbuf* rb = pRb; rb != nullptr; rb = rb->rbnext) {
                if (rb->restype == AcDb::kDxfXdAsciiString) {
                    if (foundTag) {
                        std::wstring wStr(rb->resval.rstring);
                        name = std::string(wStr.begin(), wStr.end());
                        break;
                    }
                    if (_tcscmp(rb->resval.rstring, _T("[name]")) == 0) foundTag = true;
                }
            }
            acutRelRb(pRb);
        } else if (pEnt->isKindOf(AcDbText::desc())) {
            AcDbText* pText = AcDbText::cast(pEnt);
            std::wstring wStr(pText->textString());
            name = std::string(wStr.begin(), wStr.end());
        }
        return name;
    }

    void ExportKML() {
        // 1. Baca CRS dari Environment Variable
        wchar_t crsBuf[32] = {0}, zoneBuf[32] = {0}, hemiBuf[32] = {0};
        acedGetEnv(_T("KMZ_CRS_TYPE"), crsBuf, 32);
        acedGetEnv(_T("KMZ_UTM_ZONE"), zoneBuf, 32);
        acedGetEnv(_T("KMZ_HEMISPHERE"), hemiBuf, 32);

        bool use3857 = (_tcscmp(crsBuf, _T("3857")) == 0);
        int zone = _ttoi(zoneBuf);
        if (zone == 0 && !use3857) {
            acutPrintf(_T("\nError: UTM Zone belum diset. Jalanin SLAIMPORT dulu buat nentuin Zona."));
            return;
        }
        bool isSouth = (_tcscmp(hemiBuf, _T("N")) != 0);

        // 2. Minta milih objek
        ads_name ss;
        acutPrintf(_T("\nPilih objek (Point, Block, Line, Polygon, Text) yang mau di-export: "));
        if (acedSSGet(nullptr, nullptr, nullptr, nullptr, ss) != RTNORM) return;

        // 3. Minta milih lokasi save file
        struct resbuf* rbResult = acutNewRb(RTSTR);
        if (acedGetFileD(_T("Save File KML Export"), _T("Export_FTTH.kml"), _T("kml"), 1, rbResult) != RTNORM) {
            acutRelRb(rbResult);
            acedSSFree(ss);
            return;
        }
        std::wstring wFilePath = rbResult->resval.rstring;
        acutRelRb(rbResult);
        std::string filePath(wFilePath.begin(), wFilePath.end());

        // 4. Bikin Dokumen XML/KML pakai PugiXML
        pugi::xml_document doc;
        pugi::xml_node decl = doc.append_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";

        pugi::xml_node kmlNode = doc.append_child("kml");
        kmlNode.append_attribute("xmlns") = "http://www.opengis.net/kml/2.2";
        pugi::xml_node docNode = kmlNode.append_child("Document");
        docNode.append_child("name").text().set((use3857 ? "Export_3857" : "Export_UTM"));

        // Struktur untuk grouping by Layer
        std::map<std::string, pugi::xml_node> layerFolders;

        // FIX ERROR TYPE: Pake Adesk::Int32 biar kasta memorinya cocok sama AutoCAD 64-bit
        Adesk::Int32 length = 0;
        acedSSLength(ss, &length);
        int exportCount = 0;

        for (Adesk::Int32 i = 0; i < length; i++) {
            ads_name entName;
            if (acedSSName(ss, i, entName) != RTNORM) continue;

            AcDbObjectId entId;
            if (acdbGetObjectId(entId, entName) != Acad::eOk) continue;

            AcDbEntity* pEnt;
            if (acdbOpenAcDbEntity(pEnt, entId, AcDb::kForRead) != Acad::eOk) continue;

            std::wstring wLayer(pEnt->layer());
            std::string layerName(wLayer.begin(), wLayer.end());
            std::string objName = getEntityName(pEnt);

            // Tentukan tipe geometri
            std::string geomType = "";
            bool isClosed = false;

            if (pEnt->isKindOf(AcDbPoint::desc()) || pEnt->isKindOf(AcDbBlockReference::desc()) || pEnt->isKindOf(AcDbText::desc())) {
                geomType = "Point";
            } else if (pEnt->isKindOf(AcDbPolyline::desc())) {
                AcDbPolyline* pPoly = AcDbPolyline::cast(pEnt);
                isClosed = pPoly->isClosed();

                // --- TAMBAHAN BARU: AUTO-CLOSE POLYGON (Toleransi 1 Meter) ---
                // Syarat: Garisnya belum status Closed, dan minimal punya 3 titik (biar bisa jadi area)
                if (!isClosed && pPoly->numVerts() >= 3) {
                    AcGePoint3d ptStart, ptEnd;
                    pPoly->getPointAt(0, ptStart);
                    pPoly->getPointAt(pPoly->numVerts() - 1, ptEnd);
                    
                    // Hitung jarak ujung ke ujung. Kalau <= 1.0 unit (1 meter), anggap aja nutup!
                    if (ptStart.distanceTo(ptEnd) <= 1.0) {
                        isClosed = true;
                    }
                }
                // -------------------------------------------------------------

                geomType = isClosed ? "Polygon" : "LineString";
            } else {
                pEnt->close();
                continue; // Skip objek yang ga disupport
            }

            // A. Bikin / Cari Folder Layer
            if (layerFolders.find(layerName) == layerFolders.end()) {
                pugi::xml_node f = docNode.append_child("Folder");
                f.append_child("name").text().set(layerName.c_str());
                layerFolders[layerName] = f;
            }
            pugi::xml_node layerFolder = layerFolders[layerName];

            // B. Bikin / Cari Sub-Folder Tipe Geometri (Points, LineStrings, Polygons)
            std::string subFolderName = geomType + "s";
            pugi::xml_node geomFolder;
            bool folderFound = false;
            for (pugi::xml_node child = layerFolder.child("Folder"); child; child = child.next_sibling("Folder")) {
                if (std::string(child.child_value("name")) == subFolderName) {
                    geomFolder = child;
                    folderFound = true;
                    break;
                }
            }
            if (!folderFound) {
                geomFolder = layerFolder.append_child("Folder");
                geomFolder.append_child("name").text().set(subFolderName.c_str());
            }

            // C. Bikin Placemark di dalam Sub-Folder
            pugi::xml_node placemark = geomFolder.append_child("Placemark");
            placemark.append_child("name").text().set(objName.c_str());

            // D. Tambahin Style KML persis kayak LISP lama
            pugi::xml_node styleNode = placemark.append_child("Style");
            if (geomType == "Point") {
                styleNode.append_child("IconStyle").append_child("Icon"); // Icon Kosong
                styleNode.append_child("LabelStyle").append_child("color").text().set("FF0000FF"); // Label Merah
            } else if (geomType == "LineString") {
                pugi::xml_node ls = styleNode.append_child("LineStyle");
                ls.append_child("color").text().set("FF0000FF"); // Garis Merah
                ls.append_child("width").text().set("2");
            } else if (geomType == "Polygon") {
                pugi::xml_node ls = styleNode.append_child("LineStyle");
                ls.append_child("color").text().set("FF0000FF");
                ls.append_child("width").text().set("2");
                styleNode.append_child("PolyStyle").append_child("color").text().set("B30000FF"); // Fill Merah Transparan
            }

            // E. Proses Koordinat
            double lat, lon;
            if (geomType == "Point") {
                AcGePoint3d pt;
                if (pEnt->isKindOf(AcDbPoint::desc())) pt = AcDbPoint::cast(pEnt)->position();
                else if (pEnt->isKindOf(AcDbBlockReference::desc())) pt = AcDbBlockReference::cast(pEnt)->position();
                else pt = AcDbText::cast(pEnt)->position();

                SlaMath::cad_to_wgs84(pt.x, pt.y, zone, isSouth, use3857, lat, lon);
                char coordStr[128];
                sprintf_s(coordStr, "%.8f,%.8f,0", lon, lat);
                pugi::xml_node geom = placemark.append_child("Point");
                geom.append_child("altitudeMode").text().set("clampToGround");
                geom.append_child("coordinates").text().set(coordStr);
                exportCount++;
            } 
            else {
                AcDbPolyline* pPoly = AcDbPolyline::cast(pEnt);
                pugi::xml_node geom;
                pugi::xml_node coordsNode;
                
                if (isClosed) {
                    geom = placemark.append_child("Polygon");
                    geom.append_child("altitudeMode").text().set("clampToGround");
                    coordsNode = geom.append_child("outerBoundaryIs").append_child("LinearRing").append_child("coordinates");
                } else {
                    geom = placemark.append_child("LineString");
                    geom.append_child("altitudeMode").text().set("clampToGround");
                    coordsNode = geom.append_child("coordinates");
                }

                std::string coordsStr = "";
                AcGePoint3d pt, firstPt;
                for (unsigned int v = 0; v < pPoly->numVerts(); v++) {
                    pPoly->getPointAt(v, pt);
                    if (v == 0) firstPt = pt;
                    SlaMath::cad_to_wgs84(pt.x, pt.y, zone, isSouth, use3857, lat, lon);
                    char buf[128];
                    sprintf_s(buf, "%.8f,%.8f,0 ", lon, lat);
                    coordsStr += buf;
                }
                
                // Kunci KML Polygon: Titik akhir harus = titik awal
                if (isClosed && pPoly->numVerts() > 0) {
                    SlaMath::cad_to_wgs84(firstPt.x, firstPt.y, zone, isSouth, use3857, lat, lon);
                    char buf[128];
                    sprintf_s(buf, "%.8f,%.8f,0 ", lon, lat);
                    coordsStr += buf;
                }

                coordsNode.text().set(coordsStr.c_str());
                exportCount++;
            }
            pEnt->close();
        }
        acedSSFree(ss);

        doc.save_file(filePath.c_str());
        acutPrintf(_T("\nBOOM!! %d Objek berhasil di-export jadi KML dengan struktur rapi pakai kecepatan C++!"), exportCount);
    }
}