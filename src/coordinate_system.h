#pragma once

#include <glm/glm.hpp>
#include <string>
#include <cstring>
#include <optional>
#include <fmt/format.h>

struct OGRSpatialReference;

namespace Tiles::Core::Geo {

enum class CoordinateType {
    CARTESIAN,
    GEOGRAPHIC,
    PROJECTED,
    LOCAL
};

enum class UpAxis {
    X_UP,
    Y_UP,
    Z_UP
};

struct CoordinateSystem {
    CoordinateType type = CoordinateType::CARTESIAN;
    std::string epsgCode;
    UpAxis upAxis = UpAxis::Z_UP;
    std::string name;
    std::string wkt;
    double unitFactor = 1.0;
    bool isMeterUnit = true;
    std::optional<glm::dvec3> originalMin;
    std::optional<glm::dvec3> originalMax;
    std::optional<glm::dvec3> center;  // Reference center point for local coordinate transformations

    bool isGeographic() const { return type == CoordinateType::GEOGRAPHIC; }
    bool isProjected() const { return type == CoordinateType::PROJECTED; }
    bool isCartesian() const { return type == CoordinateType::CARTESIAN; }
    bool isLocal() const { return type == CoordinateType::LOCAL; }
    bool isEcef() const { return epsgCode == "EPSG:4978"; }
    bool isWgs84() const { return epsgCode == "EPSG:4326"; }
    bool isWebMercator() const { return epsgCode == "EPSG:3857"; }

    bool valid() const { return !epsgCode.empty() || type == CoordinateType::LOCAL; }

    void setOriginalBounds(const glm::dvec3& min, const glm::dvec3& max) {
        originalMin = min;
        originalMax = max;
    }

    glm::dvec3 getCenter() const {
        if (center) {
            return *center;
        }
        if (originalMin && originalMax) {
            return (*originalMin + *originalMax) * 0.5;
        }
        return glm::dvec3(0.0);
    }

    static CoordinateSystem fromShapefile(const OGRSpatialReference* spatialRef, const glm::dvec3& refPoint);

    std::string toString() const {
        std::string typeStr;
        switch (type) {
            case CoordinateType::CARTESIAN: typeStr = "CARTESIAN"; break;
            case CoordinateType::GEOGRAPHIC: typeStr = "GEOGRAPHIC"; break;
            case CoordinateType::PROJECTED: typeStr = "PROJECTED"; break;
            case CoordinateType::LOCAL: typeStr = "LOCAL"; break;
        }
        std::string axisStr;
        switch (upAxis) {
            case UpAxis::X_UP: axisStr = "X_UP"; break;
            case UpAxis::Y_UP: axisStr = "Y_UP"; break;
            case UpAxis::Z_UP: axisStr = "Z_UP"; break;
        }
        auto center = getCenter();
        return fmt::format("[type={}, epsg={}, upAxis={}, center=({},{},{})]",
                          typeStr, epsgCode, axisStr,
                          center.x, center.y, center.z);
    }
};

}
