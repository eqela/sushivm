
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
#ifdef SUSHI_SUPPORT_LINUX
#include <arpa/inet.h>
#endif
#ifdef SUSHI_SUPPORT_MACOS
#import <mach-o/dyld.h>
#endif
#ifdef SUSHI_SUPPORT_WIN32
#include <winsock.h>
#endif
#include "sushi.h"
#include "lib_crypto.h"
#include "lib_io.h"
#include "lib_net.h"
#include "lib_os.h"
#include "lib_util.h"
#include "lib_vm.h"
#include "lib_mod.h"
#include "lib_zip.h"
#include "zbuf.h"

static int errors = 0;
static const char* executable_path = NULL;

int sushi_print_stacktrace(lua_State* state)
{
	const char* e1 = sushi_error_to_string(state);
	luaL_traceback(state, state, NULL, 0);
	const char* e2 = sushi_error_to_string(state);
	if(e1 != NULL && e2 != NULL) {
		lua_pop(state, 2);
		char* v = (char*)malloc(strlen(e1) + 1 + strlen(e2) + 1);
		strcpy(v, e1);
		strcat(v, "\n");
		strcat(v, e2);
		lua_pushstring(state, v);
		free(v);
		return 1;
	}
	if(e1 != NULL || e2 != NULL) {
		return 1;
	}
	return 0;
}

int sushi_pcall(lua_State* state, int nargs, int nret)
{
	int hpos = lua_gettop(state) - nargs;
	lua_pushcfunction(state, sushi_print_stacktrace);
	lua_insert(state, hpos);
	int r = lua_pcall(state, nargs, nret, hpos);
	lua_remove(state, hpos);
	return r;
}

void sushi_getglobal(lua_State* state, const char* name)
{
	if(name == NULL) {
		lua_pushnil(state);
		return;
	}
	const char* dot = strchr(name, '.');
	if(dot == NULL) {
		lua_getglobal(state, name);
		return;
	}
	int orig = lua_gettop(state);
	int counter = 0;
	int idx = -1;
	const char* p = name;
	while(1) {
		int compl = dot-p;
		char* comp = (char*)malloc(compl+1);
		strncpy(comp, p, compl);
		comp[compl] = 0;
		if(idx < 0) {
			lua_getglobal(state, comp);
		}
		else if(lua_istable(state, idx) == 0) {
			lua_pushnil(state);
		}
		else {
			lua_pushstring(state, comp);
			lua_gettable(state, idx);
		}
		if(lua_isnil(state, -1)) {
			lua_pushnil(state);
			return;
		}
		idx = lua_gettop(state);
		counter ++;
		free(comp);
		if(*dot == 0) {
			break;
		}
		p = dot + 1;
		dot = strchr(p, '.');
		if(dot == NULL) {
			dot = p + strlen(p);
		}
	}
	lua_insert(state, orig+1);
	lua_pop(state, counter-1);
}

const char* sushi_error_to_string(lua_State* state)
{
	const char* v = lua_tostring(state, -1);
	if(v != NULL) {
		return v;
	}
	if(lua_istable(state, -1)) {
		int tableindex = lua_gettop(state);
		lua_pushstring(state, "toString");
		lua_gettable(state, -2);
		if(lua_isfunction(state, -1)) {
			lua_pushvalue(state, tableindex);
			lua_call(state, 1, 1);
			return lua_tostring(state, -1);
		}
	}
	return NULL;
}

void sushi_error(const char* fmt, ...)
{
	va_list vlist;
	va_start(vlist, fmt);
	fprintf(stderr, "[sushi:error] ");
	vfprintf(stderr, fmt, vlist);
	fprintf(stderr, "\n");
	errors ++;
}

int sushi_get_error_count()
{
	return errors;
}

int sushi_has_errors()
{
	if(errors > 0) {
		return 1;
	}
	return 0;
}

static void init_libraries(lua_State* state)
{
	lib_crypto_init(state);
	lib_io_init(state);
	lib_net_init(state);
	lib_os_init(state);
	lib_util_init(state);
	lib_vm_init(state);
	lib_mod_init(state);
	lib_zip_init(state);
	lua_pushvalue(state ,(-10002));
	lua_setglobal(state ,"_g");
}

lua_State* sushi_create_new_state()
{
	lua_State* nstate = luaL_newstate();
	if(nstate == NULL) {
		return NULL;
	}
	init_libraries(nstate);
	return nstate;
}

void sushi_set_executable_path(const char* path)
{
	executable_path = path;
}

const char* sushi_get_executable_path()
{
	if(executable_path != NULL) {
		return executable_path;
	}
#if defined(SUSHI_SUPPORT_LINUX)
	static char selfPath[PATH_MAX+1];
	if(readlink("/proc/self/exe", selfPath, PATH_MAX) < 0) {
		return NULL;
	}
	return selfPath;
#elif defined(SUSHI_SUPPORT_MACOS)
	static char selfPath[PATH_MAX+1];
	uint32_t size = sizeof(selfPath);
	if(_NSGetExecutablePath(selfPath, &size) < 0) {
		return NULL;
	}
	return selfPath;
#elif defined(SUSHI_SUPPORT_WIN32)
	static char selfPath[PATH_MAX+1];
	int r = GetModuleFileName(NULL, selfPath, PATH_MAX);
	if(r < 1) {
		return NULL;
	}
	return selfPath;
#else
#error No implementation for sushi_get_executable_path
#endif
}

char* sushi_get_real_path(const char* path)
{
#if defined(SUSHI_SUPPORT_LINUX)
	char _realpath[PATH_MAX+1];
	const char* rpath = realpath(path, _realpath);
	if(rpath == NULL) {
		return NULL;
	}
	return strdup(rpath);
#elif defined(SUSHI_SUPPORT_MACOS)
	char _realpath[PATH_MAX+1];
	const char* rpath = realpath(path, _realpath);
	if(rpath == NULL) {
		return NULL;
	}
	return strdup(rpath);
#elif defined(SUSHI_SUPPORT_WIN32)
	char _realpath[32768];
	int r = GetFullPathName(path, 32768, _realpath, NULL);
	if(r < 1) {
		return NULL;
	}
	return strdup(_realpath);
#else
#error No implementation for realpath
#endif
}

SushiCode* sushi_code_for_buffer(unsigned char* data, unsigned long dataSize, char* fileName)
{
	SushiCode* vv = (SushiCode*)malloc(sizeof(SushiCode));
	vv->data = data;
	vv->dataSize = dataSize;
	vv->fileName = fileName;
	return vv;
}

SushiCode* sushi_code_read_from_stdin()
{
	size_t sz = 0;
	char* v = NULL;
	char buffer[2048];
	while(1) {
		size_t r = read(STDIN_FILENO, buffer, 2048);
		if(r < 1) {
			break;
		}
		v = realloc(v, sz + r);
		if(v == NULL) {
			return NULL;
		}
		memcpy(v + sz, buffer, r);
		sz += r;
	}
	SushiCode* vv = (SushiCode*)malloc(sizeof(SushiCode));
	if(vv == NULL) {
		free(v);
		return NULL;
	}
	vv->data = (unsigned char*)v;
	vv->dataSize = (unsigned long)sz;
	vv->fileName = strdup("<stdin>");
	return vv;
}

SushiCode* sushi_code_read_from_file(const char* path)
{
	if(path == NULL) {
		return NULL;
	}
	char* rpath = sushi_get_real_path(path);
	if(rpath == NULL) {
		rpath = strdup(path);
	}
	if(rpath == NULL) {
		return NULL;
	}
	struct stat st;
	if(stat(rpath, &st) != 0) {
		free(rpath);
		return NULL;
	}
	int sz = st.st_size;
	if(sz < 1) {
		free(rpath);
		return NULL;
	}
	FILE* fp = fopen(rpath, "rb");
	if(fp == NULL) {
		free(rpath);
		return NULL;
	}
	char* v = (char*)malloc(sz);
	if(v == NULL) {
		fclose(fp);
		free(rpath);
		return NULL;
	}
	if(fread(v, 1, sz, fp) != sz) {
		fclose(fp);
		free(v);
		free(rpath);
		return NULL;
	}
	fclose(fp);
	SushiCode* vv = (SushiCode*)malloc(sizeof(SushiCode));
	if(vv == NULL) {
		free(v);
		free(rpath);
		return NULL;
	}
	vv->data = (unsigned char*)v;
	vv->dataSize = sz;
	vv->fileName = rpath;
	return vv;
}

SushiCode* sushi_code_read_from_executable(const char* exepath)
{
	if(exepath == NULL) {
		return NULL;
	}
	FILE* fp = fopen(exepath, "rb");
	if(fp == NULL) {
		sushi_error("Failed to read executable: `%s'", exepath);
		return NULL;
	}
	if(fseek(fp, -8, SEEK_END) != 0) {
		sushi_error("Failed to seek within executable: `%s'", exepath);
		fclose(fp);
		return NULL;
	}
	long footerpos = ftell(fp);
	char buf[8];
	if(fread(buf, 1, 8, fp) != 8) {
		sushi_error("Failed to read executable footer: `%s'", exepath);
		fclose(fp);
		return NULL;
	}
	if(buf[0] != 'S' || buf[1] != 'u' || buf[2] != 'p' || buf[3] != 'P') {
		fclose(fp);
		return NULL;
	}
	uint32_t n = 0;
	memcpy(&n, buf+4, 4);
	n = ntohl(n);
	long codesize = footerpos - (long)n;
	if(fseek(fp, n, SEEK_SET) != 0) {
		sushi_error("Failed to seek to position %u in executable: `%s': Corrupted content", n, exepath);
		fclose(fp);
		return NULL;
	}
	char* data = (char*)malloc(codesize);
	if(fread(data, 1, codesize, fp) != codesize) {
		sushi_error("Failed to read %u bytes in position %u in executable: `%s': Corrupted content", codesize, n, exepath);
		free(data);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	SushiCode* vv = (SushiCode*)malloc(sizeof(SushiCode));
	if(vv == NULL) {
		free(data);
		return NULL;
	}
	vv->data = (unsigned char*)data;
	vv->dataSize = codesize;
	vv->fileName = strdup(exepath);
	return vv;
}

SushiCode* sushi_code_free(SushiCode* code)
{
	if(code == NULL) {
		return NULL;
	}
	if(code->data) {
		free(code->data);
	}
	if(code->fileName) {
		free(code->fileName);
	}
	free(code);
	return NULL;
}

int sushi_load_code(lua_State* state, SushiCode* code)
{
	if(code == NULL) {
		return -1;
	}
	int freecode = 0;
	unsigned char* codep = code->data;
	unsigned long codeplen = code->dataSize;
	if(codep == NULL || codeplen < 4) {
		return -1;
	}
	char* fileName = code->fileName;
	if(fileName == NULL) {
		fileName = "__code__";
	}
	if(codep[0] == 0x00 && codep[1] == 0x64 && codep[2] == 0x65 && codep[3] == 0x66) {
		zbuf_inflate(codep+4, codeplen-4, &codep, &codeplen);
		if(codep == NULL || codeplen < 1) {
			return -1;
		}
		freecode = 1;
	}
	int lbr = luaL_loadbuffer(state, (const char*)codep, codeplen, fileName);
	if(lbr == 0) {
		void* ptr = lua_newuserdata(state, sizeof(long) + (size_t)codeplen);
		luaL_getmetatable(state, "_sushi_buffer");
		lua_setmetatable(state, -2);
		memcpy(ptr, &codeplen, sizeof(long));
		memcpy(ptr + sizeof(long), codep, codeplen);
		lua_setglobal(state, "_code");
	}
	if(freecode) {
		free(codep);
		codep = NULL;
	}
	if(lbr != 0) {
		const char* error = sushi_error_to_string(state);
		if(error == NULL || *error == 0) {
			error = "Failed while processing code data";
		}
		sushi_error("`%s': `%s'", fileName, error);
		return -1;
	}
	lua_pushstring(state, fileName);
	lua_setglobal(state, "_program");
	return 0;
}

int sushi_execute_program(lua_State* state, SushiCode* code)
{
	struct timeval time1, time2, time3;
	// NOTE: Arguments to the program are supplied via the global _args parameter
	// that needs to be set prior to calling this function.
	if(sushi_load_code(state, code) != 0) {
		return -1;
	}
	if(sushi_pcall(state, 0, 1) != LUA_OK) {
		const char* errstr = sushi_error_to_string(state);
		if(errstr != NULL) {
			sushi_error("Execution failed: `%s'", errstr);
		}
		else {
			sushi_error("Execution failed.");
		}
		return -1;
	}
	int rv = lua_tonumber(state, lua_gettop(state));
	lua_getglobal(state, "_main");
	if(lua_isnil(state, lua_gettop(state)) == 0) {
		lua_getglobal(state, "_args");
		if(sushi_pcall(state, 1, 1) != LUA_OK) {
			sushi_error("Error while executing the program: `%s'", sushi_error_to_string(state));
			return -1;
		}
		rv = lua_tonumber(state, lua_gettop(state));
	}
	return rv;
}

static void _profiler_callback(void* data, lua_State* state, int samples, int vmstate)
{
	FILE* profilerFile = (FILE*)data;
	if(profilerFile == NULL) {
		return;
	}
	unsigned long len = 0;
	const char* stack = luaJIT_profile_dumpstack(state, "l f", 1, &len);
	if(stack != NULL) {
		for(int n=0; n<samples; n++) {
			fprintf(profilerFile, "%*.*s\n", (int)len, (int)len, stack);
		}
	}
}

void* sushi_profiler_start(lua_State* state, const char* outputFile)
{
	FILE* fp = NULL;
	if(outputFile != NULL && *outputFile != 0) {
		fp = fopen(outputFile, "w");
		if(fp == NULL) {
			sushi_error("Failed to write to profiler output file: `%s'", outputFile);
		}
		else {
			luaJIT_profile_start(state, "f", _profiler_callback, (void*)fp);
		}
	}
	return((void*)fp);
}

void* sushi_profiler_stop(lua_State* state, void* profiler)
{
	if(profiler != NULL) {
		luaJIT_profile_stop(state);
		fclose((FILE*)profiler);
	}
	return NULL;
}
