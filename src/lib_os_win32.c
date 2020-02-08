
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

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <windows.h>
#include "lib_os.h"

int sleep_seconds(lua_State* state)
{
	long n = luaL_checknumber(state, 2);
	Sleep(n * 1000);
	return 0;
}

int sleep_microseconds(lua_State* state)
{
	long n = luaL_checknumber(state, 2);
	Sleep(n / 1000);
	return 0;
}

int sleep_milliseconds(lua_State* state)
{
	long n = luaL_checknumber(state, 2);
	Sleep(n);
	return 0;
}

int get_all_environment_variables(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

int get_environment_variable(lua_State* state)
{
	const char* varname = lua_tostring(state, 2);
	if(varname == NULL) {
		lua_pushnil(state);
		return 1;
	}
	lua_pushstring(state, getenv(varname));
	return 1;
}

int get_system_type(lua_State* state)
{
	lua_pushstring(state, "windows");
	return 1;
}

int get_system_time_seconds(lua_State* state)
{
	tzset();
	lua_pushnumber(state, time(NULL) - timezone);
	return 1;
}

int get_system_time_milliseconds(lua_State* state)
{
	tzset();
	struct timeval tv;
	gettimeofday(&tv, (void*)0);
	long v = tv.tv_sec * 1000L + tv.tv_usec / 1000L;
	lua_pushnumber(state, v);
	return 1;
}

int get_system_timezone_seconds(lua_State* state)
{
	tzset();
	lua_pushnumber(state, timezone);
	return 1;
}

int get_timestamp_details_local(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

int get_timestamp_details_utc(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

int start_process(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

int start_piped_process(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

int replace_process(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

int wait_for_process(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

int is_process_alive(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

int send_process_signal(lua_State* state)
{
	// FIXME
	return 0;
}

int disown_process(lua_State* state)
{
	// FIXME
	return 0;
}
