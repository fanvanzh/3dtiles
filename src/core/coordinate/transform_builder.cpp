#include "transform_builder.h"
#include "coordinate_converter.h"
#include <cmath>

namespace Tiles::Core::Geo {

glm::dmat4 TransformBuilder::buildEnuToEcefTransform(double lon, double lat, double height,
                                                       const glm::dvec3& localCenter) {
    // Get base ENU to ECEF matrix
    glm::dmat4 enuToEcef = CoordinateConverter::calcEnuToEcefMatrix(lon, lat, height);

    // If local center is provided, apply offset
    if (localCenter != glm::dvec3(0)) {
        glm::dmat4 offset = buildCenterOffsetMatrix(localCenter);
        enuToEcef = enuToEcef * offset;
    }

    return enuToEcef;
}

glm::dmat4 TransformBuilder::buildYUpToZUpTransform() {
    glm::dmat4 transform(1.0);
    // Y-up to Z-up: (x, y, z) -> (x, z, -y)
    transform[0] = glm::dvec4(1.0, 0.0, 0.0, 0.0);
    transform[1] = glm::dvec4(0.0, 0.0, 1.0, 0.0);
    transform[2] = glm::dvec4(0.0, -1.0, 0.0, 0.0);
    transform[3] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    return transform;
}

glm::dmat4 TransformBuilder::buildZUpToYUpTransform() {
    glm::dmat4 transform(1.0);
    // Z-up to Y-up: (x, y, z) -> (x, -z, y)
    transform[0] = glm::dvec4(1.0, 0.0, 0.0, 0.0);
    transform[1] = glm::dvec4(0.0, 0.0, -1.0, 0.0);
    transform[2] = glm::dvec4(0.0, 1.0, 0.0, 0.0);
    transform[3] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    return transform;
}

glm::dmat4 TransformBuilder::buildCenterOffsetMatrix(const glm::dvec3& center) {
    glm::dmat4 offset(1.0);
    offset[3] = glm::dvec4(-center.x, -center.y, -center.z, 1.0);
    return offset;
}

glm::dmat4 TransformBuilder::buildUnitScaleMatrix(double fromScale, double toScale) {
    if (toScale == 0.0) {
        return glm::dmat4(1.0);
    }

    // Convert from fromScale units to toScale units
    // e.g., from mm (0.001) to m (1.0): scale = 0.001 / 1.0 = 0.001
    double scale = fromScale / toScale;
    glm::dmat4 scaleMatrix(1.0);
    scaleMatrix[0][0] = scale;
    scaleMatrix[1][1] = scale;
    scaleMatrix[2][2] = scale;
    return scaleMatrix;
}

glm::dmat4 TransformBuilder::buildTilesetTransform(const CoordinateSystem& source,
                                                    const glm::dvec3& modelCenter,
                                                    const glm::dvec3& localOffset) {
    glm::dmat4 transform(1.0);

    // Step 1: Center offset
    glm::dmat4 centerOffset = buildCenterOffsetMatrix(localOffset);

    // Step 2: Handle up-axis conversion if needed
    glm::dmat4 upAxisTransform(1.0);
    if (source.upAxis == UpAxis::Y_UP) {
        upAxisTransform = buildYUpToZUpTransform();
    }

    // Step 3: Coordinate system transformation
    glm::dmat4 coordTransform(1.0);
    if (source.isGeographic()) {
        coordTransform = CoordinateConverter::calcEnuToEcefMatrix(
            modelCenter.x, modelCenter.y, modelCenter.z);
    } else if (source.isProjected()) {
        glm::dvec3 geoCenter = CoordinateConverter::projectedToGeographic(
            modelCenter, source.epsgCode);
        coordTransform = CoordinateConverter::calcEnuToEcefMatrix(
            geoCenter.x, geoCenter.y, geoCenter.z);
    } else if (source.isEcef()) {
        // For ECEF, we need to determine the geographic origin
        double lon, lat, height;
        CoordinateConverter::ecefToGeographic(modelCenter, lon, lat, height);
        coordTransform = CoordinateConverter::calcEnuToEcefMatrix(lon, lat, height);
    }
    // For LOCAL coordinate systems, coordTransform remains identity

    // Combine transformations: coord * upAxis * centerOffset
    transform = coordTransform * upAxisTransform * centerOffset;

    return transform;
}

bool TransformBuilder::validateTransform(const glm::dmat4& transform) {
    // Check for NaN or Inf
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            double val = transform[i][j];
            if (std::isnan(val) || std::isinf(val)) {
                return false;
            }
        }
    }

    // Check determinant is not zero (matrix is invertible)
    double det = glm::determinant(transform);
    if (std::abs(det) < 1e-10) {
        return false;
    }

    return true;
}

bool TransformBuilder::isIdentity(const glm::dmat4& transform, double tolerance) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            double expected = (i == j) ? 1.0 : 0.0;
            if (std::abs(transform[i][j] - expected) > tolerance) {
                return false;
            }
        }
    }
    return true;
}

std::vector<double> TransformBuilder::serializeMatrix(const glm::dmat4& mat) {
    std::vector<double> result(16);

    // Store in row-major order for tileset.json
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result[row * 4 + col] = mat[col][row];
        }
    }

    return result;
}

glm::dmat4 TransformBuilder::deserializeMatrix(const std::vector<double>& data) {
    if (data.size() != 16) {
        return glm::dmat4(1.0);
    }

    glm::dmat4 mat;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            mat[col][row] = data[row * 4 + col];
        }
    }

    return mat;
}

// ========== Legacy API Compatibility Methods ==========

void TransformBuilder::transformYUpToZUp(std::vector<glm::vec3>& positions) {
    glm::dmat4 yUpToZUp = buildYUpToZUpTransform();
    for (auto& pos : positions) {
        glm::dvec4 pos4(pos, 1.0);
        glm::dvec4 transformed = yUpToZUp * pos4;
        pos = glm::vec3(transformed);
    }
}

void TransformBuilder::transformUnitScale(std::vector<glm::vec3>& positions,
                                           double fromScale, double toScale) {
    double scale = fromScale / toScale;
    for (auto& pos : positions) {
        pos = pos * static_cast<float>(scale);
    }
}

void TransformBuilder::centerPositions(std::vector<glm::vec3>& positions,
                                        glm::dvec3& outCenter) {
    if (positions.empty()) {
        outCenter = glm::dvec3(0.0, 0.0, 0.0);
        return;
    }

    glm::dvec3 sum(0.0, 0.0, 0.0);
    for (const auto& pos : positions) {
        sum += glm::dvec3(pos);
    }
    outCenter = sum / static_cast<double>(positions.size());

    for (auto& pos : positions) {
        pos = pos - glm::vec3(outCenter);
    }
}

}
