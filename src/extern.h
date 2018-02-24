/////////////////////////
// extern function impl by rust
extern "C" bool mkdirs(const char* path);
extern "C" bool write_file(const char* filename, const char* buf, unsigned long buf_len);
//// -- others 
extern bool write_tileset(
	double longti, double lati, 
	double tile_w, double tile_h, 
	double height_min, double height_max,
	double geometricError,
	const char* filename, const char* full_path
	) ;

extern double degree2rad(double val);

extern double lati_to_meter(double diff);

extern double longti_to_meter(double diff, double lati);

////////////////////////