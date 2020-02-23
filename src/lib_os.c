
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

#include "lib_os.h"

int sleep_seconds(lua_State* state);
int sleep_microseconds(lua_State* state);
int sleep_milliseconds(lua_State* state);
int get_all_environment_variables(lua_State* state);
int get_environment_variable(lua_State* state);
int get_system_type(lua_State* state);
int get_system_time_seconds(lua_State* state);
int get_system_time_milliseconds(lua_State* state);
int get_system_timezone_seconds(lua_State* state);
int get_timestamp_details_local(lua_State* state);
int get_timestamp_details_utc(lua_State* state);
int start_process(lua_State* state);
int start_piped_process(lua_State* state);
int replace_process(lua_State* state);
int wait_for_process(lua_State* state);
int is_process_alive(lua_State* state);
int send_process_signal(lua_State* state);
int disown_process(lua_State* state);
int start_thread(lua_State* state);

static const luaL_Reg funcs[] = {
	{ "sleep_seconds", sleep_seconds },
	{ "sleep_microseconds", sleep_microseconds },
	{ "sleep_milliseconds", sleep_milliseconds },
	{ "get_all_environment_variables", get_all_environment_variables },
	{ "get_environment_variable", get_environment_variable },
	{ "get_system_type", get_system_type },
	{ "get_system_time_seconds", get_system_time_seconds },
	{ "get_system_time_milliseconds", get_system_time_milliseconds },
	{ "get_system_timezone_seconds", get_system_timezone_seconds },
	{ "get_timestamp_details_local", get_timestamp_details_local },
	{ "get_timestamp_details_utc", get_timestamp_details_utc },
	{ "start_process", start_process },
	{ "start_piped_process", start_piped_process },
	{ "replace_process", replace_process },
	{ "wait_for_process", wait_for_process },
	{ "is_process_alive", is_process_alive },
	{ "send_process_signal", send_process_signal },
	{ "disown_process", disown_process },
	{ "start_thread", start_thread },
	{ NULL, NULL }
};

void lib_os_init(lua_State* state)
{
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_os");
}
