#include "coordinate_transformer.h"
#include "coordinate_system.h"
#include <ogrsf_frmts.h>
#include <glm/glm.hpp>
#include <cmath>
#include <vector>
#include <spdlog/spdlog.h>

namespace Tiles::Core::Geo {

glm::dvec3 CoordinateTransformer::cartographicToEcef(double lon, double lat, double height)
{
    const double pi = std::acos(-1.0);
    const double a = 6378137.0;
    const double f = 1.0 / 298.257223563;
    const double e2 = f * (2.0 - f);

    double lon_rad = lon * pi / 180.0;
    double phi = lat * pi / 180.0;

    double sinPhi = std::sin(phi), cosPhi = std::cos(phi);
    double sinLon = std::sin(lon_rad), cosLon = std::cos(lon_rad);

    double N = a / std::sqrt(1.0 - e2 * sinPhi * sinPhi);
    double x = (N + height) * cosPhi * cosLon;
    double y = (N + height) * cosPhi * sinLon;
    double z = (N * (1.0 - e2) + height) * sinPhi;

    return glm::dvec3(x, y, z);
}

glm::dmat4 CoordinateTransformer::calcEnuToEcefMatrix(double lon, double lat, double height)
{
    const double pi = std::acos(-1.0);
    const double a = 6378137.0;
    const double f = 1.0 / 298.257223563;
    const double e2 = f * (2.0 - f);

    double lon_rad = lon * pi / 180.0;
    double phi = lat * pi / 180.0;

    double sinPhi = std::sin(phi), cosPhi = std::cos(phi);
    double sinLon = std::sin(lon_rad), cosLon = std::cos(lon_rad);

    double N = a / std::sqrt(1.0 - e2 * sinPhi * sinPhi);
    double x0 = (N + height) * cosPhi * cosLon;
    double y0 = (N + height) * cosPhi * sinLon;
    double z0 = (N * (1.0 - e2) + height) * sinPhi;

    glm::dvec3 east(-sinLon,           cosLon,            0.0);
    glm::dvec3 north(-sinPhi * cosLon, -sinPhi * sinLon,  cosPhi);
    glm::dvec3 up(   cosPhi * cosLon,   cosPhi * sinLon,  sinPhi);

    glm::dmat4 T(1.0);
    T[0] = glm::dvec4(east,  0.0);
    T[1] = glm::dvec4(north, 0.0);
    T[2] = glm::dvec4(up,     0.0);
    T[3] = glm::dvec4(x0, y0, z0, 1.0);
    return T;
}

glm::dmat4 CoordinateTransformer::createCenterOffsetMatrix(const glm::dvec3& center)
{
    glm::dmat4 offset(1.0);
    offset[3] = glm::dvec4(-center.x, -center.y, -center.z, 1.0);
    return offset;
}

glm::dmat4 CoordinateTransformer::createYUpToZUpMatrix()
{
    glm::dmat4 transform(1.0);
    transform[0] = glm::dvec4(1.0, 0.0, 0.0, 0.0);
    transform[1] = glm::dvec4(0.0, 0.0, 1.0, 0.0);
    transform[2] = glm::dvec4(0.0, 1.0, 0.0, 0.0);
    transform[3] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    return transform;
}

glm::dmat4 CoordinateTransformer::createUnitScaleMatrix(double fromScale, double toScale)
{
    double scale = toScale / fromScale;
    glm::dmat4 scaleMatrix(1.0);
    scaleMatrix[0][0] = scale;
    scaleMatrix[1][1] = scale;
    scaleMatrix[2][2] = scale;
    return scaleMatrix;
}

glm::dvec3 CoordinateTransformer::geographicToLocalMeter(const glm::dvec3& geoCoord,
                                                          const glm::dvec3& centerGeo)
{
    const double pi = std::acos(-1.0);
    const double metersPerDegreeLat = 111320.0;
    double centerLatRad = centerGeo.y * pi / 180.0;
    double metersPerDegreeLon = 111320.0 * std::cos(centerLatRad);

    double dx = (geoCoord.x - centerGeo.x) * metersPerDegreeLon;
    double dy = (geoCoord.y - centerGeo.y) * metersPerDegreeLat;
    double dz = geoCoord.z;

    return glm::dvec3(dx, dy, dz);
}

glm::dvec3 CoordinateTransformer::projectedToGeographic(const glm::dvec3& projCoord,
                                                         const std::string& epsgCode)
{
    if (epsgCode.empty()) {
        SPDLOG_WARN("Empty EPSG code for projected coordinate conversion");
        return projCoord;
    }

    OGRSpatialReference srs;
    if (srs.SetFromUserInput(epsgCode.c_str()) != OGRERR_NONE) {
        SPDLOG_WARN("Failed to create spatial reference from EPSG: {}", epsgCode);
        return projCoord;
    }

    OGRSpatialReference* wgs84 = new OGRSpatialReference();
    wgs84->SetWellKnownGeogCS("WGS84");

    // Ensure proper axis order: X=East, Y=North for projected, Lon,Lat for geographic
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    wgs84->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRCoordinateTransformation* transform = OGRCreateCoordinateTransformation(&srs, wgs84);
    if (!transform) {
        SPDLOG_WARN("Failed to create coordinate transformation for EPSG: {}", epsgCode);
        delete wgs84;
        return projCoord;
    }

    double x = projCoord.x;
    double y = projCoord.y;
    double z = projCoord.z;

    transform->Transform(1, &x, &y, &z);

    OGRCoordinateTransformation::DestroyCT(transform);
    delete wgs84;

    return glm::dvec3(x, y, z);
}

glm::dmat4 CoordinateTransformer::createGeoToEcefMatrix(const CoordinateSystem& sourceCs,
                                                         const CoordinateSystem& targetCs,
                                                         const glm::dvec3& modelCenter)
{
    glm::dmat4 transform(1.0);

    glm::dmat4 offset = createCenterOffsetMatrix(modelCenter);

    glm::dmat4 yUpToZUp(1.0);
    if (sourceCs.upAxis == UpAxis::Y_UP && targetCs.upAxis == UpAxis::Z_UP) {
        yUpToZUp = createYUpToZUpMatrix();
    }

    glm::dmat4 enuToEcef(1.0);
    if (sourceCs.isGeographic()) {
        enuToEcef = calcEnuToEcefMatrix(modelCenter.x, modelCenter.y, modelCenter.z);
    } else if (sourceCs.isProjected()) {
        glm::dvec3 geoCoord = projectedToGeographic(modelCenter, sourceCs.epsgCode);
        enuToEcef = calcEnuToEcefMatrix(geoCoord.x, geoCoord.y, geoCoord.z);
    } else if (sourceCs.isCartesian()) {
        enuToEcef = calcEnuToEcefMatrix(modelCenter.x, modelCenter.y, modelCenter.z);
    }

    transform = enuToEcef * yUpToZUp * offset;

    return transform;
}

glm::dmat4 CoordinateTransformer::createTransformMatrix(const CoordinateSystem& sourceCs,
                                                         const CoordinateSystem& targetCs,
                                                         const glm::dvec3& modelCenter)
{
    glm::dmat4 transform(1.0);

    glm::dmat4 offset = createCenterOffsetMatrix(modelCenter);

    glm::dmat4 yUpToZUp(1.0);
    if (sourceCs.upAxis == UpAxis::Y_UP && targetCs.upAxis == UpAxis::Z_UP) {
        yUpToZUp = createYUpToZUpMatrix();
    }

    glm::dmat4 coordTransform(1.0);
    if (sourceCs.isLocal()) {
        coordTransform = glm::dmat4(1.0);
    } else if (sourceCs.isGeographic()) {
        coordTransform = calcEnuToEcefMatrix(modelCenter.x, modelCenter.y, modelCenter.z);
    } else if (sourceCs.isProjected()) {
        glm::dvec3 geoCoord = projectedToGeographic(modelCenter, sourceCs.epsgCode);
        coordTransform = calcEnuToEcefMatrix(geoCoord.x, geoCoord.y, geoCoord.z);
    } else if (sourceCs.isCartesian()) {
        coordTransform = calcEnuToEcefMatrix(modelCenter.x, modelCenter.y, modelCenter.z);
    }

    transform = coordTransform * yUpToZUp * offset;

    return transform;
}

void CoordinateTransformer::transformYUpToZUp(std::vector<glm::vec3>& positions)
{
    glm::dmat4 yUpToZUp = createYUpToZUpMatrix();
    for (auto& pos : positions) {
        glm::dvec4 pos4(pos, 1.0);
        glm::dvec4 transformed = yUpToZUp * pos4;
        pos = glm::vec3(transformed);
    }
}

void CoordinateTransformer::transformUnitScale(std::vector<glm::vec3>& positions,
                                                double fromScale, double toScale)
{
    double scale = toScale / fromScale;
    for (auto& pos : positions) {
        pos = pos * static_cast<float>(scale);
    }
}

void CoordinateTransformer::centerPositions(std::vector<glm::vec3>& positions,
                                             glm::dvec3& outCenter)
{
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

std::vector<double> CoordinateTransformer::serializeMatrix(const glm::dmat4& mat)
{
    std::vector<double> result(16);

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result[row * 4 + col] = mat[col][row];
        }
    }

    return result;
}

}
