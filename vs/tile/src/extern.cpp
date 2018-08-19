#include "extern.h"

#include <cstdio>
#include <string>
#include <osgDB/ConvertUTF>
#include <osgDB/FileUtils>

bool mkdirs(const char* path) {
	return osgDB::makeDirectory(path);
}

bool write_file(const char* filename, const char* buf, unsigned long buf_len) {
	// 打开文件名转 system 编码
	std::string file_system = osgDB::convertStringFromUTF8toCurrentCodePage(filename);
	FILE * f = fopen(file_system.c_str(), "wb");
	if (!f) return false;
	fwrite(buf,1,buf_len,f);
	fclose(f);
	return true;
}

void log_error(const char* msg) {
	
}