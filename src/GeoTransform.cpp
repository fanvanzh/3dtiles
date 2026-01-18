#include "GeoTransform.h"
#include <cstdio>

std::unique_ptr<OGRCoordinateTransformation, OGRCTDeleter> GeoTransform::pOgrCT = nullptr;
double GeoTransform::OriginX = 0.0;
double GeoTransform::OriginY = 0.0;
double GeoTransform::OriginZ = 0.0;
double GeoTransform::GeoOriginLon = 0.0;
double GeoTransform::GeoOriginLat = 0.0;
double GeoTransform::GeoOriginHeight = 0.0;
bool GeoTransform::IsENU = false;
glm::dmat4 GeoTransform::EcefToEnuMatrix = glm::dmat4(1);

glm::dmat4 GeoTransform::CalcEnuToEcefMatrix(double lnt, double lat, double height_min)
{
    const double pi = std::acos(-1.0);
    const double a = 6378137.0;                  // WGS84 semi-major axis
    const double f = 1.0 / 298.257223563;        // WGS84 flattening
    const double e2 = f * (2.0 - f);             // eccentricity squared

    double lon = lnt * pi / 180.0;
    double phi = lat * pi / 180.0;

    double sinPhi = std::sin(phi), cosPhi = std::cos(phi);
    double sinLon = std::sin(lon), cosLon = std::cos(lon);

    double N = a / std::sqrt(1.0 - e2 * sinPhi * sinPhi);
    double x0 = (N + height_min) * cosPhi * cosLon;
    double y0 = (N + height_min) * cosPhi * sinLon;
    double z0 = (N * (1.0 - e2) + height_min) * sinPhi;

    // ENU basis vectors expressed in ECEF
    glm::dvec3 east(-sinLon,           cosLon,            0.0);
    glm::dvec3 north(-sinPhi * cosLon, -sinPhi * sinLon,  cosPhi);
    glm::dvec3 up(   cosPhi * cosLon,   cosPhi * sinLon,  sinPhi);

    // Build ENU->ECEF (rotation + translation), column-major
    glm::dmat4 T(1.0);
    T[0] = glm::dvec4(east,  0.0);
    T[1] = glm::dvec4(north, 0.0);
    T[2] = glm::dvec4(up,    0.0);
    T[3] = glm::dvec4(x0, y0, z0, 1.0);
    return T;
}

glm::dvec3 GeoTransform::CartographicToEcef(double lnt, double lat, double height)
{
    const double pi = std::acos(-1.0);
    const double a = 6378137.0;                  // WGS84 semi-major axis
    const double f = 1.0 / 298.257223563;        // WGS84 flattening
    const double e2 = f * (2.0 - f);             // eccentricity squared

    double lon = lnt * pi / 180.0;
    double phi = lat * pi / 180.0;

    double sinPhi = std::sin(phi), cosPhi = std::cos(phi);
    double sinLon = std::sin(lon), cosLon = std::cos(lon);

    double N = a / std::sqrt(1.0 - e2 * sinPhi * sinPhi);
    double x = (N + height) * cosPhi * cosLon;
    double y = (N + height) * cosPhi * sinLon;
    double z = (N * (1.0 - e2) + height) * sinPhi;

    return { x, y, z };
}

void GeoTransform::Init(OGRCoordinateTransformation *pOgrCT, double *Origin)
{
    // Use smart pointer to manage coordinate transformation object, automatically releases old resources
    GeoTransform::pOgrCT.reset(pOgrCT);
    GeoTransform::OriginX = Origin[0];
    GeoTransform::OriginY = Origin[1];
    GeoTransform::OriginZ = Origin[2];
    GeoTransform::IsENU = false;  // Default to non-ENU, will be set by SetGeographicOrigin if ENU

    glm::dvec3 origin = { GeoTransform::OriginX, GeoTransform::OriginY, GeoTransform::OriginZ };
    glm::dvec3 origin_cartographic = origin;
    // Log ENU origin before transform
    fprintf(stderr, "[GeoTransform] ENU origin: x=%.8f y=%.8f z=%.3f\n", origin.x, origin.y, origin.z);

    // Use get() to obtain raw pointer for coordinate transformation
    if (GeoTransform::pOgrCT)
    {
        GeoTransform::pOgrCT->Transform(1, &origin_cartographic.x, &origin_cartographic.y, &origin_cartographic.z);
    }

    // Log cartographic origin after transform (degrees)
    fprintf(stderr, "[GeoTransform] Cartographic origin: lon=%.10f lat=%.10f h=%.3f\n", origin_cartographic.x, origin_cartographic.y, origin_cartographic.z);

    // For EPSG systems, use the transformed cartographic origin
    // For ENU systems, this will be overridden by SetGeographicOrigin()
    GeoTransform::GeoOriginLon = origin_cartographic.x;
    GeoTransform::GeoOriginLat = origin_cartographic.y;
    GeoTransform::GeoOriginHeight = origin_cartographic.z;

    glm::dmat4 EnuToEcefMatrix = GeoTransform::CalcEnuToEcefMatrix(origin_cartographic.x, origin_cartographic.y, origin_cartographic.z);
    GeoTransform::EcefToEnuMatrix = glm::inverse(EnuToEcefMatrix);
}

void GeoTransform::SetGeographicOrigin(double lon, double lat, double height)
{
    GeoTransform::GeoOriginLon = lon;
    GeoTransform::GeoOriginLat = lat;
    GeoTransform::GeoOriginHeight = height;
    GeoTransform::IsENU = true;

    // Recalculate ENU<->ECEF matrices using the geographic origin
    glm::dmat4 EnuToEcefMatrix = GeoTransform::CalcEnuToEcefMatrix(lon, lat, height);
    GeoTransform::EcefToEnuMatrix = glm::inverse(EnuToEcefMatrix);

    fprintf(stderr, "[GeoTransform] Geographic origin set: lon=%.10f lat=%.10f h=%.3f\n", lon, lat, height);
}
