# SPATIALLOOKALIKE (SLA) MASTER ENGINE
**High-Performance AutoCAD FTTH & Spatial Data Plugin**

The SLA Master Engine is a pure C++ (ObjectARX) based AutoCAD plugin integrated with dynamic LISP interface injection. This plugin is specifically designed to accelerate the workflow of Drafters in Fiber To The Home (FTTH) architecture design. The SLA Master Engine serves as a lightweight, blazing-fast, and highly accurate solution for processing spatial files (KML/KMZ) and rendering Live Map Backgrounds directly within the AutoCAD canvas.

## 📥 Download Latest Release
Choose the appropriate engine for your operating system:

[![Download ARX](https://img.shields.io/badge/Download-Windows_.arx-blue?style=for-the-badge&logo=windows)](https://github.com/naradevane/SpatialLookAlike/releases/latest)
[![Download LISP](https://img.shields.io/badge/Download-Mac/Legacy_.lsp-lightgrey?style=for-the-badge&logo=apple)](https://github.com/naradevane/SpatialLookAlike/releases/latest)

*(Clicking the buttons above will take you to the latest Release page where you can download the binaries and scripts).*

---

## **Key Features & Mechanics**

This plugin is divided into three primary core engines:

### **1. Live Map Background (`SLAMAP_ON`, `SLAMAP_OFF`, `SLAMAP_OPACITY`)**
* **Mechanism:** The C++ engine runs in the background, monitoring the AutoCAD camera Viewport movements in real-time. During panning or zooming, the system calculates the screen's bounding box coordinates, converts them using a high-precision 64-bit Web Mercator algorithm, and dynamically downloads raster tiles from the Google Maps API.
* **Performance:** Highly memory-efficient. Images are only loaded and rendered for the specific area currently being viewed by the user.

### **2. High-Speed Import Engine (`SLAIMPORT`)**
* **Mechanism:** Utilizes the **PugiXML** library for instant XML structure parsing. The `.kmz` file extraction is handled automatically via the native Windows `tar` command-line utility. This engine is capable of injecting tens of thousands of coordinate points into the AutoCAD Database memory in mere milliseconds.
* **KML to CAD Geometry Mapping:**
  * `<Point>` is converted into an AutoCAD `Point` or `BlockReference`, with an option to generate Middle Center formatted text labels.
  * `<LineString>` is converted into an AutoCAD Open `Polyline`.
  * `<Polygon>` is converted into an AutoCAD Closed `Polyline`.

### **3. Smart Export Engine (`SLAEXPORT`)**
* **Mechanism:** Exports CAD entities (Point, Block, Polyline, Text) back into the standard KML format. The system automatically groups the folder structure based on the user's AutoCAD Layers (e.g., `Layer -> Points / LineStrings / Polygons`).
* **Auto-Close Polygon Tolerance:** The export engine features a smart gap-tolerance algorithm. If a drafter draws an Open Polyline but the distance between the start and end points is **<= 1.0 CAD unit (1 meter)**, the engine will automatically stitch the gap and export it as a KML `<Polygon>` (instead of a `<LineString>`), complete with transparent color fill attributes.

---

## **Legacy Engine (LISP Core)**

In addition to the C++ binary, this repository also includes the original pure LISP source code located in the `legacy_engine` folder. This is provided for:
1. **macOS Users:** Since the macOS environment does not support `.arx` binaries, Mac users can load the `SLA_Master_Mac.lsp` file to utilize the core functionalities.
2. **Older Windows AutoCAD Versions:** If the `.arx` file fails to load on older AutoCAD systems, `SLA_Master_Win.lsp` serves as a reliable fallback.
3. **Educational & Reference Purposes:** Developers are welcome to explore these `.lsp` files to understand the foundational mathematics and logic behind the SpatialLookAlike engine before it was ported to C++.

---

## **Known Limitations**

1. **Internet Connection:** The Live Map feature requires an active internet connection to fetch map tiles.
2. **Operating System:** The C++ `.arx` binary is designed specifically for Windows 64-bit architectures due to its reliance on Windows-specific libraries and command-line utilities.
3. **Default CRS (Coordinate Reference System):** If the drawing lacks Spatial Manager metadata, the fallback default is set to **UTM Zone 49 Hemisphere South (S)** (covering the Indonesian region).

---

## **Regional Adaptation Guide (For Users Outside Indonesia)**

The **Web Mercator (EPSG:3857)** system in this plugin is global and can be used anywhere in the world seamlessly. However, if users work with a **Local UTM** coordinate system outside Indonesia, parameter adjustments are strictly required to prevent coordinate offsets.

**Example Scenario: A Project in the UK**
The UK generally uses UTM Zone **30** and is located in the **North Hemisphere**. If a UK user imports data using the default settings (Zone 49S), the coordinates will be misplaced by thousands of kilometers.

**Solution (Via User Interface):**
1. Type the `SLAIMPORT` command in AutoCAD.
2. Click the **CRS Settings** button.
3. Select the **UTM (Local)** option.
4. In the **UTM Zone** field, input `30`.
5. Under the Hemisphere options, select **North (N)**.
6. Click *OK* and proceed with the Import/Export process. The system will lock these settings in the user's local Windows Environment variables.

---

## **Installation & Usage**

The Windows C++ plugin utilizes a *1-File Magic* (LISP Injection) method, eliminating the need to load multiple files manually.

1. Ensure you are running a 64-bit version of AutoCAD (2021 or newer is recommended).
2. Type the `APPLOAD` command in AutoCAD.
3. Locate and load the **`LiveMapPlugin.arx`** file.
4. *(Optional)* Add this file to your **Startup Suite** to run it automatically every time AutoCAD launches.
5. Type the main command `SLAIMPORT` to launch the graphical user interface.

---

## **Contributing & Forking**

This project is entirely *Open Source*. You are highly encouraged to explore, fork, and modify the source code to fit your project requirements. 

If you add awesome new features, fix bugs, or improve the engine's performance, I would greatly appreciate it if you could submit a **Pull Request (PR)** to this repository. Let's build and improve these tools together for the FTTH Drafter community!

---

## **License**

The SLA Master Engine is distributed under the **GNU General Public License v3.0 (GPLv3)**.
This means:
* You are **FREE** to use, copy, and modify this software.
* If you redistribute this software or its modified versions, you **MUST** make your source code publicly available under the same license (GPLv3), you cannot commercialize it as closed-source, and you must credit the original author.

---

## **Legal Disclaimer**

This plugin is an independent, third-party software and is **NOT** affiliated with, endorsed, sponsored, or approved by Autodesk (AutoCAD) or Google (Google Maps). 

The Live Map Background feature fetches raster tile data purely for educational purposes, demonstrations, and internal visual referencing. All risks associated with its use, including but not limited to IP access blocking by the Map Provider due to API Terms of Service (ToS) violations, are **solely the responsibility of the End-User**. The Author shall not be held liable for any legal or technical damages arising from the use of this plugin.

---

## 📬 Connect with Me

I am a passionate FTTH Drafter and Developer focusing on spatial data automation and web-based engineering solutions. Let's connect!

* **Portfolio & Projects:** [naradevane.vercel.app](https://naradevane.vercel.app/)
* **LinkedIn:** [Ikhsanudin (naradevane)](https://www.linkedin.com/in/naradevane/)
* **Email:** [naradevane@gmail.com](mailto:naradevane@gmail.com)