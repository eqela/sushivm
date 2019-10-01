
/*
 * This file is part of SushiVM
 * Copyright (c) 2019 Eqela Oy
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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include "lib_io.h"

#ifdef SUSHI_SUPPORT_WIN32
#include <windows.h>
#include <direct.h>
#include <dirent.h>
#define NAME_MAX FILENAME_MAX
#endif

static int get_file_info(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		return 0;
	}
	struct stat st;
	if(stat(path, &st) != 0) {
		return 0;
	}
	lua_pushnumber(state, st.st_size);
#ifdef SUSHI_SUPPORT_WIN32
	lua_pushnumber(state, st.st_ctime);
	lua_pushnumber(state, st.st_atime);
	lua_pushnumber(state, st.st_mtime);
#endif
#ifdef SUSHI_SUPPORT_MACOS
	lua_pushnumber(state, st.st_ctime);
	lua_pushnumber(state, st.st_atime);
	lua_pushnumber(state, st.st_mtime);
#endif
#ifdef SUSHI_SUPPORT_LINUX
	lua_pushnumber(state, st.st_ctim.tv_sec);
	lua_pushnumber(state, st.st_atim.tv_sec);
	lua_pushnumber(state, st.st_mtim.tv_sec);
#endif
	lua_pushnumber(state, st.st_uid);
	lua_pushnumber(state, st.st_gid);
	lua_pushnumber(state, st.st_mode);
	return 7;
}

static int set_file_mode(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
	int mode = luaL_checknumber(state, 3);
	if(chmod(path, (mode_t)mode) != 0) {
		lua_pushboolean(state, 0);
		return 1;
	}
	lua_pushboolean(state, 1);
	return 1;
}

static int get_real_path(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		return 0;
	}
	char* rpath = sushi_get_real_path(path);
	if(rpath != NULL) {
		lua_pushstring(state, rpath);
		free(rpath);
	}
	else {
		lua_pushnil(state);
	}
	return 1;
}

static int create_directory(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	int r;
#if defined(SUSHI_SUPPORT_WIN32)
	r = _mkdir(path);
#else
	r = mkdir(path, 0755);
#endif
	if(r == 0) {
		lua_pushnumber(state, 1);
	}
	else {
		lua_pushnumber(state, 0);
	}
	return 1;
}

static int doTouch(const char* path)
{
	if(path == NULL) {
		return -1;
	}
#if defined(SUSHI_SUPPORT_LINUX)
	int fd = open(path, O_WRONLY | O_CREAT, 0666);
	if(fd < 0) {
		return -1;
	}
	int v = futimens(fd, NULL);
	close(fd);
	return v;
#elif defined(SUSHI_SUPPORT_MACOS)
	int fd = open(path, O_WRONLY | O_CREAT, 0666);
	if(fd < 0) {
		return -1;
	}
	int v = futimens(fd, NULL);
	close(fd);
	return v;
#elif defined(SUSHI_SUPPORT_WIN32)
	int fd = open(path, O_WRONLY | O_CREAT, 0666);
	if(fd < 0) {
		return -1;
	}
	SYSTEMTIME st;
	FILETIME mt;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &mt);
	BOOL success = SetFileTime((HANDLE)_get_osfhandle(fd), NULL, NULL, &mt);
	close(fd);
	if(success) {
		return 0;
	}
	return -1;
#else
#error No implementation for doTouch
#endif
}

static int touch_file(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	if(doTouch(path) == 0) {
		lua_pushnumber(state, 1);
	}
	else {
		lua_pushnumber(state, 0);
	}
	return 1;
}

static int remove_file(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	if(unlink(path) == 0) {
		lua_pushnumber(state, 1);
	}
	else {
		lua_pushnumber(state, 0);
	}
	return 1;
}

static int remove_directory(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	if(rmdir(path) == 0) {
		lua_pushnumber(state, 1);
	}
	else {
		lua_pushnumber(state, 0);
	}
	return 1;
}

static int close_directory(lua_State* state)
{
	DIR** ud = (DIR**)lua_touserdata(state, 2);
	if(ud && *ud) {
		closedir(*ud);
		*ud = NULL;
	}
	return 0;
}

static int open_directory(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		return 0;
	}
	DIR* dir = opendir(path);
	if(dir == NULL) {
		return 0;
	}
	DIR** ud = (DIR**)lua_newuserdata(state, sizeof(DIR*));
	*ud = dir;
	lua_newtable(state);
	lua_pushliteral(state, "__gc");
	lua_pushcfunction(state, close_directory);
	lua_rawset(state, -3);
	lua_setmetatable(state, -2);
	return 1;
}

static int read_directory(lua_State* state)
{
	DIR** ud = (DIR**)lua_touserdata(state, 2);
	if(ud == NULL || *ud == NULL) {
		return 0;
	}
	struct dirent* de = NULL;
	while(1) {
		de = readdir(*ud);
		if(de == NULL) {
			closedir(*ud);
			*ud = NULL;
			return 0;
		}
		if(!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
			continue;
		}
		break;
	}
	lua_pushstring(state, de->d_name);
	return 1;
}

static int is_fd_file(int fd)
{
	struct stat buf;
	if(fstat(fd, &buf) != 0) {
		return 0;
	}
	if (S_ISDIR(buf.st_mode)) {
		return 0;
	}
	return 1;
}

static int open_file_for_reading(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int v = open(path, O_RDONLY);
	if(is_fd_file(v) == 0) {
		close(v);
		lua_pushnumber(state, -1);
		return 1;
	}
	lua_pushnumber(state, v);
	return 1;
}

static int open_file_for_writing(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int v = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(is_fd_file(v) == 0) {
		close(v);
		lua_pushnumber(state, -1);
		return 1;
	}
	lua_pushnumber(state, v);
	return 1;
}

static int open_file_for_appending(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int v = open(path, O_APPEND | O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(is_fd_file(v) == 0) {
		close(v);
		lua_pushnumber(state, -1);
		return 1;
	}
	lua_pushnumber(state, v);
	return 1;
}

static int read_from_handle(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	void* ptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	long sz = 0;
	memcpy(&sz, ptr, sizeof(long));
	if(fd < 0 || sz < 1) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int v = read(fd, ptr+sizeof(long), sz);
	if(v < 1) {
		lua_pushnumber(state, -1);
		return 1;
	}
	lua_pushnumber(state, v);
	return 1;
}

static int write_to_handle(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	void* ptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	long size = luaL_checknumber(state, 3);
	if(size < 0) {
		memcpy(&size, ptr, sizeof(long));
	}
	if(size == 0) {
		lua_pushnumber(state, 0);
		return 1;
	}
	int r = write(fd, ptr + sizeof(long), size);
	if(r == 0) {
		r = -1;
	}
	lua_pushnumber(state, r);
	return 1;
}

static int get_size_for_handle(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	off_t original = lseek(fd, 0, SEEK_CUR);
	if(original < 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	off_t end = lseek(fd, 0, SEEK_END);
	lseek(fd, original, SEEK_SET);
	lua_pushnumber(state, end);
	return 1;
}

static int get_current_position(lua_State* state)
{
	int fd = luaL_checknumber(state, 2);
	lua_pushnumber(state, lseek(fd, 0, SEEK_CUR));
	return 1;
}

static int set_current_position(lua_State* state)
{
	int fd = luaL_checknumber(state, 2);
	long npos = luaL_checknumber(state, 3);
	lua_pushnumber(state, lseek(fd, npos, SEEK_SET));
	return 1;
}

static int close_handle(lua_State* state)
{
	int fd = luaL_checknumber(state, 2);
	if(fd >= 0) {
		close(fd);
	}
	return 0;
}

static int get_current_directory(lua_State* state)
{
	char* v = getcwd(NULL, 0);
	if(v == NULL) {
		return 0;
	}
	lua_pushstring(state, v);
	free(v);
	return 1;

}

static int set_current_directory(lua_State* state)
{
	const char* path = lua_tostring(state, 2);
	if(path != NULL) {
		lua_pushnumber(state, chdir(path));
	}
	else {
		lua_pushnumber(state, -2);
	}
	return 1;
}

static int write_to_stdout(lua_State* state)
{
	int v = 0;
	size_t len = 0;
	const char* data = lua_tolstring(state, 2, &len);
	if(data != NULL && len > 0) {
		v = (int)write(1, data, len);
	}
	lua_pushnumber(state, v);
	return 1;
}

static int write_to_stderr(lua_State* state)
{
	int v = 0;
	size_t len = 0;
	const char* data = lua_tolstring(state, 2, &len);
	if(data != NULL && len > 0) {
		v = (int)write(2, data, len);
	}
	lua_pushnumber(state, v);
	return 1;
}

static const luaL_Reg funcs[] = {
	{ "get_file_info", get_file_info },
	{ "set_file_mode", set_file_mode },
	{ "get_real_path", get_real_path },
	{ "create_directory", create_directory },
	{ "touch_file", touch_file },
	{ "remove_file", remove_file },
	{ "remove_directory", remove_directory },
	{ "close_directory", close_directory },
	{ "read_directory", read_directory },
	{ "open_directory", open_directory },
	{ "open_file_for_reading", open_file_for_reading },
	{ "open_file_for_writing", open_file_for_writing },
	{ "open_file_for_appending", open_file_for_appending },
	{ "read_from_handle", read_from_handle },
	{ "write_to_handle", write_to_handle },
	{ "get_size_for_handle", get_size_for_handle },
	{ "get_current_position", get_current_position },
	{ "set_current_position", set_current_position },
	{ "close_handle", close_handle },
	{ "get_current_directory", get_current_directory },
	{ "set_current_directory", set_current_directory },
	{ "write_to_stdout", write_to_stdout },
	{ "write_to_stderr", write_to_stderr },
	{ NULL, NULL }
};

void lib_io_init(lua_State* state)
{
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_io");
}
