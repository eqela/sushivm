
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sushi.h"

#define DEFAULT_REPOSITORY_URL "https://files.eqela.com/eqelart/tools"

extern const unsigned char install_application_lz[];
extern int install_application_lz_size;

static char* application_directory = NULL;
static char* application_repository = NULL;

void apps_set_directory(const char* dir)
{
	if(application_directory != NULL) {
		free(application_directory);
		application_directory = NULL;
	}
	if(dir != NULL) {
		application_directory = strdup(dir);
	}
}

void apps_set_repository(const char* url)
{
	if(application_repository != NULL) {
		free(application_repository);
		application_repository = NULL;
	}
	if(url != NULL) {
		application_repository = strdup(url);
	}
}

static int file_exists(const char* pathName)
{
	struct stat st;
	if(stat(pathName, &st) == 0) {
		return 1;
	}
	return 0;
}

static int parse_app_name(const char* app_name, char** pname, char** pversion)
{
	if(app_name == NULL || pname == NULL || pversion == NULL) {
		sushi_error("Invalid parameters in call to parse_app_name");
		return -1;
	}
	char* dash = strchr(app_name, '-');
	if(dash == NULL) {
		sushi_error("Invalid app name: `%s'", app_name);
		return -1;
	}
	int namelen = dash - app_name;
	char* name = (char*)malloc(namelen+1);
	strncpy(name, app_name, namelen);
	name[namelen] = 0;
	int versionlen = strlen(dash+1);
	char* version = (char*)malloc(versionlen+1);
	strncpy(version, dash+1, versionlen);
	version[versionlen] = 0;
	*pname = name;
	*pversion = version;
	return 0;
}

static char* apps_find_executable_file(const char* app_name, char* buffer, int buffer_size)
{
	if(app_name == NULL || buffer == NULL || buffer_size < 1) {
		return NULL;
	}
	char *name, *version;
	if(parse_app_name(app_name, &name, &version) != 0) {
		return NULL;
	}
	char* v = NULL;
	if(application_directory != NULL) {
		snprintf(buffer, buffer_size, "%s/%s/%s/%s.lua", application_directory, name, version, name);
		if(file_exists(buffer)) {
			v = buffer;
		}
	}
	else {
		const char* profile = sushi_get_profile_directory();
		if(profile == NULL) {
			profile = "";
		}
		snprintf(buffer, buffer_size, "%s/apps/%s/%s/%s.lua", profile, name, version, name);
		if(file_exists(buffer)) {
			v = buffer;
		}
	}
	free(name);
	free(version);
	return v;
}

static int execute_install_application(const char* type, const char* app_name, const char* app_repo, const char* app_dir)
{
	lua_State* state = sushi_create_new_state();
	if(state == NULL) {
		return -1;
	}
	SushiCode* code = (SushiCode*)malloc(sizeof(SushiCode));
	code->data = (char*)install_application_lz;
	code->dataSize = install_application_lz_size;
	code->fileName = "install_application.lz";
	lua_createtable(state, 3, 0);
	lua_pushnumber(state, 1);
	lua_pushstring(state, type);
	lua_rawset(state, -3);
	lua_pushnumber(state, 2);
	lua_pushstring(state, app_name);
	lua_rawset(state, -3);
	lua_pushnumber(state, 3);
	lua_pushstring(state, app_repo);
	lua_rawset(state, -3);
	lua_pushnumber(state, 4);
	lua_pushstring(state, app_dir);
	lua_rawset(state, -3);
	lua_setglobal(state, "_args");
	int rv = sushi_execute_program(state, code);
	free(code);
	lua_gc(state, LUA_GCCOLLECT, 0);
	lua_close(state);
	return rv;
}

static int install_application(const char* app_name, const char* type)
{
	const char* repo = application_repository;
	if(repo == NULL || *repo == 0) {
		repo = DEFAULT_REPOSITORY_URL;
	}
	char appdir_buffer[PATH_MAX+1];
	const char* appdir = application_directory;
	if(appdir == NULL || *appdir == 0) {
		const char* profile = sushi_get_profile_directory();
		if(profile == NULL) {
			profile = "";
		}
		snprintf(appdir_buffer, PATH_MAX, "%s/apps", profile);
		appdir = appdir_buffer;
	}
	if(execute_install_application(type, app_name, repo, appdir) != 0) {
		sushi_error("Application installation failed: `%s'", app_name);
		return -1;
	}
	return 0;
}

int apps_install_file(const char* file)
{
	return install_application(file, "file");
}

char* apps_get_executable_file(const char* app_name, char* buffer, int buffer_size, int allow_download)
{
	char* v = apps_find_executable_file(app_name, buffer, buffer_size);
	if(v != NULL) {
		return v;
	}
	if(allow_download) {
		if(install_application(app_name, "name") != 0) {
			sushi_error("Failed to install application: `%s'", app_name);
			return NULL;
		}
	}
	return apps_find_executable_file(app_name, buffer, buffer_size);
}
