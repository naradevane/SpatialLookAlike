# SPATIALLOOKALIKE (SLA) MASTER ENGINE
**High-Performance AutoCAD FTTH & Spatial Data Plugin**

SLA Master Engine adalah plugin AutoCAD berbasis C++ (ObjectARX) murni dengan injeksi antarmuka LISP terintegrasi. Plugin ini dirancang secara khusus untuk mempercepat alur kerja *Drafter* dalam desain arsitektur Fiber To The Home (FTTH). SLA Master Engine menjadi solusi yang ringan, sangat cepat, dan akurat untuk memproses file spasial (KML/KMZ) serta memuat *Live Map Background* langsung di kanvas AutoCAD.

---

## **Fitur Utama & Cara Kerja**

Plugin ini terbagi menjadi tiga *core engine* utama:

### **1. Live Map Background (`SLAMAP_ON`, `SLAMAP_OFF`, `SLAMAP_OPACITY`)**
* **Mekanisme:** Mesin C++ bekerja di latar belakang memantau pergerakan *Viewport* kamera AutoCAD secara *real-time*. Saat layar digeser (*panning*), sistem mengkalkulasi koordinat *bounding box* layar, mengonversinya menggunakan algoritma *Web Mercator* presisi 64-bit, dan mengunduh *raster tiles* dari Google Maps API secara dinamis.
* **Performa:** Sangat hemat memori. Gambar hanya dimuat pada area yang sedang dilihat pengguna.

### **2. High-Speed Import Engine (`SLAIMPORT`)**
* **Mekanisme:** Menggunakan pustaka **PugiXML** untuk mengurai struktur XML secara instan. Ekstraksi file `.kmz` dilakukan otomatis melalui utilitas `tar` bawaan CMD Windows. Mesin ini mampu menginjeksi puluhan ribu titik kordinat ke memori *Database* AutoCAD dalam hitungan milidetik.
* **Pemetaan Geometri KML ke CAD:**
  * `<Point>` dikonversi menjadi AutoCAD `Point` atau `BlockReference` dengan opsi penambahan teks label berformat *Middle Center*.
  * `<LineString>` dikonversi menjadi AutoCAD Open `Polyline`.
  * `<Polygon>` dikonversi menjadi AutoCAD Closed `Polyline`.

### **3. Smart Export Engine (`SLAEXPORT`)**
* **Mekanisme:** Mengekspor objek gambar (*Point, Block, Polyline, Text*) kembali ke dalam format standar KML. Sistem secara otomatis melakukan *grouping* (pengelompokan) struktur *folder* berdasarkan Layer AutoCAD pengguna (Contoh: `Layer -> Points / LineStrings / Polygons`).
* **Auto-Close Polygon Tolerance:** Mesin ekspor dilengkapi algoritma toleransi *gap*. Jika *drafter* menggambar garis terbuka (Open Polyline) namun jarak antara titik awal dan akhirnya **<= 1.0 meter (1 unit CAD)**, mesin akan menjahit celah tersebut dan otomatis mengekspornya sebagai `<Polygon>` (bukan `<LineString>`), lengkap dengan atribut *fill* warna transparan.

---

## **Keterbatasan Sistem (Known Limitations)**

1. **Koneksi Internet:** Fitur Live Map membutuhkan koneksi internet aktif untuk memuat peta.
2. **Sistem Operasi:** Didesain khusus untuk arsitektur Windows 64-bit. Tidak mendukung versi AutoCAD for Mac karena ketergantungan pada *library* Windows dan *command line utility*.
3. **Default CRS (Coordinate Reference System):** *Fallback default* apabila gambar tidak memiliki metadata *Spatial Manager* adalah **UTM Zone 49 Hemisphere South (S)** (Sistem proyeksi yang mencakup wilayah Indonesia).

---

## **Panduan Adaptasi Regional (Pengguna di Luar Indonesia)**

Sistem **Web Mercator (EPSG:3857)** pada plugin ini bersifat global dan dapat digunakan di negara manapun tanpa masalah. Namun, jika pengguna bekerja dengan sistem koordinat **UTM (Lokal)** di luar wilayah Indonesia, penyesuaian parameter wajib dilakukan agar posisi gambar tidak meleset.

**Contoh Kasus: Proyek di Inggris (UK)**
Wilayah Inggris secara umum menggunakan UTM Zone **30** dan berada di utara khatulistiwa (**North Hemisphere**). Jika pengguna di Inggris melakukan *import* menggunakan *default setting* (Zone 49S), koordinat akan melenceng sejauh ribuan kilometer.

**Solusi (Melalui User Interface):**
1. Ketik perintah `SLAIMPORT` di *command line* AutoCAD.
2. Klik tombol **CRS Settings**.
3. Pilih opsi **UTM (Lokal)**.
4. Pada kolom **UTM Zone**, masukkan angka `30`.
5. Pada pilihan *Hemisphere*, ubah menjadi **North (N)**.
6. Klik *OK* dan lanjutkan proses *Import/Export*. Sistem akan mengunci pengaturan ini di memori *Environment* lokal pengguna.

---

## **Instalasi & Penggunaan**

Plugin ini menggunakan metode *1-File Magic* (LISP Injection), sehingga pengguna tidak perlu memuat banyak file secara terpisah.

1. Pastikan Anda menjalankan AutoCAD 64-bit (Direkomendasikan versi 2021 ke atas).
2. Ketik perintah `APPLOAD` pada AutoCAD.
3. Cari dan *Load* file **`LiveMapPlugin.arx`**.
4. *(Opsional)* Masukkan file tersebut ke dalam **Startup Suite** agar otomatis berjalan setiap AutoCAD dibuka.
5. Ketik perintah utama `SLAIMPORT` untuk mulai menggunakan antarmuka grafis.

## Contributing & Forking

Proyek ini bersifat *Open Source*. Anda sangat dipersilakan untuk mempelajari, mengubah, dan memodifikasi *source code* sesuai kebutuhan proyek Anda. 

Jika Anda menambahkan fitur baru yang keren, memperbaiki *bug*, atau meningkatkan performa *engine* ini, saya akan sangat menghargainya jika Anda bersedia mengirimkan **Pull Request (PR)** ke repositori ini. Mari kita kembangkan *tools* ini bersama-sama untuk komunitas *Drafter* FTTH!

## License

SLA Master Engine didistribusikan di bawah lisensi **GNU General Public License v3.0 (GPLv3)**.
Artinya:
* Anda **BEBAS** menggunakan, menyalin, dan memodifikasi *software* ini.
* Jika Anda mendistribusikan ulang (membagikan) *software* ini atau versi modifikasinya, Anda **WAJIB** membuka *source code* Anda ke publik dengan lisensi yang sama (GPLv3), tidak boleh dikomersilkan secara tertutup, dan wajib mencantumkan nama kreator asli.

## Legal Disclaimer (Pernyataan Lepas Tanggung Jawab)

*Plugin* ini adalah perangkat lunak independen (*third-party*) dan **TIDAK** berafiliasi, didukung, disponsori, atau disetujui oleh Autodesk (AutoCAD) maupun Google (Google Maps). 

Fitur *Live Map Background* pada *plugin* ini memuat data *raster tile* murni untuk tujuan edukasi, demonstrasi, dan visualisasi referensi internal. Segala bentuk risiko penggunaan, termasuk namun tidak terbatas pada pemblokiran akses IP oleh penyedia layanan peta (Map Provider) akibat pelanggaran *Terms of Service* (ToS) penggunaan API, adalah **sepenuhnya tanggung jawab pengguna (*End-User*)**. Pengembang (Author) tidak bertanggung jawab atas kerugian hukum atau teknis apa pun yang timbul dari penggunaan *plugin* ini.

---
**Author:** Ikhsanudin (naradevane) | *Drafter FTTH*