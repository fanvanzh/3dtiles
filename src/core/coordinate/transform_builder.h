#pragma once

#include "coordinate_system.h"
#include <glm/glm.hpp>
#include <vector>

namespace Tiles::Core::Geo {

/**
 * Builder for creating transformation matrices for 3D Tiles.
 *
 * This class provides methods to build various transformation matrices
 * needed for converting between different coordinate systems and
 * for creating 3D Tiles compatible transforms.
 */
class TransformBuilder {
public:
    /**
     * Build ENU to ECEF transformation matrix.
     *
     * @param lon Longitude of ENU origin in degrees
     * @param lat Latitude of ENU origin in degrees
     * @param height Height of ENU origin in meters
     * @param localCenter Local center point to offset (optional)
     * @return 4x4 transformation matrix
     */
    static glm::dmat4 buildEnuToEcefTransform(double lon, double lat, double height,
                                               const glm::dvec3& localCenter = glm::dvec3(0));

    /**
     * Build Y-up to Z-up transformation matrix.
     * Used for converting from Y-up coordinate systems (like FBX) to Z-up (3D Tiles).
     */
    static glm::dmat4 buildYUpToZUpTransform();

    /**
     * Build Z-up to Y-up transformation matrix.
     */
    static glm::dmat4 buildZUpToYUpTransform();

    /**
     * Build center offset matrix.
     * Translates coordinates so that center becomes origin.
     */
    static glm::dmat4 buildCenterOffsetMatrix(const glm::dvec3& center);

    /**
     * Build unit scale transformation matrix.
     */
    static glm::dmat4 buildUnitScaleMatrix(double fromScale, double toScale);

    /**
     * Build complete tileset transform matrix.
     *
     * @param source Source coordinate system
     * @param modelCenter Model center point in source coordinates
     * @param localOffset Local offset to apply (optional)
     * @return Complete transformation matrix for tileset.json
     */
    static glm::dmat4 buildTilesetTransform(const CoordinateSystem& source,
                                            const glm::dvec3& modelCenter,
                                            const glm::dvec3& localOffset = glm::dvec3(0));

    /**
     * Validate a transformation matrix.
     * Checks for NaN, infinity, and basic matrix properties.
     */
    static bool validateTransform(const glm::dmat4& transform);

    /**
     * Check if matrix is approximately identity.
     */
    static bool isIdentity(const glm::dmat4& transform, double tolerance = 1e-10);

    /**
     * Serialize matrix to vector (for tileset.json).
     * Returns row-major order array of 16 doubles.
     */
    static std::vector<double> serializeMatrix(const glm::dmat4& mat);

    /**
     * Deserialize matrix from vector.
     */
    static glm::dmat4 deserializeMatrix(const std::vector<double>& data);

    // ========== Legacy API Compatibility Methods ==========
    // These methods provide backward compatibility with the old CoordinateTransformer class

    /**
     * Transform positions from Y-up to Z-up coordinate system.
     * @param positions Vector of positions to transform in-place
     */
    static void transformYUpToZUp(std::vector<glm::vec3>& positions);

    /**
     * Transform positions by unit scale.
     * @param positions Vector of positions to transform in-place
     * @param fromScale Source unit scale (e.g., 0.001 for millimeters)
     * @param toScale Target unit scale (e.g., 1.0 for meters)
     */
    static void transformUnitScale(std::vector<glm::vec3>& positions,
                                    double fromScale, double toScale);

    /**
     * Center positions around their centroid.
     * @param positions Vector of positions to center in-place
     * @param outCenter Output parameter receiving the center point
     */
    static void centerPositions(std::vector<glm::vec3>& positions,
                                 glm::dvec3& outCenter);
};

}
