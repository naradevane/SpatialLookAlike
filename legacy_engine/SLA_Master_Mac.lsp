(vl-load-com)

;; ==========================================
;; 1. MATEMATIKA DASAR & KOORDINAT ENGINE (DUAL CORE)
;; ==========================================
(defun tan (x) (/ (sin x) (cos x)))
(defun sinh (x) (/ (- (exp x) (exp (- x))) 2.0))

;; --- CORE A: MESIN UTM (ULTRA PRECISION WGS84) ---
(defun wgs84-to-utm (lat lon zone is_south / a eccSq eccPrimeSq k0 N T_val C A_val M x y lonOrigin radLat radLon)
  (setq a 6378137.0 eccSq 0.0066943799901413165 eccPrimeSq (/ eccSq (- 1.0 eccSq)) k0 0.9996)
  (setq radLat (* lat (/ pi 180.0)) radLon (* lon (/ pi 180.0)) lonOrigin (* (+ -183.0 (* zone 6.0)) (/ pi 180.0)))
  (setq N (/ a (sqrt (- 1.0 (* eccSq (sin radLat) (sin radLat))))) T_val (* (tan radLat) (tan radLat)) C (* eccPrimeSq (cos radLat) (cos radLat)) A_val (* (cos radLat) (- radLon lonOrigin)))
  (setq M (* a (+ (* (- 1.0 (/ eccSq 4.0) (/ (* eccSq eccSq) 64.0) (/ (* eccSq eccSq eccSq) 256.0)) radLat) (* (- 0.0 (/ (* 3.0 eccSq) 8.0) (/ (* 3.0 eccSq eccSq) 32.0) (/ (* 45.0 eccSq eccSq eccSq) 1024.0)) (sin (* 2.0 radLat))) (* (+ (/ (* 15.0 eccSq eccSq) 256.0) (/ (* 45.0 eccSq eccSq eccSq) 1024.0)) (sin (* 4.0 radLat))) (* (- 0.0 (/ (* 35.0 eccSq eccSq eccSq) 3072.0)) (sin (* 6.0 radLat))))))
  (setq x (+ (* k0 N A_val (+ 1.0 (* (/ (* A_val A_val) 6.0) (+ 1.0 (- T_val) C)) (* (/ (* A_val A_val A_val A_val) 120.0) (+ 5.0 (* -18.0 T_val) (* T_val T_val) (* 72.0 C) (* -58.0 eccPrimeSq))))) 500000.0))
  (setq y (* k0 (+ M (* N (tan radLat) (+ (* (/ (* A_val A_val) 2.0) 1.0) (* (/ (* A_val A_val A_val A_val) 24.0) (+ 5.0 (- T_val) (* 9.0 C) (* 4.0 C C))) (* (/ (* A_val A_val A_val A_val A_val A_val) 720.0) (+ 61.0 (* -58.0 T_val) (* T_val T_val) (* 600.0 C) (* -330.0 eccPrimeSq))))))))
  (if is_south (setq y (+ y 10000000.0)))
  (list x y 0.0)
)

(defun utm-to-wgs84 (x y zone is_south / a eccSq eccPrimeSq k0 e1 x_adj y_adj M mu phi1 N1 T1 C1 R1 D lat lon rad2deg lonOrigin term1 term2 term3)
  (setq a 6378137.0 eccSq 0.0066943799901413165 k0 0.9996 rad2deg (/ 180.0 pi))
  (setq eccPrimeSq (/ eccSq (- 1.0 eccSq)))
  (setq e1 (/ (- 1.0 (sqrt (- 1.0 eccSq))) (+ 1.0 (sqrt (- 1.0 eccSq)))))
  (setq x_adj (- x 500000.0) y_adj (if is_south (- y 10000000.0) y))
  (setq M (/ y_adj k0))
  (setq mu (/ M (* a (- 1.0 (/ eccSq 4.0) (/ (* eccSq eccSq) 64.0) (/ (* eccSq eccSq eccSq) 256.0)))))
  (setq phi1 (+ mu (* (+ (/ (* 3.0 e1) 2.0) (* -0.84375 e1 e1 e1)) (sin (* 2.0 mu))) (* (+ 1.3125 (* -1.71875 e1 e1 e1 e1)) e1 e1 (sin (* 4.0 mu))) (* 1.57291666667 e1 e1 e1 (sin (* 6.0 mu)))))
  (setq N1 (/ a (sqrt (- 1.0 (* eccSq (sin phi1) (sin phi1))))))
  (setq T1 (* (tan phi1) (tan phi1)))
  (setq C1 (* eccPrimeSq (cos phi1) (cos phi1)))
  (setq R1 (/ (* a (- 1.0 eccSq)) (expt (- 1.0 (* eccSq (sin phi1) (sin phi1))) 1.5)))
  (setq D (/ x_adj (* N1 k0)))
  (setq lat (- phi1 (* (/ (* N1 (tan phi1)) R1) (- (/ (* D D) 2.0) (- (/ (* (* D D D D) (+ 5.0 (* 3.0 T1) (* 10.0 C1) (* -4.0 C1 C1) (* -9.0 eccPrimeSq))) 24.0) (/ (* (* D D D D D D) (+ 61.0 (* 90.0 T1) (* 298.0 C1) (* 45.0 T1 T1) (* -252.0 eccPrimeSq) (* -3.0 C1 C1))) 720.0))))))
  (setq lonOrigin (* (+ -183.0 (* zone 6.0)) (/ pi 180.0)))
  (setq term1 D term2 (/ (* (* D D D) (+ 1.0 (* 2.0 T1) C1)) 6.0) term3 (/ (* (* D D D D D) (+ 5.0 (* -2.0 C1) (* 28.0 T1) (* -3.0 C1 C1) (* 8.0 eccPrimeSq) (* 24.0 T1 T1))) 120.0))
  (setq lon (+ lonOrigin (/ (+ (- term1 term2) term3) (cos phi1))))
  (list (* lon rad2deg) (* lat rad2deg) 0.0)
)

;; --- CORE B: MESIN WEB MERCATOR (EPSG:3857) ---
(defun wgs84-to-webmercator (lat lon / x y)
  (setq x (* lon (/ 20037508.34 180.0)))
  (setq y (* (log (tan (* (+ 90.0 lat) (/ pi 360.0)))) (/ 20037508.34 pi)))
  (list x y 0.0)
)

(defun webmercator-to-wgs84 (x y / lon lat)
  (setq lon (* x (/ 180.0 20037508.34)))
  (setq lat (- (* (atan (exp (* y (/ pi 20037508.34)))) (/ 360.0 pi)) 90.0))
  (list lon lat 0.0)
)

;; --- AUTO-ROUTER MESIN ---
(defun wgs84-to-cad (lat lon zone is_south crs_type)
  (if (= crs_type "3857") (wgs84-to-webmercator lat lon) (wgs84-to-utm lat lon zone is_south))
)
(defun cad-to-wgs84 (x y zone is_south crs_type)
  (if (= crs_type "3857") (webmercator-to-wgs84 x y) (utm-to-wgs84 x y zone is_south))
)

(defun get-grid-convergence-deg (lat lon zone / cm delta_lon rot_deg)
  (setq cm (+ -183.0 (* zone 6.0)) delta_lon (- lon cm))
  (* delta_lon (sin (* lat (/ pi 180.0))))
)

(defun get-osm-tile (lat lon zoom / n rad_lat x_tile y_tile)
  (setq n (expt 2.0 zoom) x_tile (fix (* (/ (+ lon 180.0) 360.0) n)) rad_lat (* lat (/ pi 180.0)))
  (setq y_tile (fix (* (- 1.0 (/ (log (+ (tan rad_lat) (/ 1.0 (cos rad_lat)))) pi)) (/ n 2.0))))
  (list x_tile y_tile)
)
(defun tile-to-wgs84 (x y zoom / n lon lat_rad lat)
  (setq n (expt 2.0 zoom) lon (- (* (/ (float x) n) 360.0) 180.0))
  (setq lat_rad (atan (sinh (* pi (- 1.0 (* 2.0 (/ (float y) n)))))) lat (* lat_rad (/ 180.0 pi)))
  (list lon lat)
)

;; ==========================================
;; 2. SPATIAL MANAGER (SPM) CRACKER ENGINE
;; ==========================================
(defun hex2dec (h / d i c n)
  (setq d 0 i 1 h (strcase h))
  (while (<= i (strlen h))
    (setq c (ascii (substr h i 1)))
    (setq n (if (<= c 57) (- c 48) (- c 55)))
    (setq d (+ (* d 16) n))
    (setq i (1+ i))
  )
  d
)

(defun sync-spm-crs ( / nod dict rec hex_str len val b1 b2 b3 b4 srid zone hemi)
  (setq nod (namedobjdict))
  (setq dict (dictsearch nod "$SPM_Srid"))
  (if dict
    (progn
      (setq rec (assoc 1004 dict))
      (if rec
        (progn
          (setq hex_str (cdr rec) len (strlen hex_str))
          (setq val (substr hex_str (- len 9) 8))
          (setq b1 (hex2dec (substr val 1 2)) b2 (hex2dec (substr val 3 2)))
          (setq b3 (hex2dec (substr val 5 2)) b4 (hex2dec (substr val 7 2)))
          (setq srid (+ b1 (* 256 b2) (* 65536 b3) (* 16777216 b4)))
          (cond
            ((and (>= srid 32701) (<= srid 32760))
              (setq zone (- srid 32700) hemi "S")
              (setenv "KMZ_UTM_ZONE" (itoa zone)) (setenv "KMZ_HEMISPHERE" hemi) (setenv "KMZ_CRS_TYPE" "UTM")
              (princ (strcat "\n[SPM SYNC] Ketarik data Spatial Manager! Mode: UTM Zone " (itoa zone) hemi))
            )
            ((and (>= srid 32601) (<= srid 32660))
              (setq zone (- srid 32600) hemi "N")
              (setenv "KMZ_UTM_ZONE" (itoa zone)) (setenv "KMZ_HEMISPHERE" hemi) (setenv "KMZ_CRS_TYPE" "UTM")
              (princ (strcat "\n[SPM SYNC] Ketarik data Spatial Manager! Mode: UTM Zone " (itoa zone) hemi))
            )
            ((= srid 3857)
              (setenv "KMZ_CRS_TYPE" "3857")
              (princ "\n[SPM SYNC] Ketarik data Spatial Manager! Mode dialihkan ke Web Mercator (EPSG:3857)")
            )
          )
        )
      )
    )
  )
  (princ)
)

;; ==========================================
;; 3. DCL ON-THE-FLY GENERATOR
;; ==========================================
(defun build-temp-dcl ( / tmp_file fp dcl_str )
  (setq tmp_file (vl-filename-mktemp "sla_ui.dcl"))
  (setq fp (open tmp_file "w"))
  (setq dcl_str "kmz_import_dialog : dialog { label = \"SLA Master - Importer (Mac)\"; : boxed_row { label = \"1. Pilih File Data\"; : edit_box { key = \"file_path\"; label = \"Path:\"; width = 50; } : button { key = \"btn_browse\"; label = \"Browse...\"; width = 12; } } : boxed_column { label = \"2. Opsi Import\"; : radio_row { key = \"import_type\"; : radio_button { key = \"opt_point\"; label = \"AutoCAD Point\"; value = \"1\"; } : radio_button { key = \"opt_block\"; label = \"AutoCAD Block\"; } } : popup_list { key = \"block_list\"; label = \"Pilih Block:\"; width = 35; } : toggle { key = \"chk_label\"; label = \"Bikin Teks Label dari tag <name>\"; value = \"1\"; } } : row { : button { key = \"btn_settings\"; label = \"API & CRS Settings\"; width = 25; } : spacer { width = 10; } ok_cancel; } } kmz_settings_dialog : dialog { label = \"API & CRS Settings\"; : boxed_column { label = \"Coordinate Reference System (CRS)\"; : radio_row { key = \"crs_type\"; : radio_button { key = \"opt_utm\"; label = \"UTM (Lokal)\"; } : radio_button { key = \"opt_3857\"; label = \"Web Mercator (Global)\"; } } : edit_box { key = \"set_utm_zone\"; label = \"UTM Zone (Ketik AUTO / Angka):\"; width = 15; } : radio_row { key = \"set_hemisphere\"; : radio_button { key = \"opt_south\"; label = \"South (S)\"; } : radio_button { key = \"opt_north\"; label = \"North (N)\"; } } : text { label = \"*UTM Zone diabaikan jika memilih Web Mercator.\"; } } ok_only; }")
  (write-line dcl_str fp)
  (close fp)
  tmp_file
)

;; ==========================================
;; 4. FUNGSI PEMBANTU I/O DATA
;; ==========================================
(defun get_drawing_blocks ( / blk blk_name blk_list )
  (setq blk (tblnext "BLOCK" T)) (while blk (setq blk_name (cdr (assoc 2 blk))) (if (/= (substr blk_name 1 1) "*") (setq blk_list (append blk_list (list blk_name)))) (setq blk (tblnext "BLOCK"))) blk_list
)
(defun extract_xml_text (str start_tag end_tag / start_pos end_pos)
  (setq start_pos (vl-string-search start_tag str) end_pos (vl-string-search end_tag str)) (if (and start_pos end_pos) (substr str (+ start_pos (strlen start_tag) 1) (- end_pos (+ start_pos (strlen start_tag)))) "")
)
(defun parse-coord-string (str / len i char cur_word word_list)
  (setq len (strlen str) i 1 cur_word "" word_list nil) (while (<= i len) (setq char (substr str i 1)) (if (or (= char " ") (= char "\t") (= char "\n") (= char "\r")) (progn (if (/= cur_word "") (setq word_list (append word_list (list cur_word)))) (setq cur_word "")) (setq cur_word (strcat cur_word char))) (setq i (1+ i))) (if (/= cur_word "") (setq word_list (append word_list (list cur_word)))) word_list
)
(defun get-xy-from-comma-str (cstr / pos1 pos2 x y)
  (setq pos1 (vl-string-search "," cstr)) (if pos1 (progn (setq x (atof (substr cstr 1 pos1)) cstr (substr cstr (+ pos1 2)) pos2 (vl-string-search "," cstr)) (if pos2 (setq y (atof (substr cstr 1 pos2))) (setq y (atof cstr))) (list x y)) nil)
)
(defun inject-xdata (ent name_str / exData oldData newData)
  (if (and ent name_str (/= name_str "")) (progn (if (not (tblsearch "APPID" "$SPM-[KML_DATA]")) (regapp "$SPM-[KML_DATA]")) (setq exData (list -3 (list "$SPM-[KML_DATA]" (cons 1002 "{") (cons 1000 "[name]") (cons 1000 name_str) (cons 1002 "}")))) (setq oldData (entget ent) newData (append oldData (list exData))) (entmod newData)))
)
(defun get-object-name (ent ent_data obj_type / xdata ext_data spm_list name flag text_val)
  (setq name nil xdata (entget ent '("*"))) (if (setq ext_data (assoc -3 xdata)) (progn (setq spm_list (assoc "$SPM-[KML_DATA]" (cdr ext_data))) (if spm_list (progn (setq flag nil) (foreach item (cdr spm_list) (if (and (= (car item) 1000) flag) (progn (setq name (cdr item)) (setq flag nil))) (if (and (= (car item) 1000) (= (cdr item) "[name]")) (setq flag T))))))) (if (not name) (if (or (= obj_type "TEXT") (= obj_type "MTEXT")) (if (setq text_val (assoc 1 ent_data)) (setq name (cdr text_val))))) (if (or (not name) (= name "")) "Tanpa_Nama" name)
)

;; ==========================================
;; 5. COMMAND 1: SLAIMP (IMPORT KMZ/KML - MAC)
;; ==========================================
(defun c:SLAIMP ( / dcl_file dcl_id file_path import_type block_name use_label result my_blocks selected_idx ext target_kml is_temp_file temp_dir normalized_kml *kmz_utm_zone* *kmz_hemisphere* *crs_type* fp line pt_name obj_type in_coords coord_buffer pt_list xy pt is_south utm_zone poly_ent_data last_ent pts_count auto_zone zone_detected char prev_char buf fp_in fp_out retry kml_files)
  (sync-spm-crs)
  
  (setq *kmz_utm_zone* (if (and (getenv "KMZ_UTM_ZONE") (/= (getenv "KMZ_UTM_ZONE") "")) (getenv "KMZ_UTM_ZONE") "AUTO"))
  (setq *kmz_hemisphere* (if (getenv "KMZ_HEMISPHERE") (getenv "KMZ_HEMISPHERE") "S"))
  (setq *crs_type* (if (getenv "KMZ_CRS_TYPE") (getenv "KMZ_CRS_TYPE") "UTM"))
  
  (setq dcl_file (build-temp-dcl))
  (setq dcl_id (load_dialog dcl_file))
  (if (not (new_dialog "kmz_import_dialog" dcl_id)) (progn (alert "Gagal nampilin UI.") (exit)))

  (set_tile "file_path" "") (set_tile "opt_point" "1") (mode_tile "block_list" 1) (set_tile "chk_label" "1")
  (setq my_blocks (get_drawing_blocks))
  (if my_blocks (progn (start_list "block_list") (mapcar 'add_list my_blocks) (end_list) (set_tile "block_list" "0")) (progn (start_list "block_list") (add_list "-- TIDAK ADA BLOCK --") (end_list) (mode_tile "opt_block" 1)))

  (action_tile "opt_point" "(mode_tile \"block_list\" 1)")
  (action_tile "opt_block" "(mode_tile \"block_list\" 0)")
  (action_tile "btn_browse" "(setq file_path (getfiled \"Pilih File KMZ/KML\" \"\" \"kml;kmz\" 4)) (if file_path (set_tile \"file_path\" file_path))")
  
  (action_tile "btn_settings"
    "(if (new_dialog \"kmz_settings_dialog\" dcl_id)
        (progn
          (if (= *crs_type* \"3857\") (set_tile \"opt_3857\" \"1\") (set_tile \"opt_utm\" \"1\"))
          (set_tile \"set_utm_zone\" *kmz_utm_zone*)
          (if (= *kmz_hemisphere* \"S\") (set_tile \"opt_south\" \"1\") (set_tile \"opt_north\" \"1\"))
          (action_tile \"accept\"
            \"(progn
               (setq *crs_type* (if (= (get_tile \\\"opt_3857\\\") \\\"1\\\") \\\"3857\\\" \\\"UTM\\\"))
               (setq *kmz_utm_zone* (strcase (get_tile \\\"set_utm_zone\\\")))
               (setq *kmz_hemisphere* (if (= (get_tile \\\"opt_south\\\") \\\"1\\\") \\\"S\\\" \\\"N\\\"))
               (setenv \\\"KMZ_CRS_TYPE\\\" *crs_type*)
               (setenv \\\"KMZ_UTM_ZONE\\\" *kmz_utm_zone*)
               (setenv \\\"KMZ_HEMISPHERE\\\" *kmz_hemisphere*)
               (done_dialog 1)
             )\"
          )
          (start_dialog)
        )
      )"
  )

  (action_tile "accept" "(progn (setq file_path (get_tile \"file_path\")) (setq import_type (if (= (get_tile \"opt_point\") \"1\") \"POINT\" \"BLOCK\")) (if my_blocks (progn (setq selected_idx (atoi (get_tile \"block_list\"))) (setq block_name (nth selected_idx my_blocks))) (setq block_name \"NONE\")) (setq use_label (get_tile \"chk_label\")) (if (= file_path \"\") (alert \"Pilih filenya dulu brok!\") (done_dialog 1)))")
  (action_tile "cancel" "(done_dialog 0)")
  
  (setq result (start_dialog))
  (unload_dialog dcl_id)
  (vl-file-delete dcl_file)

  (if (= result 1)
    (progn
      ;; PENGGUNAAN '/' UNTUK PATHING MAC
      (setq ext (strcase (vl-filename-extension file_path)) target_kml nil is_temp_file nil temp_dir (strcat (vl-filename-directory file_path) "/_temp_kmz"))

      (if (= ext ".KMZ")
        (progn
          (vl-mkdir temp_dir) 
          (princ "\n📦 Mengekstrak KMZ (Mac UNIX Unzip)...")
          (startapp "/usr/bin/unzip" (strcat "-q -o \"" file_path "\" -d \"" temp_dir "\""))
          ;; Delay pintar buat nunggu unzip mac kelar
          (setq kml_files nil retry 0)
          (while (and (not kml_files) (< retry 20))
             (command "_.DELAY" 500)
             (setq kml_files (vl-directory-files temp_dir "*.kml" 1))
             (setq retry (1+ retry))
          )
          (if kml_files (setq target_kml (strcat temp_dir "/" (car kml_files))))
          (setq is_temp_file T)
        )
        (setq target_kml file_path is_temp_file T temp_dir (strcat (vl-filename-directory file_path) "/_temp_kml") target_kml (progn (vl-mkdir temp_dir) file_path))
      )

      (if target_kml
        (progn
          (setq normalized_kml (strcat temp_dir "/normalized_doc.kml") fp_in (open target_kml "r") fp_out (open normalized_kml "w") prev_char nil buf "")
          (if fp_in (progn (while (setq char (read-char fp_in)) (if (and (= prev_char 62) (= char 60)) (progn (write-line buf fp_out) (setq buf "<")) (setq buf (strcat buf (chr char)))) (setq prev_char char)) (if (/= buf "") (write-line buf fp_out)) (close fp_in) (close fp_out)))

          (setq fp (open normalized_kml "r") pt_name "" obj_type "POINT" in_coords nil coord_buffer "" pts_count 0)
          (setq auto_zone (if (or (= *kmz_utm_zone* "AUTO") (= *kmz_utm_zone* "") (= (atoi *kmz_utm_zone*) 0)) T nil))
          (if (not auto_zone) (setq utm_zone (atoi *kmz_utm_zone*) is_south (if (= *kmz_hemisphere* "S") T nil)))
          (setq zone_detected nil)

          (if fp
            (progn
              (while (setq line (read-line fp))
                (if (vl-string-search "<name>" line) (setq pt_name (extract_xml_text line "<name>" "</name>")))
                (if (vl-string-search "<LineString>" line) (setq obj_type "LINE")) (if (vl-string-search "<Polygon>" line) (setq obj_type "POLYGON")) (if (vl-string-search "<Point>" line) (setq obj_type "POINT"))
                (if (vl-string-search "<coordinates>" line) (setq in_coords T)) (if in_coords (setq coord_buffer (strcat coord_buffer " " line)))
                
                (if (vl-string-search "</coordinates>" line)
                  (progn
                    (setq in_coords nil coord_buffer (vl-string-subst "" "<coordinates>" coord_buffer) coord_buffer (vl-string-subst "" "</coordinates>" coord_buffer))
                    (setq pt_list (parse-coord-string coord_buffer))
                    (if (> (length pt_list) 0)
                      (progn
                        (setq xy (get-xy-from-comma-str (car pt_list)))
                        (if (and xy (not zone_detected))
                          (progn
                            (if auto_zone
                              (progn
                                (setq utm_zone (1+ (fix (/ (+ (car xy) 180.0) 6.0)))) (setq is_south (if (< (cadr xy) 0.0) T nil))
                                (setenv "KMZ_UTM_ZONE" (itoa utm_zone)) (setenv "KMZ_HEMISPHERE" (if is_south "S" "N"))
                                (if (/= *crs_type* "3857") (princ (strcat "\n🌍 [AUTO-CRS] Zona terdeteksi di UTM Zone " (itoa utm_zone) (if is_south " South" " North"))))
                              )
                            )
                            (setq zone_detected T)
                          )
                        )
                        (if xy
                          (if (= (length pt_list) 1)
                            (progn
                              (setq pt (wgs84-to-cad (cadr xy) (car xy) utm_zone is_south *crs_type*))
                              (if (= import_type "POINT") (entmake (list '(0 . "POINT") (cons 10 pt))) (entmake (list '(0 . "INSERT") (cons 2 block_name) (cons 10 pt))))
                              (inject-xdata (entlast) pt_name) (if (and (= use_label "1") (/= pt_name "")) (entmake (list '(0 . "TEXT") (cons 10 pt) (cons 11 pt) (cons 40 2.0) (cons 1 pt_name) '(72 . 1) '(73 . 2)))) (setq pts_count (1+ pts_count))
                            )
                            (progn
                              (setq poly_ent_data (list '(0 . "LWPOLYLINE") '(100 . "AcDbEntity") '(100 . "AcDbPolyline") (cons 90 (length pt_list))))
                              (if (= obj_type "POLYGON") (setq poly_ent_data (append poly_ent_data (list '(70 . 1)))) (setq poly_ent_data (append poly_ent_data (list '(70 . 0)))))
                              (foreach cstr pt_list (setq xy (get-xy-from-comma-str cstr)) (if xy (setq poly_ent_data (append poly_ent_data (list (cons 10 (list (car (wgs84-to-cad (cadr xy) (car xy) utm_zone is_south *crs_type*)) (cadr (wgs84-to-cad (cadr xy) (car xy) utm_zone is_south *crs_type*)))))))))
                              (entmake poly_ent_data) (inject-xdata (entlast) pt_name) (setq pts_count (1+ pts_count))
                            )
                          )
                        )
                      )
                    )
                    (setq coord_buffer "" pt_name "" obj_type "POINT")
                  )
                )
              )
              (close fp)
              (setvar "PDMODE" 34) (setvar "PDSIZE" 2.0) (command "_.ZOOM" "_E")
              (alert (strcat "Boom! " (itoa pts_count) " Objek berhasil di-import (Mac Edition)!"))
            )
          )
          ;; BERSIH BERSIH FOLDER MAC (/bin/sh)
          (if is_temp_file 
            (progn 
              (startapp "/bin/sh" (strcat "-c \"rm -rf '" temp_dir "'\""))
            )
          )
        )
      )
    )
  )
  (princ)
)

;; ==========================================
;; 6. COMMAND 2: SLAEXP (EXPORT KML)
;; ==========================================
(defun c:SLAEXP ( / zone_str is_south utm_zone crs_type ss filepath fp i ent ent_data obj_type layer_name obj_name is_closed pts pt geom_type all_items unique_layers geom_types layer_items type_items wgs_pt first_pt align_h align_v)
  (sync-spm-crs)
  
  (setq zone_str (getenv "KMZ_UTM_ZONE")) (setq is_south (if (= (getenv "KMZ_HEMISPHERE") "S") T nil))
  (setq crs_type (if (getenv "KMZ_CRS_TYPE") (getenv "KMZ_CRS_TYPE") "UTM"))
  
  (if (and (= crs_type "UTM") (or (not zone_str) (= (strcase zone_str) "AUTO") (= zone_str "")))
    (progn (alert "Waduh! UTM Zone lu masih AUTO brok.\nJalanin SLAIMP > API Settings buat nentuin zona lu.") (exit))
  )
  (setq utm_zone (if zone_str (atoi zone_str) 0))

  (princ "\nBlok Objek yang mau di-export: ")
  (setq ss (ssget '((0 . "POINT,INSERT,LWPOLYLINE,TEXT,MTEXT"))))
  (if (not ss) (progn (princ "\nGa ada objek yang dipilih.") (exit)))

  (setq all_items nil unique_layers nil i 0)
  (while (< i (sslength ss))
    (setq ent (ssname ss i) ent_data (entget ent) obj_type (cdr (assoc 0 ent_data)))
    (setq layer_name (vl-string-translate "<>&" "---" (cdr (assoc 8 ent_data))))
    (setq obj_name (vl-string-translate "<>&" "---" (get-object-name ent ent_data obj_type)))

    (setq geom_type nil pts nil is_closed nil)
    (cond
      ((or (= obj_type "POINT") (= obj_type "INSERT") (= obj_type "TEXT") (= obj_type "MTEXT"))
        (setq geom_type "Point")
        (if (= obj_type "TEXT")
          (progn (setq align_h (cdr (assoc 72 ent_data)) align_v (cdr (assoc 73 ent_data))) (if (and (or (and align_h (> align_h 0)) (and align_v (> align_v 0))) (assoc 11 ent_data)) (setq pts (cdr (assoc 11 ent_data))) (setq pts (cdr (assoc 10 ent_data)))))
          (setq pts (cdr (assoc 10 ent_data))) 
        )
      )
      ((= obj_type "LWPOLYLINE")
        (setq is_closed (= (logand 1 (cdr (assoc 70 ent_data))) 1) geom_type (if is_closed "Polygon" "LineString"))
        (foreach item ent_data (if (= (car item) 10) (setq pts (append pts (list (cdr item))))))
      )
    )
    (if geom_type (progn (setq all_items (append all_items (list (list layer_name geom_type obj_name pts is_closed)))) (if (not (member layer_name unique_layers)) (setq unique_layers (append unique_layers (list layer_name))))))
    (setq i (1+ i))
  )
  
  (setq unique_layers (vl-sort unique_layers '<))
  
  ;; MENGGUNAKAN DWGPREFIX UNTUK DEFAULT FOLDER DI MAC (Mencegah Error "C:\")
  (setq filepath (getfiled "Save File KML Export" (getvar "DWGPREFIX") "kml" 1))
  (if (not filepath) (exit))
  
  (setq fp (open filepath "w"))
  (write-line "<?xml version=\"1.0\" encoding=\"UTF-8\"?><kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document>" fp)
  (write-line (strcat "<name>Export_" crs_type "</name>") fp)

  (setq geom_types '("Polygon" "LineString" "Point"))
  (foreach layer unique_layers
    (setq layer_items (vl-remove-if-not '(lambda (x) (= (car x) layer)) all_items))
    (write-line (strcat "<Folder><name>" layer "</name>") fp)
    (foreach gtype geom_types
      (setq type_items (vl-remove-if-not '(lambda (x) (= (cadr x) gtype)) layer_items))
      (if (> (length type_items) 0)
        (progn
          (write-line (strcat "<Folder><name>" gtype "s</name>") fp)
          (foreach item type_items
            (setq obj_name (nth 2 item) pts (nth 3 item) is_closed (nth 4 item))
            (write-line (strcat "<Placemark><name>" obj_name "</name>") fp)
            (cond
              ((= gtype "Point")
                (setq wgs_pt (cad-to-wgs84 (car pts) (cadr pts) utm_zone is_south crs_type))
                (write-line "<Style><IconStyle><Icon/></IconStyle><LabelStyle><color>FF0000FF</color></LabelStyle></Style><Point><altitudeMode>clampToGround</altitudeMode><coordinates>" fp)
                (write-line (strcat (rtos (car wgs_pt) 2 8) "," (rtos (cadr wgs_pt) 2 8) ",0</coordinates></Point>") fp)
              )
              ((= gtype "LineString")
                (write-line "<Style><LineStyle><color>FF0000FF</color><width>2</width></LineStyle></Style><LineString><altitudeMode>clampToGround</altitudeMode><coordinates>" fp)
                (foreach pt pts (setq wgs_pt (cad-to-wgs84 (car pt) (cadr pt) utm_zone is_south crs_type)) (write-line (strcat (rtos (car wgs_pt) 2 8) "," (rtos (cadr wgs_pt) 2 8) ",0 ") fp))
                (write-line "</coordinates></LineString>" fp)
              )
              ((= gtype "Polygon")
                (write-line "<Style><LineStyle><color>FF0000FF</color><width>2</width></LineStyle><PolyStyle><color>B30000FF</color></PolyStyle></Style><Polygon><altitudeMode>clampToGround</altitudeMode><outerBoundaryIs><LinearRing><coordinates>" fp)
                (foreach pt pts (setq wgs_pt (cad-to-wgs84 (car pt) (cadr pt) utm_zone is_south crs_type)) (write-line (strcat (rtos (car wgs_pt) 2 8) "," (rtos (cadr wgs_pt) 2 8) ",0 ") fp))
                (setq first_pt (cad-to-wgs84 (car (car pts)) (cadr (car pts)) utm_zone is_south crs_type))
                (write-line (strcat (rtos (car first_pt) 2 8) "," (rtos (cadr first_pt) 2 8) ",0 </coordinates></LinearRing></outerBoundaryIs></Polygon>") fp)
              )
            )
            (write-line "</Placemark>" fp)
          )
          (write-line "</Folder>" fp)
        )
      )
    )
    (write-line "</Folder>" fp)
  )

  (write-line "</Document></kml>" fp)
  (close fp)
  (princ (strcat "\nBoom! " (itoa (length all_items)) " Objek berhasil di-export ke KMZ!"))
  (princ)
)

;; ==========================================
;; 7. COMMAND 3: SLAMAP (DOWNLOAD MAP - MAC CURL)
;; ==========================================
(defun c:SLAMAP ( / zone_str is_south utm_zone crs_type pt1 pt2 x_min y_min x_max y_max center_x center_y center_wgs c_lon c_lat east_wgs north_wgs m_per_deg_lon m_per_deg_lat tl_wgs br_wgs tile_min tile_max x_start x_end y_start y_end zoom_level total_tiles cur_x cur_y url filepath tile_wgs_tl tile_wgs_br ins_x ins_y ins_pt tile_w_m tile_h_m img_ent img_obj rot_deg old_osmode min_pt max_pt pt_min pt_max phys_width scale_ratio tile_group max_tile_dim clip_pt2 tile_idx prog_msg retry)
  (sync-spm-crs)
  
  (setvar "CMDECHO" 0) (setvar "IMAGEFRAME" 0)
  (setq zone_str (getenv "KMZ_UTM_ZONE") is_south (if (= (getenv "KMZ_HEMISPHERE") "S") T nil))
  (setq crs_type (if (getenv "KMZ_CRS_TYPE") (getenv "KMZ_CRS_TYPE") "UTM"))

  (if (and (= crs_type "UTM") (or (not zone_str) (= (strcase zone_str) "AUTO") (= zone_str "")))
    (progn (alert "Set UTM Zone dulu pake SLAIMP brok!") (exit))
  )
  (setq utm_zone (if zone_str (atoi zone_str) 0))

  (setq pt1 (getpoint "\n[1] Klik ujung KIRI ATAS area proyek lu: ")) (if (not pt1) (exit))
  (setq pt2 (getcorner pt1 "\n[2] Tarik kotak sampai ujung KANAN BAWAH area lu: ")) (if (not pt2) (exit))

  (setq x_min (min (car pt1) (car pt2)) x_max (max (car pt1) (car pt2)) y_min (min (cadr pt1) (cadr pt2)) y_max (max (cadr pt1) (cadr pt2)))
  (setq center_x (/ (+ x_min x_max) 2.0) center_y (/ (+ y_min y_max) 2.0))
  (setq center_wgs (cad-to-wgs84 center_x center_y utm_zone is_south crs_type) c_lon (car center_wgs) c_lat (cadr center_wgs))
  (setq east_wgs (cad-to-wgs84 (+ center_x 1000.0) center_y utm_zone is_south crs_type) north_wgs (cad-to-wgs84 center_x (+ center_y 1000.0) utm_zone is_south crs_type))
  (setq m_per_deg_lon (/ 1000.0 (- (car east_wgs) c_lon)) m_per_deg_lat (/ 1000.0 (- (cadr north_wgs) c_lat)))
  (setq tl_wgs (cad-to-wgs84 x_min y_max utm_zone is_south crs_type) br_wgs (cad-to-wgs84 x_max y_min utm_zone is_south crs_type))

  (setq zoom_level 19 tile_min (get-osm-tile (cadr tl_wgs) (car tl_wgs) zoom_level) tile_max (get-osm-tile (cadr br_wgs) (car br_wgs) zoom_level))
  (setq total_tiles (* (+ 1 (- (car tile_max) (car tile_min))) (+ 1 (- (cadr tile_max) (cadr tile_min)))))
  
  (while (> total_tiles 400)
    (setq zoom_level (1- zoom_level) tile_min (get-osm-tile (cadr tl_wgs) (car tl_wgs) zoom_level) tile_max (get-osm-tile (cadr br_wgs) (car br_wgs) zoom_level))
    (setq total_tiles (* (+ 1 (- (car tile_max) (car tile_min))) (+ 1 (- (cadr tile_max) (cadr tile_min)))))
  )

  (setq x_start (car tile_min) x_end (car tile_max) y_start (cadr tile_min) y_end (cadr tile_max))
  (setq rot_deg (if (= crs_type "3857") 0.0 (get-grid-convergence-deg c_lat c_lon utm_zone)))
  
  (if (/= (getvar "CLAYER") "MAP_BACKGROUND")
    (setq *sla_prev_layer* (getvar "CLAYER"))
  )
  
  (command "_.-LAYER" "_Make" "MAP_BACKGROUND" "_Color" "8" "" "_Set" "MAP_BACKGROUND" "")
  
  (setq tile_group (ssadd) old_osmode (getvar "OSMODE"))
  (setvar "OSMODE" 0)

  (princ (strcat "\nSedang mendownload " (itoa total_tiles) " ubin peta... Cek progress di pojok kiri bawah!"))
  
  (setq cur_x x_start tile_idx 0)
  (while (<= cur_x x_end)
    (setq cur_y y_start)
    (while (<= cur_y y_end)
      (setq url (strcat "https://mt1.google.com/vt/lyrs=s&x=" (itoa cur_x) "&y=" (itoa cur_y) "&z=" (itoa zoom_level)))
      (setq filepath (vl-filename-mktemp (strcat "kmz_gmap_" (itoa cur_x) "_" (itoa cur_y) "_") (getvar "TEMPPREFIX") ".png"))
      (setq tile_wgs_tl (tile-to-wgs84 cur_x cur_y zoom_level) tile_wgs_br (tile-to-wgs84 (+ cur_x 1) (+ cur_y 1) zoom_level))
      (setq ins_x (+ center_x (* (- (car tile_wgs_tl) c_lon) m_per_deg_lon)) ins_y (+ center_y (* (- (cadr tile_wgs_br) c_lat) m_per_deg_lat)) ins_pt (list ins_x ins_y))
      (setq tile_w_m (* (- (car tile_wgs_br) (car tile_wgs_tl)) m_per_deg_lon) tile_h_m (* (- (cadr tile_wgs_tl) (cadr tile_wgs_br)) m_per_deg_lat))
      
      (setq tile_idx (1+ tile_idx))
      (setq prog_msg (strcat "SLA Master Mac: Menjahit Ubin " (itoa tile_idx) " / " (itoa total_tiles)))
      (grtext -1 prog_msg)
      
      ;; MENGGUNAKAN MAC CURL UNTUK DOWNLOAD (Silent mode, custom Agent)
      (startapp "/usr/bin/curl" (strcat "-s -A 'Mozilla/5.0' -o \"" filepath "\" \"" url "\""))
      
      ;; DELAY LOOP (Menunggu cURL selesai nulis gambar)
      (setq retry 0)
      (while (and (not (findfile filepath)) (< retry 20))
         (command "_.DELAY" 200)
         (setq retry (1+ retry))
      )

      (if (findfile filepath)
        (progn
          (setvar "FILEDIA" 0)
          (command "_.-IMAGE" "_Attach" filepath "_NON" ins_pt 1.0 0.0)
          (setq img_ent (entlast) img_obj (vlax-ename->vla-object img_ent))
          (vla-GetBoundingBox img_obj 'min_pt 'max_pt)
          (setq pt_min (vlax-safearray->list min_pt) pt_max (vlax-safearray->list max_pt) phys_width (- (car pt_max) (car pt_min))) 
          (setq max_tile_dim (max tile_w_m tile_h_m) scale_ratio (/ (float max_tile_dim) (float phys_width)))
          (command "_SCALE" img_ent "" "_NON" ins_pt scale_ratio)
          (setq clip_pt2 (list (+ ins_x tile_w_m) (+ ins_y tile_h_m)))
          (command "_.IMAGECLIP" img_ent "_New" "_Rectangular" "_NON" ins_pt "_NON" clip_pt2)
          (command "_.DRAWORDER" img_ent "" "_Back") 
          (ssadd img_ent tile_group)
          (setvar "FILEDIA" 1)
        )
      )
      (setq cur_y (1+ cur_y))
    )
    (setq cur_x (1+ cur_x))
  )
  
  (grtext -1 "")
  
  (if (and (> (sslength tile_group) 0) (/= rot_deg 0.0)) (command "_.ROTATE" tile_group "" "_NON" (list center_x center_y) rot_deg))

  (setvar "OSMODE" old_osmode) (setvar "CMDECHO" 1)
  (princ "\nBOOM! PETA PRESISI BERHASIL DI-LOAD DI MAC!")
  (princ)
)

;; ==========================================
;; 8. COMMAND 4 & 5: HIDEMAP & SHOWMAP (LAYER MEMORY)
;; ==========================================
(defun c:HIDEMAP ()
  (if (not (tblsearch "LAYER" "MAP_BACKGROUND"))
    (princ "\nLayer peta belum ada brok. Ketik SLAMAP dulu buat download!")
    (progn
      (if (= (getvar "CLAYER") "MAP_BACKGROUND")
        (if (and *sla_prev_layer* (tblsearch "LAYER" *sla_prev_layer*) (/= *sla_prev_layer* "MAP_BACKGROUND"))
          (setvar "CLAYER" *sla_prev_layer*)
          (setvar "CLAYER" "0")
        )
      )
      (command "_.-LAYER" "_Freeze" "MAP_BACKGROUND" "")
      (princ "\n❄️ Peta dibekukan (Freeze) dan layer dikembalikan. Ketik SHOWMAP buat nampilin lagi.")
    )
  )
  (princ)
)

(defun c:SHOWMAP ()
  (if (not (tblsearch "LAYER" "MAP_BACKGROUND"))
    (princ "\nLayer peta belum ada brok. Ketik SLAMAP dulu buat download!")
    (progn
      (command "_.-LAYER" "_Thaw" "MAP_BACKGROUND" "")
      (princ "\n🔥 Peta dicairkan (Thaw). Peta kembali muncul tanpa download ulang!")
    )
  )
  (princ)
)

(princ "\n=============================================")
(princ "\n[SLA Master V11.1 - Final MAC Edition]")
(princ "\nPerintah yang tersedia:")
(princ "\n  - SLAIMP  : Import KMZ/KML")
(princ "\n  - SLAEXP  : Export Objek ke KML")
(princ "\n  - SLAMAP  : Download Peta & Masukkan ke AutoCAD")
(princ "\n  - HIDEMAP : Bekukan (Freeze) Peta Sementara")
(princ "\n  - SHOWMAP : Munculkan Kembali Peta")
(princ "\n=============================================")
(princ)