#include <gdal/ogr_spatialref.h>
#include <gdal/ogrsf_frmts.h>

#include <cmath>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

#include "extern.h"

///////////////////////
static const double pi = std::acos(-1);

extern "C" bool epsg_convert(int insrs, double* val, char* path) {
    CPLSetConfigOption("GDAL_DATA", path);
    OGRSpatialReference inRs,outRs;
    inRs.importFromEPSG(insrs);
    outRs.importFromEPSG(4326);
    OGRCoordinateTransformation *poCT = OGRCreateCoordinateTransformation( &inRs, &outRs );
    if (poCT) {
        if (poCT->Transform( 1, val, val + 1)) {
            delete poCT;
            return true;
        }
        delete poCT;
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


std::vector<double> transfrom_xyz(double radian_x, double radian_y, double height_min){
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
    
    double dx = xn * height_min;
    double dy = yn * height_min;
    double dz = zn * height_min;

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
    return matrix;
}

extern "C" void transform_c(double center_x, double center_y, double height_min, double* ptr) {
    double radian_x = degree2rad( center_x );
    double radian_y = degree2rad( center_y );
    std::vector<double> v = transfrom_xyz(radian_x, radian_y, height_min);
    std::memcpy(ptr, v.data(), v.size() * 8);
}

bool write_tileset_box( 
    Transform* trans, Box& box,     
    double geometricError,
    const char* b3dm_file,
    const char* json_file) {

    std::vector<double> matrix;
    if (trans) {
        matrix = transfrom_xyz(trans->radian_x,trans->radian_y,trans->min_height);
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
                \"url\": \"%s\"}}}", geometricError, b3dm_file);

        json_txt += last_buf;

        bool ret = write_file(json_file, json_txt.data(), json_txt.size());
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
        const char* json_file) {
        std::vector<double> matrix;
        if (trans) {
            matrix = transfrom_xyz(trans->radian_x,trans->radian_y,trans->min_height);
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
                    \"url\": \"%s\"}}}", geometricError, b3dm_file);

            json_txt += last_buf;

            bool ret = write_file(json_file, json_txt.data(), json_txt.size());
            if (!ret) {
                LOG_E("write file %s fail", json_file);
            }
            return ret;
        }

/**
根据经纬度，生成tileset
*/
        bool write_tileset(
            double radian_x, double radian_y, 
            double tile_w, double tile_h, 
            double height_min, double height_max,
            double geometricError,
            const char* filename, const char* full_path
            ) {

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
                            \"url\": \"%s\"}}}", geometricError, filename);

                    json_txt += last_buf;

                    bool ret = write_file(full_path, json_txt.data(), json_txt.size());
                    if (!ret) {
                        LOG_E("write file %s fail", filename);
                    }
                    return ret;
                }