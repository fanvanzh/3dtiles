#pragma once
/* vcpkg path */
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>
#include <memory>

#include "glm/glm.hpp"

// GDAL OGRCoordinateTransformation custom deleter
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
    // Use smart pointer to manage GDAL coordinate transformation object lifecycle
    static std::unique_ptr<OGRCoordinateTransformation, OGRCTDeleter> pOgrCT;

    static double OriginX;

    static double OriginY;

    static double OriginZ;

    // For ENU systems: store the geographic origin separately
    static double GeoOriginLon;
    static double GeoOriginLat;
    static double GeoOriginHeight;

    // Flag to indicate if this is an ENU system
    static bool IsENU;

    static glm::dmat4 EcefToEnuMatrix;

    static glm::dmat4 CalcEnuToEcefMatrix(double lnt, double lat, double height_min);

    static glm::dvec3 CartographicToEcef(double lnt, double lat, double height);

    static void Init(OGRCoordinateTransformation *pOgrCT, double *Origin);

    static void SetGeographicOrigin(double lon, double lat, double height);
};