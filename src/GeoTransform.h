#pragma once
/* vcpkg path */
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>
#include <memory>

#include "coordinate_system.h"

struct OGRCTDeleter
{
    void operator()(OGRCoordinateTransformation* pCT) const
    {
        if (pCT != nullptr)
        {
            OGRCoordinateTransformation::DestroyCT(pCT);
        }
    }
};

class GeoTransform
{
public:
    static inline thread_local std::unique_ptr<OGRCoordinateTransformation, OGRCTDeleter> pOgrCT = nullptr;

    static inline thread_local double OriginX = 0.0;
    static inline thread_local double OriginY = 0.0;
    static inline thread_local double OriginZ = 0.0;

    static inline thread_local double GeoOriginLon = 0.0;
    static inline thread_local double GeoOriginLat = 0.0;
    static inline thread_local double GeoOriginHeight = 0.0;

    static inline thread_local bool IsENU = false;
    static inline thread_local glm::dmat4 EcefToEnuMatrix = glm::dmat4(1);

    static inline thread_local Tiles::Core::Geo::CoordinateSystem sourceCS;
    static inline thread_local Tiles::Core::Geo::CoordinateSystem targetCS;

    static glm::dmat4 CalcEnuToEcefMatrix(double lnt, double lat, double height_min);
    static glm::dvec3 CartographicToEcef(double lnt, double lat, double height);
    static void Init(OGRCoordinateTransformation *pOgrCT, double *Origin);
    static void SetGeographicOrigin(double lon, double lat, double height);

    static void InitFromSource(const OGRSpatialReference* spatialRef, const glm::dvec3& refPoint);

    static void setCenterPoint(double center_x, double center_y, double center_z = 0.0) {
        sourceCS.center = glm::dvec3(center_x, center_y, center_z);
    }

    static glm::dvec3 transformPoint(double x, double y, double z);

    static const Tiles::Core::Geo::CoordinateSystem& getSourceCoordinateSystem() {
        return sourceCS;
    }
};
