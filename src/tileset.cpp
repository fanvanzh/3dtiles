
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

///////////////////////
extern "C" bool write_file(const char* filename, const char* buf, unsigned long buf_len);


///////////////////////

double lati_to_meter(double diff) {
	return diff / 0.000000157891;
}

double longti_to_meter(double diff, double lati) {
	return diff / (0.000000156785 * std::cos(lati));
}

double meter_to_lati (double m) {
	return m * 0.000000157891;
}

double meter_to_longti(double m, double lati) {
	return m * 0.000000156785 * std::cos(lati);
}

/**
根据经纬度，生成tileset
*/
bool write_tileset(double longti, double lati, 
	double tile_w, double tile_h, 
	double height_min, double height_max,
	const char* filename, const char* full_path
	) {

	double ellipsod_a = 40680631590769;
	double ellipsod_b = 40680631590769;
	double ellipsod_c = 40408299984661.4;

	const double pi = std::acos(-1);
	double radian_x = longti / 180.0 * pi;
	double radian_y = lati / 180.0 * pi;

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
		radian_x - (tile_w / 2) * 0.000000156785 * std::cos(radian_y),
		radian_y - tile_h * 0.000000157891,
		radian_x + (tile_w / 2) * 0.000000156785 * std::cos(radian_y),
		radian_y + tile_h * 0.000000157891,
		height_min,
		height_max
	};

	double geometricError = std::max(tile_w, tile_h);
	geometricError /= 10.0;

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
    	printf("write file %s fail\n", filename);
    }
    return ret;
}