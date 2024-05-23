#pragma once
/* vcpkg path */
#ifdef _WIN32
    #include <ogr_spatialref.h>
    #include <ogrsf_frmts.h>
#else
    #include <gdal/ogr_spatialref.h>
    #include <gdal/ogrsf_frmts.h>
#endif

#include "glm/glm.hpp"

class GeoTransform
{
public:
    static OGRCoordinateTransformation *pOgrCT;

    static double OriginX;

    static double OriginY;

    static double OriginZ;

    static glm::dmat4 EcefToEnuMatrix;

    static glm::dmat4 CalcEnuToEcefMatrix(double lnt, double lat, double height_min);

    static glm::dvec3 CartographicToEcef(double lnt, double lat, double height);

    static void Init(OGRCoordinateTransformation *pOgrCT, double *Origin);
};