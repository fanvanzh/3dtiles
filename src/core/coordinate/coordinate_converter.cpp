#include "coordinate_converter.h"
#include <ogrsf_frmts.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace Tiles::Core::Geo {

// Static utility methods

glm::dvec3 CoordinateConverter::geographicToEcef(double lon, double lat, double height) {
    double lon_rad = lon * DEG_TO_RAD;
    double lat_rad = lat * DEG_TO_RAD;

    double sinLat = std::sin(lat_rad);
    double cosLat = std::cos(lat_rad);
    double sinLon = std::sin(lon_rad);
    double cosLon = std::cos(lon_rad);

    // Radius of curvature in the prime vertical
    double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

    double x = (N + height) * cosLat * cosLon;
    double y = (N + height) * cosLat * sinLon;
    double z = (N * (1.0 - WGS84_E2) + height) * sinLat;

    return glm::dvec3(x, y, z);
}

void CoordinateConverter::ecefToGeographic(const glm::dvec3& ecef,
                                           double& lon, double& lat, double& height) {
    double x = ecef.x;
    double y = ecef.y;
    double z = ecef.z;

    // Calculate longitude
    lon = std::atan2(y, x) * RAD_TO_DEG;

    // Bowring's method for latitude calculation
    double p = std::sqrt(x * x + y * y);
    double theta = std::atan2(z * WGS84_A, p * WGS84_B);

    lat = std::atan2(z + WGS84_EP2 * WGS84_B * std::pow(std::sin(theta), 3),
                     p - WGS84_E2 * WGS84_A * std::pow(std::cos(theta), 3));

    // Iterative refinement for better accuracy
    for (int i = 0; i < 5; ++i) {
        double sinLat = std::sin(lat);
        double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);
        double prevLat = lat;
        height = p / std::cos(lat) - N;
        lat = std::atan2(z, p * (1.0 - WGS84_E2 * N / (N + height)));

        if (std::abs(lat - prevLat) < 1e-12) break;
    }

    lat *= RAD_TO_DEG;
}

glm::dmat4 CoordinateConverter::calcEnuToEcefMatrix(double lon, double lat, double height) {
    double lon_rad = lon * DEG_TO_RAD;
    double lat_rad = lat * DEG_TO_RAD;

    double sinLat = std::sin(lat_rad);
    double cosLat = std::cos(lat_rad);
    double sinLon = std::sin(lon_rad);
    double cosLon = std::cos(lon_rad);

    // Calculate ECEF coordinates of origin
    double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);
    double x0 = (N + height) * cosLat * cosLon;
    double y0 = (N + height) * cosLat * sinLon;
    double z0 = (N * (1.0 - WGS84_E2) + height) * sinLat;

    // ENU basis vectors in ECEF
    glm::dvec3 east(-sinLon, cosLon, 0.0);
    glm::dvec3 north(-sinLat * cosLon, -sinLat * sinLon, cosLat);
    glm::dvec3 up(cosLat * cosLon, cosLat * sinLon, sinLat);

    // Build transformation matrix (column-major)
    glm::dmat4 T(1.0);
    T[0] = glm::dvec4(east, 0.0);
    T[1] = glm::dvec4(north, 0.0);
    T[2] = glm::dvec4(up, 0.0);
    T[3] = glm::dvec4(x0, y0, z0, 1.0);

    return T;
}

glm::dvec3 CoordinateConverter::projectedToGeographic(const glm::dvec3& projCoord,
                                                        const std::string& srsDefinition) {
    if (srsDefinition.empty()) {
        SPDLOG_WARN("Empty SRS definition for projected coordinate conversion");
        return projCoord;
    }

    OGRSpatialReference srs;
    OGRErr err = srs.SetFromUserInput(srsDefinition.c_str());
    if (err != OGRERR_NONE) {
        SPDLOG_WARN("Failed to create spatial reference from: {}", srsDefinition);
        return projCoord;
    }

    OGRSpatialReference wgs84;
    wgs84.SetWellKnownGeogCS("WGS84");

    // Use traditional GIS axis order
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRCoordinateTransformation* transform = OGRCreateCoordinateTransformation(&srs, &wgs84);
    if (!transform) {
        SPDLOG_WARN("Failed to create coordinate transformation");
        return projCoord;
    }

    double x = projCoord.x;
    double y = projCoord.y;
    double z = projCoord.z;

    int success = transform->Transform(1, &x, &y, &z);
    OGRCoordinateTransformation::DestroyCT(transform);

    if (!success) {
        SPDLOG_WARN("Coordinate transformation failed");
        return projCoord;
    }

    return glm::dvec3(x, y, z);
}

glm::dvec3 CoordinateConverter::geographicToProjected(const glm::dvec3& geoCoord,
                                                        const std::string& srsDefinition) {
    if (srsDefinition.empty()) {
        SPDLOG_WARN("Empty SRS definition for geographic coordinate conversion");
        return geoCoord;
    }

    OGRSpatialReference srs;
    OGRErr err = srs.SetFromUserInput(srsDefinition.c_str());
    if (err != OGRERR_NONE) {
        SPDLOG_WARN("Failed to create spatial reference from: {}", srsDefinition);
        return geoCoord;
    }

    OGRSpatialReference wgs84;
    wgs84.SetWellKnownGeogCS("WGS84");

    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRCoordinateTransformation* transform = OGRCreateCoordinateTransformation(&wgs84, &srs);
    if (!transform) {
        SPDLOG_WARN("Failed to create coordinate transformation");
        return geoCoord;
    }

    double x = geoCoord.x;
    double y = geoCoord.y;
    double z = geoCoord.z;

    int success = transform->Transform(1, &x, &y, &z);
    OGRCoordinateTransformation::DestroyCT(transform);

    if (!success) {
        SPDLOG_WARN("Coordinate transformation failed");
        return geoCoord;
    }

    return glm::dvec3(x, y, z);
}

glm::dvec3 CoordinateConverter::geographicToLocalMeter(const glm::dvec3& geoCoord,
                                                          const glm::dvec3& centerGeo) {
    const double metersPerDegreeLat = 111320.0;
    double centerLatRad = centerGeo.y * DEG_TO_RAD;
    double metersPerDegreeLon = 111320.0 * std::cos(centerLatRad);

    double dx = (geoCoord.x - centerGeo.x) * metersPerDegreeLon;
    double dy = (geoCoord.y - centerGeo.y) * metersPerDegreeLat;
    double dz = geoCoord.z;

    return glm::dvec3(dx, dy, dz);
}

// Instance methods

CoordinateConverter::CoordinateConverter(const CoordinateSystem& source,
                                          const CoordinateSystem& target)
    : source_(source), target_(target), ogrTransform_(nullptr), ownsTransform_(false) {

    if (!source.valid() || !target.valid()) {
        SPDLOG_WARN("Invalid coordinate system for conversion");
        return;
    }

    // For now, only support conversions that require OGR
    if (source.isProjected() && target.isGeographic()) {
        OGRSpatialReference srs;
        srs.SetFromUserInput(source.epsgCode.c_str());

        OGRSpatialReference wgs84;
        wgs84.SetWellKnownGeogCS("WGS84");

        srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        ogrTransform_ = OGRCreateCoordinateTransformation(&srs, &wgs84);
        ownsTransform_ = true;
    }
}

CoordinateConverter::~CoordinateConverter() {
    if (ownsTransform_ && ogrTransform_) {
        OGRCoordinateTransformation::DestroyCT(ogrTransform_);
    }
}

glm::dvec3 CoordinateConverter::convert(const glm::dvec3& point) const {
    if (!ogrTransform_) {
        return point;
    }

    double x = point.x;
    double y = point.y;
    double z = point.z;

    int success = ogrTransform_->Transform(1, &x, &y, &z);
    if (!success) {
        SPDLOG_WARN("Coordinate conversion failed");
        return point;
    }

    return glm::dvec3(x, y, z);
}

std::vector<glm::dvec3> CoordinateConverter::convertBatch(const std::vector<glm::dvec3>& points) const {
    if (!ogrTransform_ || points.empty()) {
        return points;
    }

    std::vector<double> xs, ys, zs;
    xs.reserve(points.size());
    ys.reserve(points.size());
    zs.reserve(points.size());

    for (const auto& p : points) {
        xs.push_back(p.x);
        ys.push_back(p.y);
        zs.push_back(p.z);
    }

    int success = ogrTransform_->Transform(static_cast<int>(points.size()),
                                           xs.data(), ys.data(), zs.data());

    if (!success) {
        SPDLOG_WARN("Batch coordinate conversion failed");
        return points;
    }

    std::vector<glm::dvec3> result;
    result.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        result.emplace_back(xs[i], ys[i], zs[i]);
    }

    return result;
}

}
