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
    // Use inline thread_local to ensure each thread has its own coordinate transformation object
    // This avoids race conditions when multiple threads access pOgrCT concurrently
    // With inline initialization, no need to define in .cpp file
    static inline thread_local std::unique_ptr<OGRCoordinateTransformation, OGRCTDeleter> pOgrCT = nullptr;

    static inline thread_local double OriginX = 0.0;

    static inline thread_local double OriginY = 0.0;

    static inline thread_local double OriginZ = 0.0;

    // For ENU systems: store as geographic origin separately
    static inline thread_local double GeoOriginLon = 0.0;
    static inline thread_local double GeoOriginLat = 0.0;
    static inline thread_local double GeoOriginHeight = 0.0;

    // Flag to indicate if this is an ENU system
    static inline thread_local bool IsENU = false;

    static inline thread_local glm::dmat4 EcefToEnuMatrix = glm::dmat4(1);

    static glm::dmat4 CalcEnuToEcefMatrix(double lnt, double lat, double height_min);

    static glm::dvec3 CartographicToEcef(double lnt, double lat, double height);

    static void Init(OGRCoordinateTransformation *pOgrCT, double *Origin);

    static void SetGeographicOrigin(double lon, double lat, double height);
};