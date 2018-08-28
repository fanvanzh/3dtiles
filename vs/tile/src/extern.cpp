#include "extern.h"

#include <cstdio>
#include <string>
#include <osgDB/ConvertUTF>
#include <osgDB/FileUtils>

bool mkdirs(const char* path) {
	return osgDB::makeDirectory(path);
}

bool write_file(const char* filename, const char* buf, unsigned long buf_len) {
	// ���ļ���ת system ����
	FILE * f = fopen(filename, "wb");
	if (!f) return false;
	fwrite(buf,1,buf_len,f);
	fclose(f);
	return true;
}

void log_error(const char* msg) {
	
}