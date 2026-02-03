#pragma once

#include <glm/glm.hpp>
#include <string>
#include "coordinate_system.h"

namespace Tiles::Core::Geo {

class CoordinateTransformer {
public:
    static glm::dmat4 calcEnuToEcefMatrix(double lon, double lat, double height);

    static glm::dvec3 cartographicToEcef(double lon, double lat, double height);

    static glm::dmat4 createCenterOffsetMatrix(const glm::dvec3& center);

    static glm::dmat4 createYUpToZUpMatrix();

    static glm::dmat4 createUnitScaleMatrix(double fromScale, double toScale);

    static void transformYUpToZUp(std::vector<glm::vec3>& positions);

    static void transformUnitScale(std::vector<glm::vec3>& positions,
                                double fromScale, double toScale);

    static void centerPositions(std::vector<glm::vec3>& positions,
                              glm::dvec3& outCenter);

    static std::vector<double> serializeMatrix(const glm::dmat4& mat);

    static glm::dmat4 createTransformMatrix(const CoordinateSystem& sourceCs,
                                            const CoordinateSystem& targetCs,
                                            const glm::dvec3& modelCenter);

    static glm::dvec3 geographicToLocalMeter(const glm::dvec3& geoCoord,
                                              const glm::dvec3& centerGeo);

    static glm::dvec3 projectedToGeographic(const glm::dvec3& projCoord,
                                             const std::string& epsgCode);

    static glm::dmat4 createGeoToEcefMatrix(const CoordinateSystem& sourceCs,
                                             const CoordinateSystem& targetCs,
                                             const glm::dvec3& modelCenter);
};

}
