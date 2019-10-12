
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
#include "zbuf.h"

static int errors = 0;

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

int sushi_print_stacktrace(lua_State* state)
{
	luaL_traceback(state, state, NULL, 0);
	sushi_error("%s", lua_tostring(state, -1));
	lua_pop(state, 1);
	return 1;
}

static void init_libraries(lua_State* state)
{
	lib_crypto_init(state);
	lib_io_init(state);
	lib_net_init(state);
	lib_os_init(state);
	lib_util_init(state);
	lib_vm_init(state);
	lua_pushvalue(state ,(-10002));
	lua_setglobal(state ,"_g");
}

lua_State* sushi_create_new_state()
{
	lua_State* nstate = luaL_newstate();
	if(nstate == NULL) {
		return NULL;
	}
	lua_pushcfunction(nstate, sushi_print_stacktrace);
	init_libraries(nstate);
	return nstate;
}

const char* sushi_get_executable_path()
{
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
	vv->data = v;
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
	vv->data = v;
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
	vv->data = data;
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

int sushi_execute_program(lua_State* state, SushiCode* code)
{
	// NOTE: Arguments to the program are supplied via the global _args parameter
	// that needs to be set prior to calling this function.
	if(code == NULL) {
		return -1;
	}
	int freecode = 0;
	unsigned char* codep = code->data;
	unsigned long codeplen = code->dataSize;
	if(codep == NULL || codeplen < 4) {
		return -1;
	}
	if(codep[0] == 0x00 && codep[1] == 0x64 && codep[2] == 0x65 && codep[3] == 0x66) {
		zbuf_inflate(codep+4, codeplen-4, &codep, &codeplen);
		if(codep == NULL || codeplen < 1) {
			return -1;
		}
		freecode = 1;
	}
	int lbr = luaL_loadbuffer(state, codep, codeplen, code->fileName);
	if(freecode) {
		free(codep);
		codep = NULL;
	}
	if(lbr != 0) {
		const char* error = lua_tostring(state, -1);
		if(error == NULL || *error == 0) {
			error = "Failed while processing code data";
		}
		sushi_error("`%s': `%s'", code->fileName, error);
		return -1;
	}
	lua_pushstring(state, code->fileName);
	lua_setglobal(state, "_program");
	if(lua_pcall(state, 0, 1, 1) != LUA_OK) {
		const char* errstr = lua_tostring(state, -1);
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
		if(lua_pcall(state, 1, 1, 1) != 0) {
			sushi_error("Error while running the program: `%s'", lua_tostring(state, -1));
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
