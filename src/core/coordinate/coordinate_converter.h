#pragma once

#include "coordinate_system.h"
#include "enu_context.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

struct OGRCoordinateTransformation;
struct OGRSpatialReference;

namespace Tiles::Core::Geo {

/**
 * Unified coordinate conversion utility class.
 * 
 * Provides static methods for coordinate conversions and instance methods
 * for coordinate system specific conversions.
 */
class CoordinateConverter {
public:
    // ========== WGS84 Constants ==========
    static constexpr double WGS84_A = 6378137.0;                    // Semi-major axis (m)
    static constexpr double WGS84_F = 1.0 / 298.257223563;         // Flattening
    static constexpr double WGS84_E2 = WGS84_F * (2.0 - WGS84_F);  // First eccentricity squared
    static constexpr double WGS84_B = WGS84_A * (1.0 - WGS84_F);   // Semi-minor axis (m)
    static constexpr double WGS84_EP2 = (WGS84_A * WGS84_A - WGS84_B * WGS84_B) / (WGS84_B * WGS84_B);  // Second eccentricity squared
    
    static constexpr double PI = 3.14159265358979323846;
    static constexpr double DEG_TO_RAD = PI / 180.0;
    static constexpr double RAD_TO_DEG = 180.0 / PI;

    // ========== Static Utility Methods ==========

    /**
     * Convert geographic coordinates (WGS84) to ECEF.
     * 
     * @param lon Longitude in degrees
     * @param lat Latitude in degrees
     * @param height Height in meters above ellipsoid
     * @return ECEF coordinates (X, Y, Z) in meters
     */
    static glm::dvec3 geographicToEcef(double lon, double lat, double height);

    /**
     * Convert ECEF coordinates to geographic (WGS84).
     * 
     * @param ecef ECEF coordinates (X, Y, Z) in meters
     * @param lon Output longitude in degrees
     * @param lat Output latitude in degrees
     * @param height Output height in meters
     */
    static void ecefToGeographic(const glm::dvec3& ecef, 
                                  double& lon, double& lat, double& height);

    /**
     * Calculate the ENU to ECEF transformation matrix.
     * 
     * @param lon Longitude of ENU origin in degrees
     * @param lat Latitude of ENU origin in degrees
     * @param height Height of ENU origin in meters
     * @return 4x4 transformation matrix (column-major)
     */
    static glm::dmat4 calcEnuToEcefMatrix(double lon, double lat, double height);

    /**
     * Convert projected coordinates to geographic coordinates.
     * Supports EPSG codes and WKT strings.
     * 
     * @param projCoord Projected coordinates (X, Y, Z)
     * @param srsDefinition SRS definition (EPSG:xxxx or WKT string)
     * @return Geographic coordinates (lon, lat, height) in degrees/meters
     */
    static glm::dvec3 projectedToGeographic(const glm::dvec3& projCoord,
                                            const std::string& srsDefinition);

    /**
     * Convert geographic coordinates to projected coordinates.
     * 
     * @param geoCoord Geographic coordinates (lon, lat, height)
     * @param srsDefinition Target SRS definition (EPSG:xxxx or WKT)
     * @return Projected coordinates (X, Y, Z)
     */
    static glm::dvec3 geographicToProjected(const glm::dvec3& geoCoord,
                                            const std::string& srsDefinition);

    /**
     * Convert geographic coordinates to local ENU coordinates.
     * Uses approximate conversion for small distances.
     * 
     * @param geoCoord Geographic coordinates (lon, lat, height)
     * @param centerGeo Center point in geographic coordinates
     * @return Local ENU coordinates relative to center
     */
    static glm::dvec3 geographicToLocalMeter(const glm::dvec3& geoCoord,
                                              const glm::dvec3& centerGeo);

    // ========== Instance Methods ==========

    /**
     * Create a coordinate converter between two coordinate systems.
     */
    CoordinateConverter(const CoordinateSystem& source, 
                        const CoordinateSystem& target);

    ~CoordinateConverter();

    /**
     * Convert a single point.
     */
    glm::dvec3 convert(const glm::dvec3& point) const;

    /**
     * Convert a batch of points.
     */
    std::vector<glm::dvec3> convertBatch(const std::vector<glm::dvec3>& points) const;

private:
    CoordinateSystem source_;
    CoordinateSystem target_;
    OGRCoordinateTransformation* ogrTransform_;
    bool ownsTransform_;
};

}
