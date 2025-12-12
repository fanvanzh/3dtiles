#pragma once
#include <fmt/printf.h>
#include <spdlog/spdlog.h>

/////////////////////////
// extern function impl by rust
extern "C" bool mkdirs(const char* path);
extern "C" bool write_file(const char* filename, const char* buf, unsigned long buf_len);

inline void log_printf_impl(spdlog::level::level_enum lvl, const char* format, ...) {
	char buf[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	spdlog::log(lvl, "{}", buf);
}

#define LOG_D(format, ...) \
	do { \
		log_printf_impl(spdlog::level::debug, format __VA_OPT__(,) __VA_ARGS__); \
	} while (0)

#define LOG_I(format, ...) \
	do { \
		log_printf_impl(spdlog::level::info, format __VA_OPT__(,) __VA_ARGS__); \
	} while (0)

#define LOG_W(format, ...) \
	do { \
		log_printf_impl(spdlog::level::warn, format __VA_OPT__(,) __VA_ARGS__); \
	} while (0)

#define LOG_E(format, ...) \
	do { \
		log_printf_impl(spdlog::level::err, format __VA_OPT__(,) __VA_ARGS__); \
	} while (0)

//// -- others
struct Transform
{
	double radian_x;
	double radian_y;
	double min_height;
};

struct Box
{
	double matrix[12];
};

struct Region
{
	double min_x;
	double min_y;
	double max_x;
	double max_y;
	double min_height;
	double max_height;
};

bool write_tileset_region(
	Transform* trans,
	Region& region,
	double geometricError,
	const char* b3dm_file,
	const char* json_file);

bool write_tileset_box(
	Transform* trans, Box& box,
	double geometricError,
	const char* b3dm_file,
	const char* json_file);

bool write_tileset(
	double longti, double lati,
	double tile_w, double tile_h,
	double height_min, double height_max,
	double geometricError,
	const char* filename, const char* full_path
	) ;

extern "C" {
	double degree2rad(double val);
	double lati_to_meter(double diff);
	double longti_to_meter(double diff, double lati);
	double meter_to_lati(double m);
	double meter_to_longti(double m, double lati);
}

////////////////////////