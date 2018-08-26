#pragma once
#ifdef WIN32
#define DLL_API __declspec(dllexport)
#else
#define DLL_API __declspec(dllimport)
#endif

extern "C" {
	DLL_API void* osgb23dtile_path(const char* in_path, const char* out_path, double *box, int* len, int max_lvl);

	DLL_API void free_buffer(void* buf);

	DLL_API void transform_c(double center_x, double center_y, double height_min, double* ptr);
}
