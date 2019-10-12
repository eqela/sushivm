
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
#include <stdio.h>
#include <string.h>
#include "sushi.h"

static const char* get_app_name(const char* fullpath)
{
	if(fullpath == NULL) {
		return NULL;
	}
	const char* slash = strrchr(fullpath, '/');
	if(slash == NULL) {
		return fullpath;
	}
	const char* v = slash + 1;
	const char* dot = strchr(v, '.');
	if(dot == NULL) {
		return v;
	}
	static char vbuf[PATH_MAX+1];
	int len = (int)(dot - v);
	strncpy(vbuf, v, len);
	vbuf[len] = 0;
	return vbuf;
}

static const char* get_custom_application_path(char* buffer, const char* path, const char* appname)
{
	if(path == NULL) {
		return appname;
	}
	if(appname == NULL) {
		return NULL;
	}
	if(strlen(path) + 1 + strlen(appname) > PATH_MAX) {
		return NULL;
	}
	sprintf(buffer, "%s/%s", path, appname);
	return buffer;
}

static const char* get_application_path()
{
	const char* exe = sushi_get_executable_path();
	if(exe == NULL) {
		return NULL;
	}
	static char applicationPath[PATH_MAX+1];
	if(!strncmp(exe, "/bin/", 5)) {
		return get_custom_application_path(applicationPath, "/lib/", exe+5);
	}
	if(!strncmp(exe, "/usr/bin/", 9)) {
		return get_custom_application_path(applicationPath, "/usr/lib/", exe+9);
	}
	if(!strncmp(exe, "/usr/local/bin/", 15)) {
		return get_custom_application_path(applicationPath, "/usr/local/lib/", exe+15);
	}
	const char* slash = strrchr(exe, '/');
	if(slash == NULL) {
		slash = strrchr(exe, '\\');
	}
	if(slash == NULL) {
		return NULL;
	}
	int len = (int)(slash - exe);
	strncpy(applicationPath, exe, len);
	applicationPath[len] = 0;
	return applicationPath;
}

static const char* get_code_filename_for_app(const char* appname)
{
	if(appname == NULL) {
		return NULL;
	}
	const char* appdir = get_application_path();
	if(appdir == NULL) {
		return NULL;
	}
	if(strlen(appdir) + 1 + strlen(appname) + 4 > PATH_MAX) {
		return NULL;
	}
	static char v[PATH_MAX+1];
	sprintf(v, "%s/%s.lua", appdir, appname);
	return v;
}

int main(int c, char** v)
{
	int processArgc = c;
	char** processArgv = v;
	int processArgcReserved = 2;
	SushiCode* code = sushi_code_read_from_executable(sushi_get_executable_path());
	if(sushi_has_errors()) {
		return 1;
	}
	if(code == NULL) {
		const char* appname = get_app_name(v[0]);
		const char* filename = NULL;
		if(!strcasecmp(appname, "sushi")) {
			if(c < 2) {
				code = sushi_code_read_from_stdin();
				if(code == NULL) {
					sushi_error("Failed while reading code from standard input.");
					return 1;
				}
			}
			else {
				if(!strcasecmp(v[1], "-v") || !strcasecmp(v[1], "-version") || !strcasecmp(v[1], "--version")) {
					printf("%s\n", SUSHI_VERSION);
					return 0;
				}
				if(!strcasecmp(v[1], "-h") || !strcasecmp(v[1], "-help") || !strcasecmp(v[1], "--help")) {
					sushi_error("Usage: %s <code> <parameters..>", v[0]);
					return 0;
				}
				processArgcReserved = 2;
				filename = v[1];
			}
		}
		else {
			filename = get_code_filename_for_app(appname);
			if(filename == NULL) {
				sushi_error("Failed to find code file for app: `%s'", appname);
				return 1;
			}
			processArgcReserved = 1;
		}
		if(code == NULL) {
			code = sushi_code_read_from_file(filename);
			if(code == NULL) {
				sushi_error("Failed to read code file: `%s'", v[1]);
				return 1;
			}
		}
	}
	else {
		processArgcReserved = 1;
	}
	lua_State* state = sushi_create_new_state();
	if(state == NULL) {
		return 1;
	}
	void* profiler = sushi_profiler_start(state, getenv("SUSHI_PROFILER_OUTPUT"));
	lua_createtable(state, processArgc-processArgcReserved, 0);
	for(int i = processArgcReserved; i < processArgc; i++) {
		lua_pushnumber(state, i - processArgcReserved + 1);
		lua_pushstring(state, processArgv[i]);
		lua_rawset(state, -3);
	}
	lua_setglobal(state, "_args");
	int rv = sushi_execute_program(state, code);
	profiler = sushi_profiler_stop(state, profiler);
	code = sushi_code_free(code);
	lua_gc(state, LUA_GCCOLLECT, 0);
	lua_close(state);
	return rv;
}
