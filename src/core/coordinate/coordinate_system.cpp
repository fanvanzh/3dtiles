#include "coordinate_system.h"
#include <ogrsf_frmts.h>

namespace Tiles::Core::Geo {

CoordinateSystem CoordinateSystem::fromShapefile(const OGRSpatialReference* spatialRef,
                                                  const glm::dvec3& refPoint) {
    CoordinateSystem cs;
    cs.center = refPoint;

    if (!spatialRef) {
        // Default to WGS84 if no spatial reference
        cs.epsgCode = "EPSG:4326";
        cs.type = CoordinateType::GEOGRAPHIC;
        cs.upAxis = UpAxis::Y_UP;
        cs.isMeterUnit = false;
        cs.datum = GeographicDatum::WGS84;
        return cs;
    }

    // Get EPSG code
    const char* authorityCode = spatialRef->GetAuthorityCode(nullptr);
    if (authorityCode && std::strlen(authorityCode) > 0) {
        cs.epsgCode = std::string("EPSG:") + authorityCode;
        cs.datum = detectDatum(cs.epsgCode);
    } else {
        cs.epsgCode = "UNKNOWN";
    }

    // Get authority name
    const char* authorityName = spatialRef->GetAuthorityName(nullptr);
    if (authorityName) {
        cs.name = std::string(authorityName) + ":" +
                 (authorityCode ? authorityCode : "");
    }

    // Export WKT
    char* wkt = nullptr;
    if (spatialRef->exportToWkt(&wkt) == OGRERR_NONE && wkt) {
        cs.wkt = std::string(wkt);
        CPLFree(wkt);
    }

    // Determine coordinate type
    if (spatialRef->IsGeographic()) {
        cs.type = CoordinateType::GEOGRAPHIC;
        cs.isMeterUnit = false;
    } else if (spatialRef->IsProjected()) {
        cs.type = CoordinateType::PROJECTED;
        cs.isMeterUnit = true;
    } else if (cs.epsgCode == "EPSG:4978") {
        cs.type = CoordinateType::ECEF;
        cs.isMeterUnit = true;
    } else {
        cs.type = CoordinateType::CARTESIAN;
        cs.isMeterUnit = true;
    }

    cs.upAxis = UpAxis::Z_UP;

    // Get linear units
    double factor = spatialRef->GetLinearUnits();
    cs.unitFactor = factor;

    return cs;
}

GeographicDatum CoordinateSystem::detectDatum(const std::string& epsgCode) {
    if (epsgCode == "EPSG:4326") {
        return GeographicDatum::WGS84;
    } else if (epsgCode == "EPSG:4490") {
        return GeographicDatum::CGCS2000;
    }

    // Check for CGCS2000 projected coordinate systems
    // EPSG:4548-4554 are CGCS2000 3-degree Gauss-Kruger
    // EPSG:4491-4506 are CGCS2000 6-degree Gauss-Kruger
    if (epsgCode.find("EPSG:454") == 0 || epsgCode.find("EPSG:449") == 0) {
        return GeographicDatum::CGCS2000;
    }

    return GeographicDatum::UNKNOWN;
}

}
