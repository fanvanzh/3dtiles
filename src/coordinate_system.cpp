#include "coordinate_system.h"
#include <ogrsf_frmts.h>

namespace Tiles::Core::Geo {

CoordinateSystem CoordinateSystem::fromShapefile(const OGRSpatialReference* spatialRef, const glm::dvec3& refPoint) {
    CoordinateSystem cs;

    if (!spatialRef) {
        cs.epsgCode = "EPSG:4326";
        cs.type = CoordinateType::GEOGRAPHIC;
        cs.upAxis = UpAxis::Y_UP;
        cs.isMeterUnit = false;
        cs.center = refPoint;
        return cs;
    }

    const char* authorityCode = spatialRef->GetAuthorityCode(nullptr);
    if (authorityCode && strlen(authorityCode) > 0) {
        cs.epsgCode = std::string("EPSG:") + authorityCode;
    } else {
        cs.epsgCode = "UNKNOWN";
    }

    const char* authorityName = spatialRef->GetAuthorityName(nullptr);
    if (authorityName) {
        cs.name = std::string(authorityName) + ":" + (authorityCode ? authorityCode : "");
    }

    char* wkt = nullptr;
    if (spatialRef->exportToWkt(&wkt) == OGRERR_NONE && wkt) {
        cs.wkt = std::string(wkt);
        CPLFree(wkt);
    }

    if (spatialRef->IsGeographic()) {
        cs.type = CoordinateType::GEOGRAPHIC;
        cs.isMeterUnit = false;
    } else if (spatialRef->IsProjected()) {
        cs.type = CoordinateType::PROJECTED;
        cs.isMeterUnit = true;
    } else {
        cs.type = CoordinateType::CARTESIAN;
        cs.isMeterUnit = true;
    }

    cs.upAxis = UpAxis::Z_UP;

    double factor = spatialRef->GetLinearUnits();
    cs.unitFactor = factor;

    cs.center = refPoint;

    return cs;
}

}
