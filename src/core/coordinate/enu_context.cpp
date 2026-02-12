#include "enu_context.h"
#include "coordinate_converter.h"
#include <cmath>

namespace Tiles::Core::Geo {

ENUContext ENUContext::fromGeographic(double lon, double lat, double height) {
    ENUContext context;
    context.originLon = lon;
    context.originLat = lat;
    context.originHeight = height;

    // Calculate ECEF coordinates of origin
    context.originEcef = CoordinateConverter::geographicToEcef(lon, lat, height);

    // Calculate transformation matrix
    context.enuToEcefMatrix = CoordinateConverter::calcEnuToEcefMatrix(lon, lat, height);
    context.ecefToEnuMatrix = glm::inverse(context.enuToEcefMatrix);

    return context;
}

ENUContext ENUContext::fromEcef(const glm::dvec3& ecefOrigin) {
    ENUContext context;
    context.originEcef = ecefOrigin;

    // Convert ECEF to geographic
    CoordinateConverter::ecefToGeographic(ecefOrigin,
                                           context.originLon,
                                           context.originLat,
                                           context.originHeight);

    // Calculate transformation matrix
    context.enuToEcefMatrix = CoordinateConverter::calcEnuToEcefMatrix(
        context.originLon, context.originLat, context.originHeight);
    context.ecefToEnuMatrix = glm::inverse(context.enuToEcefMatrix);

    return context;
}

bool ENUContext::validate() const {
    // Check that matrices are valid (no NaN or Inf)
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            double val = enuToEcefMatrix[i][j];
            if (std::isnan(val) || std::isinf(val)) {
                return false;
            }
        }
    }

    // Check that matrices are inverses of each other (with relaxed tolerance)
    glm::dmat4 identity = ecefToEnuMatrix * enuToEcefMatrix;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            double expected = (i == j) ? 1.0 : 0.0;
            if (std::abs(identity[i][j] - expected) > 1e-9) {
                return false;
            }
        }
    }

    // Check ENU basis vectors are orthonormal (with relaxed tolerance)
    glm::dvec3 east(enuToEcefMatrix[0]);
    glm::dvec3 north(enuToEcefMatrix[1]);
    glm::dvec3 up(enuToEcefMatrix[2]);

    // Check orthogonality
    if (std::abs(glm::dot(east, north)) > 1e-9) return false;
    if (std::abs(glm::dot(east, up)) > 1e-9) return false;
    if (std::abs(glm::dot(north, up)) > 1e-9) return false;

    // Check unit length
    if (std::abs(glm::length(east) - 1.0) > 1e-9) return false;
    if (std::abs(glm::length(north) - 1.0) > 1e-9) return false;
    if (std::abs(glm::length(up) - 1.0) > 1e-9) return false;

    return true;
}

}
