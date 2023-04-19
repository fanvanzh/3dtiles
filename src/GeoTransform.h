#pragma once
#include <gdal/ogr_spatialref.h>
#include <gdal/ogrsf_frmts.h>

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