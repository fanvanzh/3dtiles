#pragma once

/* vcpkg path */
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include "coordinate_system.h"
#include "coordinate_converter.h"
#include "transform_builder.h"

namespace Tiles::Core::Geo {

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

/**
 * GeoTransform - Global coordinate transformation state manager.
 *
 * This class provides thread-local storage for coordinate transformation state,
 * including the coordinate transformation object, origin points, and ENU matrices.
 *
 * Note: This is a legacy compatibility class. New code should use CoordinateConverter
 * and TransformBuilder directly.
 */
class GeoTransform
{
public:
    static inline thread_local std::unique_ptr<OGRCoordinateTransformation, OGRCTDeleter> pOgrCT = nullptr;

    // Shared state: set once on main thread before parallel processing, read on worker threads
    static inline double OriginX = 0.0;
    static inline double OriginY = 0.0;
    static inline double OriginZ = 0.0;

    static inline double GeoOriginLon = 0.0;
    static inline double GeoOriginLat = 0.0;
    static inline double GeoOriginHeight = 0.0;

    static inline bool IsENU = false;
    static inline glm::dmat4 EcefToEnuMatrix = glm::dmat4(1);

    // Source SRS info for lazy per-thread OGR transform creation
    static inline int SourceEPSG_ = 0;
    static inline std::string SourceWKT_;
    static inline bool GlobalInitialized_ = false;

    // Ensure the current thread has a valid pOgrCT (lazy init for worker threads)
    static void EnsureThreadTransform();

    static inline thread_local CoordinateSystem sourceCS;
    static inline thread_local CoordinateSystem targetCS;

    static glm::dmat4 CalcEnuToEcefMatrix(double lnt, double lat, double height_min)
    {
        return CoordinateConverter::calcEnuToEcefMatrix(lnt, lat, height_min);
    }

    static glm::dvec3 CartographicToEcef(double lnt, double lat, double height)
    {
        return CoordinateConverter::geographicToEcef(lnt, lat, height);
    }

    static void Init(OGRCoordinateTransformation *pOgrCT, double *Origin);

    static void SetGeographicOrigin(double lon, double lat, double height);

    static void InitFromSource(const OGRSpatialReference* spatialRef, const glm::dvec3& refPoint);

    static void setCenterPoint(double center_x, double center_y, double center_z = 0.0) {
        sourceCS.center = glm::dvec3(center_x, center_y, center_z);
    }

    static glm::dvec3 transformPoint(double x, double y, double z);

    static const CoordinateSystem& getSourceCoordinateSystem() {
        return sourceCS;
    }
};

} // namespace Tiles::Core::Geo

// Global alias for backward compatibility
using GeoTransform = Tiles::Core::Geo::GeoTransform;
