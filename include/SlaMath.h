#pragma once
#include "LiveMapCommon.h"

namespace SlaMath {
    // Web Mercator
    void wgs84_to_webmercator(double lat, double lon, double& x, double& y);
    
    // UTM (Ultra Precision)
    void wgs84_to_utm(double lat, double lon, int zone, bool is_south, double& x, double& y);
    
    // Universal Router
    void wgs84_to_cad(double lat, double lon, int zone, bool is_south, bool use_3857, double& x, double& y);
    
    // INVERSE: CAD ke WGS84 (Buat Export)
    void webmercator_to_wgs84(double x, double y, double& lat, double& lon);
    void utm_to_wgs84(double x, double y, int zone, bool is_south, double& lat, double& lon);
    void cad_to_wgs84(double x, double y, int zone, bool is_south, bool use_3857, double& lat, double& lon);
}

