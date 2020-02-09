
/*
 * This file is part of SushiVM
 * Copyright (c) 2019-2020 Eqela Oy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <time.h>
#include <string.h>
#include "zip.h"
#include "unzip.h"
#include "lib_zip.h"

#if defined(SUSHI_SUPPORT_LINUX) || defined(SUSHI_SUPPORT_MACOS)
# define ZIP_VERSIONMADEBY 0x031e
#else
# define ZIP_VERSIONMADEBY 0x001e
#endif

int zip_write_open(lua_State* state)
{
	const char* file = luaL_checkstring(state, 2);
	if(file == NULL) {
		lua_pushnil(state);
		return 1;
	}
	zipFile zip = zipOpen64(file, APPEND_STATUS_CREATE);
	if(zip == NULL) {
		lua_pushnil(state);
		return 1;
	}
	void* rptr = lua_newuserdata(state, sizeof(zipFile));
	luaL_getmetatable(state, "_sushi_zip");
	lua_setmetatable(state, -2);
	memcpy(rptr, &zip, sizeof(zipFile));
	return 1;
}

int zip_write_start_file(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 2, "_sushi_zip");
	if(bzip == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
	zipFile zip;
	memcpy(&zip, bzip, sizeof(zipFile));
	if(zip == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
	const char* filename = luaL_checkstring(state, 3);
	if(filename == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
	long timestamp = (long)luaL_checknumber(state, 4);
	if(timestamp < 0) {
		timestamp = 0;
	}
	unsigned int fileAttributes = (unsigned int)luaL_checknumber(state, 5) & 0xffff;
	int large = (int)luaL_checknumber(state, 6);
	time_t tt = (time_t)timestamp;
#ifdef SUSHI_SUPPORT_WIN32
	struct tm* tmv = localtime(&tt);
#else
	struct tm tm;
	localtime_r(&tt, &tm);
	struct tm* tmv = &tm;
#endif
	zip_fileinfo zif;
	memset(&zif, 0, sizeof(zip_fileinfo));
	zif.tmz_date.tm_sec = tmv->tm_sec;
	zif.tmz_date.tm_min = tmv->tm_min;
	zif.tmz_date.tm_hour = tmv->tm_hour;
	zif.tmz_date.tm_mday = tmv->tm_mday;
	zif.tmz_date.tm_mon = tmv->tm_mon;
	zif.tmz_date.tm_year = tmv->tm_year;
	zif.dosDate = 0;
	zif.internal_fa = 0;
	zif.external_fa = fileAttributes << 16;
	if(zipOpenNewFileInZip4_64(zip, filename, &zif, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION, 0, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, NULL, 0, ZIP_VERSIONMADEBY, 0, large) != ZIP_OK) {
		lua_pushboolean(state, 0);
		return 1;
	}
	lua_pushboolean(state, 1);
	return 1;
}

int zip_write_to_file(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 2, "_sushi_zip");
	if(bzip == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
	zipFile zip;
	memcpy(&zip, bzip, sizeof(zipFile));
	if(zip == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
	void* ptr = luaL_checkudata(state, 3, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
	long sz = (long)luaL_checknumber(state, 4);
	if(sz < 0) {
		memcpy(&sz, ptr, sizeof(long));
	}
	ptr += sizeof(long);
	if(zipWriteInFileInZip(zip, ptr, sz) != ZIP_OK) {
		lua_pushboolean(state, 0);
		return 1;
	}
	lua_pushboolean(state, 1);
	return 1;
}

int zip_write_end_file(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 2, "_sushi_zip");
	if(bzip == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
	zipFile zip;
	memcpy(&zip, bzip, sizeof(zipFile));
	if(zip != NULL) {
		zipCloseFileInZip(zip);
	}
	return 0;
}

int zip_write_close_gc(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 1, "_sushi_zip");
	if(bzip == NULL) {
		return 0;
	}
	zipFile zip;
	memcpy(&zip, bzip, sizeof(zipFile));
	if(zip != NULL) {
		zipClose(zip, NULL);
	}
	memset(bzip, 0, sizeof(zipFile));
	return 0;
}

int zip_write_close(lua_State* state)
{
	lua_remove(state, 1);
	return zip_write_close_gc(state);
}

int zip_read_open(lua_State* state)
{
	const char* file = luaL_checkstring(state, 2);
	if(file == NULL) {
		lua_pushnil(state);
		return 1;
	}
	unzFile unz = unzOpen64(file);
	if(unz == NULL) {
		lua_pushnil(state);
		return 1;
	}
	void* rptr = lua_newuserdata(state, sizeof(unzFile));
	luaL_getmetatable(state, "_sushi_unzip");
	lua_setmetatable(state, -2);
	memcpy(rptr, &unz, sizeof(unzFile));
	return 1;
}

int zip_read_first(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 2, "_sushi_unzip");
	if(bzip == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	unzFile zip;
	memcpy(&zip, bzip, sizeof(unzFile));
	if(zip == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	if(unzGoToFirstFile(zip) != UNZ_OK) {
		lua_pushnumber(state, 0);
		return 1;
	}
	lua_pushnumber(state, 1);
	return 1;
}

int zip_read_next(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 2, "_sushi_unzip");
	if(bzip == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	unzFile zip;
	memcpy(&zip, bzip, sizeof(unzFile));
	if(zip == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	int r = unzGoToNextFile(zip);
	if(r == UNZ_END_OF_LIST_OF_FILE) {
		lua_pushnumber(state, 2);
		return 1;
	}
	if(r != UNZ_OK) {
		lua_pushnumber(state, 0);
		return 1;
	}
	lua_pushnumber(state, 1);
	return 1;
}

int zip_read_entry_info(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 2, "_sushi_unzip");
	if(bzip == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	unzFile zip;
	memcpy(&zip, bzip, sizeof(unzFile));
	if(zip == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	unz_file_info64 fi;
	char filename[65536];
	int r = unzGetCurrentFileInfo64(zip, &fi, filename, 65536, NULL, 0, NULL, 0);
	if(r != UNZ_OK) {
		lua_pushnil(state);
		return 1;
	}
	int mode = fi.external_fa >> 16 & 0xffff;
	lua_pushstring(state, filename);
	lua_pushnumber(state, fi.compressed_size);
	lua_pushnumber(state, fi.uncompressed_size);
	lua_pushnumber(state, mode);
	return 4;
}

int zip_read_open_file(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 2, "_sushi_unzip");
	if(bzip == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	unzFile zip;
	memcpy(&zip, bzip, sizeof(unzFile));
	if(zip == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	const char* filename = luaL_checkstring(state, 3);
	if(filename == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	if(unzLocateFile(zip, filename, 1) != UNZ_OK) {
		lua_pushnumber(state, 0);
		return 1;
	}
	if(unzOpenCurrentFile(zip) != UNZ_OK) {
		lua_pushnumber(state, 0);
		return 1;
	}
	lua_pushnumber(state, 1);
	return 1;
}

int zip_read_get_file_data(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 2, "_sushi_unzip");
	if(bzip == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	unzFile zip;
	memcpy(&zip, bzip, sizeof(unzFile));
	if(zip == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	void* ptr = luaL_checkudata(state, 3, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushboolean(state, -1);
		return 1;
	}
	long sz;
	memcpy(&sz, ptr, sizeof(long));
	ptr += sizeof(long);
	int r = unzReadCurrentFile(zip, ptr, sz);
	lua_pushnumber(state, r);
	return 1;
}

int zip_read_close_file(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 2, "_sushi_unzip");
	if(bzip == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	unzFile zip;
	memcpy(&zip, bzip, sizeof(unzFile));
	if(zip == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	if(unzCloseCurrentFile(zip) != UNZ_OK) {
		lua_pushnumber(state, 0);
		return 1;
	}
	lua_pushnumber(state, 1);
	return 1;
}

int zip_read_close_gc(lua_State* state)
{
	void* bzip = (void*)luaL_checkudata(state, 1, "_sushi_unzip");
	if(bzip == NULL) {
		return 0;
	}
	unzFile zip;
	memcpy(&zip, bzip, sizeof(unzFile));
	if(zip != NULL) {
		unzClose(zip);
	}
	memset(bzip, 0, sizeof(unzFile));
	return 0;
}

int zip_read_close(lua_State* state)
{
	lua_remove(state, 1);
	return zip_read_close_gc(state);
}

static const luaL_Reg funcs[] = {
	{ "write_open", zip_write_open },
	{ "write_start_file", zip_write_start_file },
	{ "write_to_file", zip_write_to_file },
	{ "write_end_file", zip_write_end_file },
	{ "write_close", zip_write_close },
	{ "read_open", zip_read_open },
	{ "read_first", zip_read_first },
	{ "read_next", zip_read_next },
	{ "read_entry_info", zip_read_entry_info },
	{ "read_open_file", zip_read_open_file },
	{ "read_get_file_data", zip_read_get_file_data },
	{ "read_close_file", zip_read_close_file },
	{ "read_close", zip_read_close },
	{ NULL, NULL }
};

void lib_zip_init(lua_State* state)
{
	// _sushi_zip type
	luaL_newmetatable(state, "_sushi_zip");
	lua_pushliteral(state, "__gc");
	lua_pushcfunction(state, zip_write_close_gc);
	lua_rawset(state, -3);
	lua_pop(state, 1);
	// _sushi_unzip type
	luaL_newmetatable(state, "_sushi_unzip");
	lua_pushliteral(state, "__gc");
	lua_pushcfunction(state, zip_read_close_gc);
	lua_rawset(state, -3);
	lua_pop(state, 1);
	// function table
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_zip");
}
