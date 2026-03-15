#include "SlaMath.h"
#include <cmath>

namespace SlaMath {
    constexpr double kPi = 3.14159265358979323846;

    void wgs84_to_webmercator(double lat, double lon, double& x, double& y) {
        constexpr double a = 6378137.0; 
        x = lon * (kPi * a) / 180.0;
        y = std::log(std::tan((90.0 + lat) * kPi / 360.0)) * a;
    }

    void wgs84_to_utm(double lat, double lon, int zone, bool is_south, double& x, double& y) {
        double a = 6378137.0;
        double eccSq = 0.0066943799901413165;
        double eccPrimeSq = eccSq / (1.0 - eccSq);
        double k0 = 0.9996;

        double radLat = lat * kPi / 180.0;
        double radLon = lon * kPi / 180.0;
        double lonOrigin = (-183.0 + (zone * 6.0)) * kPi / 180.0;

        double N = a / std::sqrt(1.0 - eccSq * std::sin(radLat) * std::sin(radLat));
        double T_val = std::tan(radLat) * std::tan(radLat);
        double C = eccPrimeSq * std::cos(radLat) * std::cos(radLat);
        double A_val = std::cos(radLat) * (radLon - lonOrigin);

        double M = a * ((1.0 - eccSq / 4.0 - 3.0 * eccSq * eccSq / 64.0 - 5.0 * eccSq * eccSq * eccSq / 256.0) * radLat
            - (3.0 * eccSq / 8.0 + 3.0 * eccSq * eccSq / 32.0 + 45.0 * eccSq * eccSq * eccSq / 1024.0) * std::sin(2.0 * radLat)
            + (15.0 * eccSq * eccSq / 256.0 + 45.0 * eccSq * eccSq * eccSq / 1024.0) * std::sin(4.0 * radLat)
            - (35.0 * eccSq * eccSq * eccSq / 3072.0) * std::sin(6.0 * radLat));

        x = k0 * N * A_val * (1.0 + A_val * A_val / 6.0 * (1.0 - T_val + C)
            + A_val * A_val * A_val * A_val / 120.0 * (5.0 - 18.0 * T_val + T_val * T_val + 72.0 * C - 58.0 * eccPrimeSq)) + 500000.0;

        y = k0 * (M + N * std::tan(radLat) * (A_val * A_val / 2.0 
            + A_val * A_val * A_val * A_val / 24.0 * (5.0 - T_val + 9.0 * C + 4.0 * C * C)
            + A_val * A_val * A_val * A_val * A_val * A_val / 720.0 * (61.0 - 58.0 * T_val + T_val * T_val + 600.0 * C - 330.0 * eccPrimeSq)));

        if (is_south) y += 10000000.0;
    }

    void wgs84_to_cad(double lat, double lon, int zone, bool is_south, bool use_3857, double& x, double& y) {
        if (use_3857) wgs84_to_webmercator(lat, lon, x, y);
        else wgs84_to_utm(lat, lon, zone, is_south, x, y);
    }

    // INVERSE: CAD ke WGS84 (Buat Export)
    void webmercator_to_wgs84(double x, double y, double& lat, double& lon);
    void utm_to_wgs84(double x, double y, int zone, bool is_south, double& lat, double& lon);
    void cad_to_wgs84(double x, double y, int zone, bool is_south, bool use_3857, double& lat, double& lon);

    void webmercator_to_wgs84(double x, double y, double& lat, double& lon) {
        constexpr double a = 6378137.0;
        lon = (x / a) * 180.0 / kPi;
        lat = std::atan(std::exp(y / a)) * 360.0 / kPi - 90.0;
    }

    void utm_to_wgs84(double x, double y, int zone, bool is_south, double& lat, double& lon) {
        constexpr double a = 6378137.0;
        constexpr double eccSq = 0.0066943799901413165;
        constexpr double k0 = 0.9996;
        double eccPrimeSq = eccSq / (1.0 - eccSq);
        double e1 = (1.0 - std::sqrt(1.0 - eccSq)) / (1.0 + std::sqrt(1.0 - eccSq));

        double x_adj = x - 500000.0;
        double y_adj = is_south ? y - 10000000.0 : y;

        double M = y_adj / k0;
        double mu = M / (a * (1.0 - eccSq / 4.0 - 3.0 * eccSq * eccSq / 64.0 - 5.0 * eccSq * eccSq * eccSq / 256.0));

        double phi1 = mu + (3.0 * e1 / 2.0 - 27.0 * e1 * e1 * e1 / 32.0) * std::sin(2.0 * mu)
                    + (21.0 * e1 * e1 / 16.0 - 55.0 * e1 * e1 * e1 * e1 / 32.0) * std::sin(4.0 * mu)
                    + (151.0 * e1 * e1 * e1 / 96.0) * std::sin(6.0 * mu);

        double N1 = a / std::sqrt(1.0 - eccSq * std::sin(phi1) * std::sin(phi1));
        double T1 = std::tan(phi1) * std::tan(phi1);
        double C1 = eccPrimeSq * std::cos(phi1) * std::cos(phi1);
        double R1 = a * (1.0 - eccSq) / std::pow(1.0 - eccSq * std::sin(phi1) * std::sin(phi1), 1.5);
        double D = x_adj / (N1 * k0);

        double tLat = phi1 - (N1 * std::tan(phi1) / R1) * (D * D / 2.0
                      - (5.0 + 3.0 * T1 + 10.0 * C1 - 4.0 * C1 * C1 - 9.0 * eccPrimeSq) * D * D * D * D / 24.0
                      + (61.0 + 90.0 * T1 + 298.0 * C1 + 45.0 * T1 * T1 - 252.0 * eccPrimeSq - 3.0 * C1 * C1) * D * D * D * D * D * D / 720.0);

        double lonOrigin = (-183.0 + zone * 6.0) * kPi / 180.0;
        double tLon = lonOrigin + (D - (1.0 + 2.0 * T1 + C1) * D * D * D / 6.0
                      + (5.0 - 2.0 * C1 + 28.0 * T1 - 3.0 * C1 * C1 + 8.0 * eccPrimeSq + 24.0 * T1 * T1) * D * D * D * D * D / 120.0) / std::cos(phi1);

        lat = tLat * 180.0 / kPi;
        lon = tLon * 180.0 / kPi;
    }

    void cad_to_wgs84(double x, double y, int zone, bool is_south, bool use_3857, double& lat, double& lon) {
        if (use_3857) webmercator_to_wgs84(x, y, lat, lon);
        else utm_to_wgs84(x, y, zone, is_south, lat, lon);
    }
}