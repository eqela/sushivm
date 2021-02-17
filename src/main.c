
/*
 * This file is part of SushiVM
 * Copyright (c) 2019-2021 J42 Pte Ltd
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

#if defined(SUSHI_SUPPORT_WIN32)
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shlobj.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sushi.h"
#if defined(SUSHI_SUPPORT_LINUX) || defined(SUSHI_SUPPORT_MACOS)
#include <signal.h>
#endif
#include <limits.h>
#include "shebang.h"
#include "apps.h"

static int is_offline = 0;
static int is_cloud = 0;
static const char* profiler_output = NULL;

int string_ends_with(const char* str, const char* ends)
{
	if(str == NULL) {
		return 0;
	}
	if(ends == NULL) {
		return 1;
	}
	const char* p = strstr(str, ends);
	if(p == NULL) {
		return 0;
	}
	if(strcmp(p, ends) != 0) {
		return 0;
	}
	return 1;
}

int is_file_name(const char* name)
{
	if(name == NULL) {
		return 0;
	}
	if(strchr(name, '/') != NULL) {
		return 1;
	}
	if(strchr(name, '\\') != NULL) {
		return 1;
	}
	if(string_ends_with(name, ".lz") || string_ends_with(name, ".lua") || string_ends_with(name, ".ss") || string_ends_with(name, ".sapp")) {
		return 1;
	}
	return 0;
}

int is_app_name(const char* name)
{
	if(name == NULL) {
		return 0;
	}
	if(is_file_name(name)) {
		return 0;
	}
	if(strchr(name, '-') == NULL) {
		return 0;
	}
	return 1;
}

static const char* get_app_name(const char* fullpath)
{
	if(fullpath == NULL) {
		return NULL;
	}
	const char* v = fullpath;
	const char* slash = strrchr(fullpath, '/');
#if defined(SUSHI_SUPPORT_WIN32)
	if(slash == NULL) {
		slash = strrchr(fullpath, '\\');
	}
#endif
	if(slash != NULL) {
		v = slash + 1;
	}
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

static int sushi_main_code(SushiCode* code, int processArgc, const char** processArgv, int processArgcReserved)
{
	lua_State* state = sushi_create_new_state();
	if(state == NULL) {
		return -1;
	}
	void* profiler = sushi_profiler_start(state, profiler_output);
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

static int sushi_main_file(const char* fileName, int processArgc, const char** processArgv, int processArgcReserved);

static int sushi_main_app(const char* appName, int processArgc, const char** processArgv, int processArgcReserved)
{
	if(is_cloud) {
		sushi_error("Not supported for direct app call: -cloud");
		return -1;
	}
	char buffer[PATH_MAX+1];
	char* script = apps_get_executable_file(appName, buffer, PATH_MAX, is_offline == 1 ? 0 : 1);
	if(script == NULL) {
		sushi_error("Application not found: `%s'", appName);
		return -1;
	}
	return sushi_main_file(script, processArgc, processArgv, processArgcReserved);
}

static int sushi_main_file(const char* fileName, int processArgc, const char** processArgv, int processArgcReserved)
{
	if(fileName == NULL) {
		sushi_error("No file to execute.");
		return -1;
	}
	SushiCode* code = NULL;
	if(is_cloud) {
		// FIXME: Compile the code using an API call
		sushi_error("cloud compilation: Not implemented");
		return -1;
	}
	else {
		if(!strcmp(fileName, "-")) {
			code = sushi_code_read_from_stdin();
			if(code == NULL) {
				sushi_error("Failed while reading code from standard input.");
				return -1;
			}
		}
		else {
			int nc;
			char** nv;
			if(shebang_process(fileName, processArgc, processArgv, processArgcReserved, &nc, &nv) > 0) {
				int v = sushi_main_app(nv[0], nc, (const char**)nv, 1);
				shebang_argv_free(nv);
				return v;
			}
			code = sushi_code_read_from_file(fileName);
			if(code == NULL) {
				sushi_error("Failed to read code file: `%s'", fileName);
				return -1;
			}
		}
	}
	return sushi_main_code(code, processArgc, processArgv, processArgcReserved);
}

static int sushi_main_install(const char* appname)
{
	if(is_file_name(appname)) {
		return apps_install_file(appname);
	}
	char buffer[PATH_MAX+1];
	char* script = apps_get_executable_file(appname, buffer, PATH_MAX, is_offline == 1 ? 0 : 1);
	if(script == NULL) {
		sushi_error("Application not found: `%s'", appname);
		return -1;
	}
	return 0;
}

static const char* _get_environment_variable(const char* name)
{
#ifdef SUSHI_SUPPORT_WIN32
	static char buffer[32768];
	int r = GetEnvironmentVariable(name, buffer, 32768);
	if(r < 1) {
		return "";
	}
	return buffer;
#else
	return getenv(name);
#endif
}

static void initialize_configuration()
{
	char* homeDirectory = NULL;
#ifdef SUSHI_SUPPORT_WIN32
	PWSTR profile;
	char hdbuffer[32768];
	if(SHGetKnownFolderPath(&FOLDERID_Profile, 0, NULL, &profile) == S_OK) {
		size_t rl;
		wcstombs_s(&rl, hdbuffer, 32768, profile, 32767);
		homeDirectory = hdbuffer;
		CoTaskMemFree(profile);
	}
	if(homeDirectory == NULL) {
		homeDirectory = "C:\\";
	}
	SetEnvironmentVariable("HOME", homeDirectory);
#else
	homeDirectory = _get_environment_variable("HOME");
	if(homeDirectory == NULL) {
		homeDirectory = "/";
		setenv("HOME", homeDirectory, 1);
	}
#endif
	const char* pdir = _get_environment_variable("SUSHI_PROFILE_DIRECTORY");
	if(pdir != NULL) {
		sushi_set_profile_directory(pdir);
	}
	else {
		char buffer[PATH_MAX+1];
		snprintf(buffer, PATH_MAX, "%s/.sushi", homeDirectory);
		sushi_set_profile_directory(buffer);
	}
	profiler_output = _get_environment_variable("SUSHI_PROFILER_OUTPUT");
	const char* appDir = _get_environment_variable("SUSHI_APPLICATION_DIRECTORY");
	if(appDir != NULL && *appDir) {
		apps_set_directory(appDir);
	}
	const char* offline = _get_environment_variable("SUSHI_OFFLINE");
	if(offline && !strcasecmp(offline, "1")) {
		is_offline = 1;
	}
	const char* cloud = _get_environment_variable("SUSHI_CLOUD");
	if(cloud && !strcasecmp(cloud, "1")) {
		is_cloud = 1;
	}
}

static void show_usage(const char* v0)
{
	printf("Sushi VM %s\n\n", SUSHI_VERSION);
	printf("Usage: %s <parameters>\n\n", v0);
	printf("Supported parameters:\n");
	printf("  -v, -version --version                       Display SushiVM version\n");
	printf("  -h, -help, --help                            Display SushiVM usage\n");
	printf("  -i, -install, --install <file|name-version>  Install a Sushi application\n");
	printf("  -offline, --offline                          Prevent downloading of applications\n");
	// printf("  -cloud, --cloud                              Compile scripts in the cloud\n");
	printf("  -p, -profile, --profile <dir>                Specify path to profile directory\n");
	printf("  -profiler-output, --profiler-output          Specify an output file for profiling output\n");
	printf("  -apps, --apps <dir>                          Specify the directory where applications are saved\n");
	printf("  -stdin                                       Read program to execute from stdin\n");
	printf("  -f, -file, --file <file> <args>              Specify the filename of a program to execute\n");
	printf("  -a, -app, --app <name-version> <args>        Specify the name and version of application to execute\n");
	printf("  <file|name-version> <args>                   Execute a file or application\n");
	printf("\n");
}

int main(int c, const char** v)
{
	sushi_init_libraries();
	initialize_configuration();
#if defined(SUSHI_SUPPORT_LINUX) || defined(SUSHI_SUPPORT_MACOS)
	signal(SIGPIPE, SIG_IGN);
	// FIXME: Handle SIGINT, SIGTERM
#endif
	// Special scenario 1: If the app code is embedded in the executable
	{
		const char* ep = sushi_get_executable_path();
		if(ep == NULL) {
			char* rp = sushi_get_real_path(v[0]);
			if(rp != NULL) {
				sushi_set_executable_path(rp);
				ep = rp;
			}
		}
		SushiCode* code = sushi_code_read_from_executable(ep);
		if(sushi_has_errors()) {
			return -1;
		}
		if(code != NULL) {
			return sushi_main_code(code, c, v, 1);
		}
	}
	// Special scenario 2: If the executable is not "sushi"
	{
		const char* appname = get_app_name(v[0]);
		if(strcasecmp(appname, "sushi") != 0) {
			const char* filename = get_code_filename_for_app(appname);
			if(filename == NULL) {
				sushi_error("Failed to find code file for app: `%s'", appname);
				return -1;
			}
			SushiCode* code = sushi_code_read_from_file(filename);
			if(code == NULL) {
				sushi_error("Failed to read code file: `%s'", filename);
				return -1;
			}
			return sushi_main_code(code, c, v, 1);
		}
	}
	int acceptOptions = 1;
	// Normal scenario: Execute as "sushi"
	for(int n=1; n<c; n++) {
		if(acceptOptions) {
			if(!strcasecmp(v[n], "-v") || !strcasecmp(v[n], "-version") || !strcasecmp(v[n], "--version")) {
				printf("%s\n", SUSHI_VERSION);
				return 0;
			}
			if(!strcasecmp(v[n], "-h") || !strcasecmp(v[n], "-help") || !strcasecmp(v[n], "--help")) {
				show_usage(v[0]);
				return 0;
			}
			if(!strcasecmp(v[n], "-i") || !strcasecmp(v[n], "-install") || !strcasecmp(v[n], "--install")) {
				const char* package = NULL;
				n++;
				if(n < c) {
					package = v[n];
				}
				if(package == NULL) {
					sushi_error("Missing parameter for `%s'", v[n-1]);
					return -1;
				}
				return sushi_main_install(package);
			}
			if(!strcasecmp(v[n], "-offline") || !strcasecmp(v[n], "--offline")) {
				is_offline = 1;
				if(is_cloud) {
					sushi_error("Cannot mix -offline and -cloud!");
					return -1;
				}
				continue;
			}
			if(!strcasecmp(v[n], "-cloud") || !strcasecmp(v[n], "--cloud")) {
				is_cloud = 1;
				if(is_offline) {
					sushi_error("Cannot mix -offline and -cloud!");
					return -1;
				}
				continue;
			}
			if(!strcasecmp(v[n], "-p") || !strcasecmp(v[n], "-profile") || !strcasecmp(v[n], "--profile")) {
				const char* dir = NULL;
				n++;
				if(n < c) {
					dir = v[n];
				}
				if(dir == NULL) {
					sushi_error("Missing parameter for `%s'", v[n-1]);
					return -1;
				}
				sushi_set_profile_directory(dir);
				continue;
			}
			if(!strcasecmp(v[n], "-profiler-output") || !strcasecmp(v[n], "--profiler-output")) {
				const char* file = NULL;
				n++;
				if(n < c) {
					file = v[n];
				}
				if(file == NULL) {
					sushi_error("Missing parameter for `%s'", v[n-1]);
					return -1;
				}
				profiler_output = file;
				continue;
			}
			if(!strcasecmp(v[n], "-apps") || !strcasecmp(v[n], "--apps")) {
				const char* dir = NULL;
				n++;
				if(n < c) {
					dir = v[n];
				}
				if(dir == NULL) {
					sushi_error("Missing parameter for `%s'", v[n-1]);
					return -1;
				}
				apps_set_directory(dir);
				continue;
			}
			if(!strcasecmp(v[n], "-stdin")) {
				n++;
				return sushi_main_file("-", c, v, n);
			}
			if(!strcasecmp(v[n], "-f") || !strcasecmp(v[n], "-file") || !strcasecmp(v[n], "--file")) {
				const char* fileName = NULL;
				n++;
				if(n < c) {
					fileName = v[n];
				}
				if(fileName == NULL) {
					sushi_error("Missing parameter for `%s'", v[n-1]);
					return -1;
				}
				n++;
				return sushi_main_file(fileName, c, v, n);
			}
			if(!strcasecmp(v[n], "-a") || !strcasecmp(v[n], "-app") || !strcasecmp(v[n], "--app")) {
				const char* appName = NULL;
				n++;
				if(n < c) {
					appName = v[n];
				}
				if(appName == NULL) {
					sushi_error("Missing parameter for `%s'", v[n-1]);
					return -1;
				}
				n++;
				return sushi_main_app(appName, c, v, n);
			}
			if(!strcasecmp(v[n], "--")) {
				acceptOptions = 0;
				continue;
			}
		}
		if(is_file_name(v[n])) {
			return sushi_main_file(v[n], c, v, n+1);
		}
		if(is_app_name(v[n])) {
			return sushi_main_app(v[n], c, v, n+1);
		}
		sushi_error("Unsupported parameter: `%s'", v[n]);
		return -1;
	}
	show_usage(v[0]);
	return 0;
}
