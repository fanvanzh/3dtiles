#pragma once

#include <glm/glm.hpp>
#include <cmath>

namespace Tiles::Core::Geo {

/**
 * ENU (East-North-Up) coordinate system context.
 * 
 * ENU is a local tangent plane coordinate system where:
 * - X axis points East
 * - Y axis points North  
 * - Z axis points Up (away from Earth's center)
 * 
 * The origin is at a specific geographic location (lon, lat, height).
 */
struct ENUContext {
    glm::dvec3 originEcef;          // ENU origin in ECEF coordinates
    glm::dmat4 enuToEcefMatrix;     // Transformation from ENU to ECEF
    glm::dmat4 ecefToEnuMatrix;     // Transformation from ECEF to ENU
    double originLon;               // Origin longitude in degrees
    double originLat;               // Origin latitude in degrees
    double originHeight;            // Origin height in meters

    ENUContext() : originEcef(0.0), originLon(0.0), originLat(0.0), originHeight(0.0) {
        enuToEcefMatrix = glm::dmat4(1.0);
        ecefToEnuMatrix = glm::dmat4(1.0);
    }

    /**
     * Create ENU context from geographic coordinates (WGS84).
     * 
     * @param lon Longitude in degrees
     * @param lat Latitude in degrees
     * @param height Height in meters above ellipsoid
     * @return ENUContext initialized at the specified location
     */
    static ENUContext fromGeographic(double lon, double lat, double height);

    /**
     * Create ENU context from ECEF coordinates.
     * 
     * @param ecefOrigin Origin in ECEF coordinates
     * @return ENUContext initialized at the specified ECEF location
     */
    static ENUContext fromEcef(const glm::dvec3& ecefOrigin);

    /**
     * Validate the ENU context.
     * Checks that transformation matrices are valid and consistent.
     * 
     * @return true if valid, false otherwise
     */
    bool validate() const;

    /**
     * Transform a point from ENU to ECEF coordinates.
     */
    glm::dvec3 enuToEcef(const glm::dvec3& enu) const {
        return glm::dvec3(enuToEcefMatrix * glm::dvec4(enu, 1.0));
    }

    /**
     * Transform a point from ECEF to ENU coordinates.
     */
    glm::dvec3 ecefToEnu(const glm::dvec3& ecef) const {
        return glm::dvec3(ecefToEnuMatrix * glm::dvec4(ecef, 1.0));
    }
};

}
