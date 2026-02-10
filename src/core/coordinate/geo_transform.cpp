#include "geo_transform.h"
#include <cstdio>
#include <ogrsf_frmts.h>

namespace Tiles::Core::Geo {

// ---- CoordinateSystem implementation (kept here to guarantee linkage) ----

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

// ---- end CoordinateSystem implementation ----

void GeoTransform::Init(OGRCoordinateTransformation *pOgrCT, double *Origin)
{
    GeoTransform::pOgrCT.reset(pOgrCT);
    GeoTransform::OriginX = Origin[0];
    GeoTransform::OriginY = Origin[1];
    GeoTransform::OriginZ = Origin[2];
    GeoTransform::IsENU = false;

    glm::dvec3 origin = { GeoTransform::OriginX, GeoTransform::OriginY, GeoTransform::OriginZ };
    glm::dvec3 origin_cartographic = origin;
    fprintf(stderr, "[GeoTransform] ENU origin: x=%.8f y=%.8f z=%.3f\n", origin.x, origin.y, origin.z);

    if (GeoTransform::pOgrCT)
    {
        GeoTransform::pOgrCT->Transform(1, &origin_cartographic.x, &origin_cartographic.y, &origin_cartographic.z);
    }

    fprintf(stderr, "[GeoTransform] Cartographic origin: lon=%.10f lat=%.10f h=%.3f\n",
            origin_cartographic.x, origin_cartographic.y, origin_cartographic.z);

    GeoTransform::GeoOriginLon = origin_cartographic.x;
    GeoTransform::GeoOriginLat = origin_cartographic.y;
    GeoTransform::GeoOriginHeight = origin_cartographic.z;

    glm::dmat4 EnuToEcefMatrix = CoordinateConverter::calcEnuToEcefMatrix(
        origin_cartographic.x, origin_cartographic.y, origin_cartographic.z);
    GeoTransform::EcefToEnuMatrix = glm::inverse(EnuToEcefMatrix);

    GeoTransform::GlobalInitialized_ = true;
}

void GeoTransform::EnsureThreadTransform()
{
    if (GeoTransform::pOgrCT) return;  // Already initialized on this thread
    if (!GeoTransform::GlobalInitialized_) return;  // Global state not set yet
    if (GeoTransform::IsENU) return;  // ENU: SRSOrigin is already baked into geometry, skip Correction

    OGRSpatialReference outRs;
    outRs.importFromEPSG(4326);
    outRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRSpatialReference inRs;
    if (GeoTransform::SourceEPSG_ > 0) {
        // Recreate transform from stored EPSG code
        inRs.importFromEPSG(GeoTransform::SourceEPSG_);
        inRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    } else if (!GeoTransform::SourceWKT_.empty()) {
        // Recreate transform from stored WKT
        inRs.importFromWkt(GeoTransform::SourceWKT_.c_str());
        inRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    } else {
        return;  // No source SRS info available
    }

    OGRCoordinateTransformation* poCT = OGRCreateCoordinateTransformation(&inRs, &outRs);
    if (poCT) {
        GeoTransform::pOgrCT.reset(poCT);
        fprintf(stderr, "[GeoTransform] Worker thread: created per-thread OGR transform\n");
    } else {
        fprintf(stderr, "[GeoTransform] Worker thread: FAILED to create OGR transform\n");
    }
}

void GeoTransform::SetGeographicOrigin(double lon, double lat, double height)
{
    GeoTransform::GeoOriginLon = lon;
    GeoTransform::GeoOriginLat = lat;
    GeoTransform::GeoOriginHeight = height;
    GeoTransform::IsENU = true;

    glm::dmat4 EnuToEcefMatrix = CoordinateConverter::calcEnuToEcefMatrix(lon, lat, height);
    GeoTransform::EcefToEnuMatrix = glm::inverse(EnuToEcefMatrix);

    fprintf(stderr, "[GeoTransform] Geographic origin set: lon=%.10f lat=%.10f h=%.3f\n", lon, lat, height);
}

void GeoTransform::InitFromSource(const OGRSpatialReference* spatialRef, const glm::dvec3& refPoint) {
    sourceCS = CoordinateSystem::fromShapefile(spatialRef, refPoint);
    targetCS.type = CoordinateType::CARTESIAN;
    targetCS.epsgCode = "EPSG:4978";
    targetCS.upAxis = UpAxis::Z_UP;
    targetCS.isMeterUnit = true;
}

glm::dvec3 GeoTransform::transformPoint(double x, double y, double z) {
    glm::dvec3 point(x, y, z);
    glm::dvec3 center = GeoTransform::sourceCS.getCenter();

    if (GeoTransform::sourceCS.isGeographic()) {
        return CoordinateConverter::geographicToLocalMeter(point, center);
    } else if (GeoTransform::sourceCS.isProjected()) {
        glm::dvec3 geo = CoordinateConverter::projectedToGeographic(
            point, GeoTransform::sourceCS.epsgCode);
        glm::dvec3 centerGeo = CoordinateConverter::projectedToGeographic(
            center, GeoTransform::sourceCS.epsgCode);
        return CoordinateConverter::geographicToLocalMeter(geo, centerGeo);
    }
    return point;
}

} // namespace Tiles::Core::Geo
