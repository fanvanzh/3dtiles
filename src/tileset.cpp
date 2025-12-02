#include <cmath>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>

/* vcpkg path */
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>

#include "extern.h"
#include "GeoTransform.h"

///////////////////////
static const double pi = std::acos(-1);

extern "C" bool
epsg_convert(int insrs, double* val, char* gdal_data, char *proj_lib) {
    CPLSetConfigOption("GDAL_DATA", gdal_data);
    CPLSetConfigOption("PROJ_LIB", proj_lib);
    OGRSpatialReference inRs,outRs;
    OGRErr inErr = inRs.importFromEPSG(insrs);
    if (inErr != OGRERR_NONE) {
        LOG_E("importFromEPSG(%d) failed, err_code=%d", insrs, inErr);
        return false;
    }
    inRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    OGRErr outErr = outRs.importFromEPSG(4326);
    if (outErr != OGRERR_NONE) {
        LOG_E("importFromEPSG(4326) failed, err_code=%d", outErr);
        return false;
    }
    outRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    fprintf(stderr, "[SRS] EPSG:%d -> EPSG:4326 (axis=traditional)\n", insrs);
    fprintf(stderr, "[Origin ENU] x=%.6f y=%.6f z=%.3f\n", val[0], val[1], val[2]);
    OGRCoordinateTransformation *poCT = OGRCreateCoordinateTransformation(&inRs, &outRs );
    GeoTransform::Init(poCT, val);
    if (poCT) {
        if (poCT->Transform( 1, val, val + 1)) {
            fprintf(stderr, "[Origin LLA] lon=%.10f lat=%.10f\n", val[0], val[1]);
            // poCT will be used later so don't delete it
            // delete poCT;
            return true;
        }
        // delete poCT;
    }
    return false;
}

extern "C" bool
enu_init(double lon, double lat, double* origin_enu, char* gdal_data, char* proj_lib) {
    CPLSetConfigOption("GDAL_DATA", gdal_data);
    CPLSetConfigOption("PROJ_LIB", proj_lib);

    // For ENU systems, the origin_enu contains local ENU offsets
    // We need to set up GeoTransform for the geometry correction to work
    fprintf(stderr, "[SRS] ENU:%.7f,%.7f (origin offset: %.3f, %.3f, %.3f)\n",
            lat, lon, origin_enu[0], origin_enu[1], origin_enu[2]);
    fprintf(stderr, "[Origin ENU] x=%.6f y=%.6f z=%.3f\n", origin_enu[0], origin_enu[1], origin_enu[2]);

    // Create an identity transform (no actual projection needed for ENU)
    // The origin is already in the correct coordinate system
    OGRSpatialReference inRs, outRs;
    outRs.importFromEPSG(4326);
    outRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    // For ENU, we create a trivial transform that just passes through
    // This allows the geometry correction logic in InfoVisitor to work
    OGRCoordinateTransformation *poCT = OGRCreateCoordinateTransformation(&outRs, &outRs);

    // IMPORTANT: For ENU, pass the ENU offsets to GeoTransform::Init
    // These will be used in InfoVisitor::Correction to offset local vertices
    // Store: [enu_offset_x, enu_offset_y, enu_offset_z]
    GeoTransform::Init(poCT, origin_enu);

    // Store the geographic origin separately for ENU->ECEF calculations
    GeoTransform::SetGeographicOrigin(lon, lat, 0.0);

    fprintf(stderr, "[Origin LLA] lon=%.10f lat=%.10f\n", lon, lat);
    return poCT != nullptr;
}

extern "C" bool
wkt_convert(char* wkt, double* val, char* path) {
    CPLSetConfigOption("GDAL_DATA", path);
    OGRSpatialReference inRs,outRs;
    inRs.importFromWkt(&wkt);
    outRs.importFromEPSG(4326);
    inRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    outRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    fprintf(stderr, "[SRS] WKT -> EPSG:4326 (axis=traditional)\n");
    fprintf(stderr, "[Origin ENU] x=%.6f y=%.6f z=%.3f\n", val[0], val[1], val[2]);
    OGRCoordinateTransformation *poCT = OGRCreateCoordinateTransformation( &inRs, &outRs );
    GeoTransform::Init(poCT, val);
    if (poCT) {
        if (poCT->Transform( 1, val, val + 1)) {
            fprintf(stderr, "[Origin LLA] lon=%.10f lat=%.10f\n", val[0], val[1]);
            // delete poCT;
            return true;
        }
        // delete poCT;
    }
    return false;
}

extern "C"
{
	double degree2rad(double val) {
		return val * pi / 180.0;
	}
	double lati_to_meter(double diff) {
		return diff / 0.000000157891;
	}

	double longti_to_meter(double diff, double lati) {
		return diff / 0.000000156785 * std::cos(lati);
	}

	double meter_to_lati(double m) {
		return m * 0.000000157891;
	}

	double meter_to_longti(double m, double lati) {
		return m * 0.000000156785 / std::cos(lati);
	}
}

std::vector<double>
transfrom_xyz(double lon_deg, double lat_deg, double height_min){
    // WGS84 parameters
    const double pi = std::acos(-1.0);
    const double a = 6378137.0;
    const double f = 1.0 / 298.257223563;
    const double e2 = f * (2.0 - f);

    // Convert degrees to radians
    double lon = lon_deg * pi / 180.0;
    double lat = lat_deg * pi / 180.0;

    double sinLat = std::sin(lat), cosLat = std::cos(lat);
    double sinLon = std::sin(lon), cosLon = std::cos(lon);

    double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    double x0 = (N + height_min) * cosLat * cosLon;
    double y0 = (N + height_min) * cosLat * sinLon;
    double z0 = (N * (1.0 - e2) + height_min) * sinLat;

    // ENU basis vectors expressed in ECEF
    double east_x = -sinLon;
    double east_y =  cosLon;
    double east_z =  0.0;

    double north_x = -sinLat * cosLon;
    double north_y = -sinLat * sinLon;
    double north_z =  cosLat;

    double up_x =  cosLat * cosLon;
    double up_y =  cosLat * sinLon;
    double up_z =  sinLat;

    // Column-major 4x4 ENU->ECEF transform
    std::vector<double> matrix = {
        east_x,  east_y,  east_z,  0.0,
        north_x, north_y, north_z, 0.0,
        up_x,    up_y,    up_z,    0.0,
        x0,      y0,      z0,      1.0
    };
    return matrix;
}

extern "C" void
transform_c(double center_x, double center_y, double height_min, double* ptr) {
    // transfrom_xyz now expects degrees directly
    std::vector<double> v = transfrom_xyz(center_x, center_y, height_min);
    // Log ECEF translation from ENU->ECEF transform
    fprintf(stderr, "[transform_c] lon=%.10f lat=%.10f h=%.3f -> ECEF translation: x=%.10f y=%.10f z=%.10f\n",
            center_x, center_y, height_min, v[12], v[13], v[14]);
    std::memcpy(ptr, v.data(), v.size() * 8);
}

// New function to handle ENU offsets in transform matrix
extern "C" void
transform_c_with_enu_offset(double center_x, double center_y, double height_min,
                           double enu_offset_x, double enu_offset_y, double enu_offset_z,
                           double* ptr) {
    // Calculate base transform matrix (transfrom_xyz now expects degrees)
    std::vector<double> v = transfrom_xyz(center_x, center_y, height_min);
    fprintf(stderr, "[transform_c_with_enu_offset] Base ECEF at lon=%.10f lat=%.10f h=%.3f: x=%.10f y=%.10f z=%.10f\n",
            center_x, center_y, height_min, v[12], v[13], v[14]);

    // Apply ENU offset to the translation components (indices 12, 13, 14)
    // Convert ENU offset to ECEF offset and add to translation
    const double pi = std::acos(-1.0);
    double lat_rad = center_y * pi / 180.0;
    double lon_rad = center_x * pi / 180.0;

    double sinLat = std::sin(lat_rad);
    double cosLat = std::cos(lat_rad);
    double sinLon = std::sin(lon_rad);
    double cosLon = std::cos(lon_rad);

    // ENU to ECEF transformation
    double ecef_offset_x = -sinLon * enu_offset_x - sinLat * cosLon * enu_offset_y + cosLat * cosLon * enu_offset_z;
    double ecef_offset_y =  cosLon * enu_offset_x - sinLat * sinLon * enu_offset_y + cosLat * sinLon * enu_offset_z;
    double ecef_offset_z =  cosLat * enu_offset_y + sinLat * enu_offset_z;

    fprintf(stderr, "[transform_c_with_enu_offset] ENU offset (%.3f, %.3f, %.3f) -> ECEF offset (%.10f, %.10f, %.10f)\n",
            enu_offset_x, enu_offset_y, enu_offset_z, ecef_offset_x, ecef_offset_y, ecef_offset_z);

    // Add ECEF offset to translation components
    v[12] += ecef_offset_x;
    v[13] += ecef_offset_y;
    v[14] += ecef_offset_z;

    fprintf(stderr, "[transform_c_with_enu_offset] Final ECEF translation: x=%.10f y=%.10f z=%.10f\n",
            v[12], v[13], v[14]);

    std::memcpy(ptr, v.data(), v.size() * 8);
}

bool
write_tileset_box(Transform* trans, Box& box, double geometricError,
                    const char* b3dm_file, const char* json_file) {
    std::vector<double> matrix;
    if (trans) {
        // Convert radians to degrees for transfrom_xyz
        double lon_deg = trans->radian_x * 180.0 / std::acos(-1.0);
        double lat_deg = trans->radian_y * 180.0 / std::acos(-1.0);
        matrix = transfrom_xyz(lon_deg, lat_deg, trans->min_height);
    }
    std::string json_txt = "{\"asset\": {\
        \"version\": \"0.0\",\
        \"gltfUpAxis\": \"Y\"\
    },\
    \"geometricError\":";
    json_txt += std::to_string(geometricError);
    json_txt += ",\"root\": {";
    std::string trans_str = "\"transform\": [";
    if (trans) {
        for (int i = 0; i < 15 ; i++) {
            trans_str += std::to_string(matrix[i]);
            trans_str += ",";
        }
        trans_str += "1],";
        json_txt += trans_str;
    }
    json_txt += "\"boundingVolume\": {\
        \"box\": [";
    for (int i = 0; i < 11 ; i++) {
        json_txt += std::to_string(box.matrix[i]);
        json_txt += ",";
    }
    json_txt += std::to_string(box.matrix[11]);

    char last_buf[512];
    sprintf(last_buf,"]},\"geometricError\": %f,\
        \"refine\": \"REPLACE\",\
        \"content\": {\
            \"uri\": \"%s\"}}}", geometricError, b3dm_file);

    json_txt += last_buf;

    bool ret = write_file(json_file, json_txt.data(), (unsigned long)json_txt.size());
    if (!ret) {
        LOG_E("write file %s fail", json_file);
    }
    return ret;
}

bool write_tileset_region(
    Transform* trans,
    Region& region,
    double geometricError,
    const char* b3dm_file,
    const char* json_file)
{
    std::vector<double> matrix;
    if (trans) {
        // Convert radians to degrees for transfrom_xyz
        double lon_deg = trans->radian_x * 180.0 / std::acos(-1.0);
        double lat_deg = trans->radian_y * 180.0 / std::acos(-1.0);
        matrix = transfrom_xyz(lon_deg, lat_deg, trans->min_height);
    }
    std::string json_txt = "{\"asset\": {\
        \"version\": \"0.0\",\
        \"gltfUpAxis\": \"Y\"\
    },\
    \"geometricError\":";
    json_txt += std::to_string(geometricError);
    json_txt += ",\"root\": {";
    std::string trans_str = "\"transform\": [";
    if (trans) {
        for (int i = 0; i < 15 ; i++) {
            trans_str += std::to_string(matrix[i]);
            trans_str += ",";
        }
        trans_str += "1],";
        json_txt += trans_str;
    }
    json_txt += "\"boundingVolume\": {\
    \"region\": [";
    double* pRegion = (double*)&region;
    for (int i = 0; i < 5 ; i++) {
        json_txt += std::to_string(pRegion[i]);
        json_txt += ",";
    }
    json_txt += std::to_string(pRegion[5]);

    char last_buf[512];
    sprintf(last_buf,"]},\"geometricError\": %f,\
        \"refine\": \"REPLACE\",\
        \"content\": {\
            \"uri\": \"%s\"}}}", geometricError, b3dm_file);

    json_txt += last_buf;

    bool ret = write_file(json_file, json_txt.data(), (unsigned long)json_txt.size());
    if (!ret) {
        LOG_E("write file %s fail", json_file);
    }
    return ret;
}

/***/
bool
write_tileset(double radian_x, double radian_y,
            double tile_w, double tile_h, double height_min, double height_max,
            double geometricError, const char* filename, const char* full_path) {
    double ellipsod_a = 40680631590769;
    double ellipsod_b = 40680631590769;
    double ellipsod_c = 40408299984661.4;

    const double pi = std::acos(-1);
    double xn = std::cos(radian_x) * std::cos(radian_y);
    double yn = std::sin(radian_x) * std::cos(radian_y);
    double zn = std::sin(radian_y);

    double x0 = ellipsod_a * xn;
    double y0 = ellipsod_b * yn;
    double z0 = ellipsod_c * zn;
    double gamma = std::sqrt(xn*x0 + yn*y0 + zn*z0);
    double px = x0 / gamma;
    double py = y0 / gamma;
    double pz = z0 / gamma;
    double dx = x0 * height_min;
    double dy = y0 * height_min;
    double dz = z0 * height_min;

    std::vector<double> east_mat = {-y0,x0,0};
    std::vector<double> north_mat = {
        (y0*east_mat[2] - east_mat[1]*z0),
        (z0*east_mat[0] - east_mat[2]*x0),
        (x0*east_mat[1] - east_mat[0]*y0)
    };
    double east_normal = std::sqrt(
        east_mat[0]*east_mat[0] +
        east_mat[1]*east_mat[1] +
        east_mat[2]*east_mat[2]
        );
    double north_normal = std::sqrt(
        north_mat[0]*north_mat[0] +
        north_mat[1]*north_mat[1] +
        north_mat[2]*north_mat[2]
        );

    std::vector<double> matrix = {
        east_mat[0] / east_normal,
        east_mat[1] / east_normal,
        east_mat[2] / east_normal,
        0,
        north_mat[0] / north_normal,
        north_mat[1] / north_normal,
        north_mat[2] / north_normal,
        0,
        xn,
        yn,
        zn,
        0,
        px + dx,
        py + dy,
        pz + dz,
        1
    };

    std::vector<double> region = {
        radian_x - meter_to_longti(tile_w / 2, radian_y),
        radian_y - meter_to_lati(tile_h / 2),
        radian_x + meter_to_longti(tile_w / 2, radian_y),
        radian_y + meter_to_lati(tile_h / 2),
        0,
        height_max
    };

    std::string json_txt = "{\"asset\": {\
        \"version\": \"0.0\",\
        \"gltfUpAxis\": \"Y\"\
    },\
    \"geometricError\":";
    json_txt += std::to_string(geometricError);
    json_txt += ",\"root\": {\
        \"transform\": [";
    for (int i = 0; i < 15 ; i++) {
        json_txt += std::to_string(matrix[i]);
        json_txt += ",";
    }
    json_txt += "1],\
    \"boundingVolume\": {\
    \"region\": [";
    for (int i = 0; i < 5 ; i++) {
        json_txt += std::to_string(region[i]);
        json_txt += ",";
    }
    json_txt += std::to_string(region[5]);

    char last_buf[512];
    sprintf(last_buf,"]},\"geometricError\": %f,\
        \"refine\": \"REPLACE\",\
        \"content\": {\
        \"uri\": \"%s\"}}}", geometricError, filename);

    json_txt += last_buf;

    bool ret = write_file(full_path, json_txt.data(), (unsigned long)json_txt.size());
    if (!ret) {
        LOG_E("write file %s fail", filename);
    }
    return ret;
}