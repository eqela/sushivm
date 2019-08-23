
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
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "lib_os.h"

static int sleep_seconds(lua_State* state)
{
	long n = luaL_checknumber(state, 2);
	sleep(n);
	return 0;
}

static int sleep_microseconds(lua_State* state)
{
	long n = luaL_checknumber(state, 2);
	usleep(n);
	return 0;
}

static int sleep_milliseconds(lua_State* state)
{
	long n = luaL_checknumber(state, 2);
	usleep(n * 1000);
	return 0;
}

static int get_environment_variable(lua_State* state)
{
	const char* varname = lua_tostring(state, 2);
	if(varname == NULL) {
		return 0;
	}
	lua_pushstring(state, getenv(varname));
	return 1;
}

static int get_system_type(lua_State* state)
{
	// FIXME: Include the different possible values here
	lua_pushstring(state, "linux");
	return 1;
}

static int get_system_time_seconds(lua_State* state)
{
	tzset();
	lua_pushnumber(state, time(NULL) - timezone);
	return 1;
}

static int get_system_time_milliseconds(lua_State* state)
{
	tzset();
	struct timeval tv;
	gettimeofday(&tv, (void*)0);
	long v = tv.tv_sec * 1000L + tv.tv_usec / 1000L;
	lua_pushnumber(state, v);
	return 1;
}

static int get_system_timezone_seconds(lua_State* state)
{
	tzset();
	lua_pushnumber(state, timezone);
	return 1;
}

static int do_start_process(lua_State* state, int doFork, int fdin, int fdout, int fderr)
{
	const char* ocmd = lua_tostring(state, 1);
	if(ocmd == NULL) {
		return -1;
	}
	const char* cmd = ocmd;
	int issupp = 0;
	int cmdlen = strlen(cmd);
	if(cmdlen >= 5 && strcmp(cmd + cmdlen - 5, ".supp") == 0) {
		issupp = 1;
	}
	int ntop = 0;
	if(lua_isnil(state, 2) == 0 && lua_istable(state, 2) == 1) {
		ntop = lua_objlen(state, 2);
	}
	int nargs = issupp + 1 + ntop;
	char** args = (char**)malloc((nargs + 1) * sizeof(char*));
	int n = 0;
	if(issupp == 1) {
		cmd = sushi_get_executable_path();
		if(cmd == NULL) {
			cmd = "sushi";
		}
		args[n++] = (char*)cmd;
	}
	args[n++] = (char*)ocmd;
	if(ntop > 0) {
		for(int i=1; i<=ntop; i++) {
			lua_rawgeti(state, 2, i);
			int cidx = lua_gettop(state);
			if(lua_isnil(state, cidx) == 0) {
				const char* param = lua_tostring(state, lua_gettop(state));
				if(param != NULL) {
					args[n++] = (char*)param;
				}
			}
			lua_pop(state, 1);
		}
	}
	args[n++] = NULL;
	char** envp = NULL;
	if(lua_isnil(state, 3) == 0 && lua_istable(state, 3) == 1) {
		int envsz = lua_objlen(state, 3);
		if(envsz > 0) {
			envp = (char**)malloc((envsz + 1) * sizeof(char*));
			int en = 0;
			for(int i=1; i<=envsz; i++) {
				lua_rawgeti(state, 3, i);
				int cidx = lua_gettop(state);
				if(lua_isnil(state, cidx) == 0) {
					const char* param = lua_tostring(state, lua_gettop(state));
					if(param != NULL) {
						envp[en++] = (char*)param;
					}
				}
				lua_pop(state, 1);
			}
			envp[en++] = NULL;
		}
	}
	const char* cwd = NULL;
	if(lua_isnil(state, 4) == 0 && lua_isstring(state, 4) == 1) {
		cwd = lua_tostring(state, 4);
	}
	if(doFork == 1) {
		int pid = fork();
		if(pid != 0) {
			free(args);
			return pid;
		}
	}
	if(fdin >= 0) {
		dup2(fdin, STDIN_FILENO);
		close(fdin);
	}
	if(fdout >= 0) {
		dup2(fdout, STDOUT_FILENO);
		close(fdout);
	}
	if(fderr >= 0) {
		dup2(fderr, STDERR_FILENO);
		close(fderr);
	}
	if(cwd != NULL) {
		chdir(cwd);
	}
	execve(cmd, args, envp);
	free(args);
	return -1;
}

static int start_process(lua_State* state)
{
	lua_remove(state, 1);
	lua_pushnumber(state, do_start_process(state, 1, -1, -1, -1));
	return 1;
}

static int start_piped_process(lua_State* state)
{
	int fdin[2];
	int fdout[2];
	int fderr[2];
	fdin[0] = -1;
	fdin[1] = -1;
	fdout[0] = -1;
	fdout[1] = -1;
	fderr[0] = -1;
	fderr[1] = -1;
	int v1 = pipe(fdin);
	int v2 = pipe(fdout);
	int v3 = pipe(fderr);
	if(v1 != 0 || v2 != 0 || v3 != 0) {
		if(fdin[0] >= 0) { close(fdin[0]); }
		if(fdin[1] >= 0) { close(fdin[1]); }
		if(fdout[0] >= 0) { close(fdout[0]); }
		if(fdout[1] >= 0) { close(fdout[1]); }
		if(fderr[0] >= 0) { close(fderr[0]); }
		if(fderr[1] >= 0) { close(fderr[1]); }
		lua_pushnumber(state, -1);
		return 1;
	}
	int pid = do_start_process(state, 1, fdin[0], fdout[1], fderr[1]);
	close(fdin[0]);
	close(fdout[1]);
	close(fderr[1]);
	if(pid < 0) {
		close(fdin[1]);
		close(fdout[0]);
		close(fderr[0]);
		fdin[1] = -1;
		fdout[0] = -1;
		fderr[0] = -1;
	}
	lua_pushnumber(state, pid);
	lua_pushnumber(state, fdin[1]);
	lua_pushnumber(state, fdout[0]);
	lua_pushnumber(state, fderr[0]);
	return 4;
}

static int replace_process(lua_State* state)
{
	lua_remove(state, 1);
	lua_pushnumber(state, do_start_process(state, 0, -1, -1, -1));
	return 1;
}

static int wait_for_process(lua_State* state)
{
	int pid = luaL_checknumber(state, 2);
	int status = 0;
	waitpid(pid, &status, 0);
	lua_pushnumber(state, WEXITSTATUS(status));
	return 1;
}

static int check_process(lua_State* state)
{
	int pid = luaL_checknumber(state, 2);
	int status = 0;
	int r = waitpid(pid, &status, WNOHANG);
	if(r <= 0) {
		lua_pushnumber(state, 1);
	}
	else {
		lua_pushnumber(state, 0);
	}
	return 1;
}

static int send_process_signal(lua_State* state)
{
	int pid = luaL_checknumber(state, 2);
	int signal = luaL_checknumber(state, 3);
	kill(pid, signal);
	return 0;
}

static int disown_process(lua_State* state)
{
	// FIXME
	return 0;
}

/*
static int startThread(lua_State* state)
{
	lua_remove(state, 1);
	ThreadArgs* args = thread_args_create();
    unsigned long codeLen;
    const char *codeBuf = lua_tolstring(state, 1, &codeLen);
	if(codeBuf != NULL && codeLen > 0) {
		args->code = buffer_create(codeBuf, codeLen);
	}
	unsigned long paramLen;
	const char* paramBuf = lua_tolstring(state, 2, &paramLen);
	if(paramBuf != NULL && paramLen > 0) {
		args->param = buffer_create(paramBuf, paramLen);
	}
	args->state = NULL;
	pthread_t thread;
	if(pthread_create(&thread, NULL, _threadMain, (void*)args) != 0) {
		thread_args_free(args);
		lua_pushnumber(state, 0);
		return 1;
	}
	lua_pushnumber(state, (int)thread);
	return 1;
}

static void* _threadMain(void* arg)
{
	ThreadArgs* args = (ThreadArgs*)arg;
	if(args == NULL) {
		return (void*)1;
	}
	void* rv = 0;
	lua_State* state = sushi_create_new_state();
	if(state != NULL) {
		if(args->code != NULL) {
			luaL_loadbuffer(state, args->code->pointer, args->code->size, "__code__");
			lua_call(state, 0, 0);
		}
		lua_getglobal(state, "_threadMain");
		if(lua_isnil(state, lua_gettop(state)) == 0) {
			int params = 0;
			if(args->param != NULL) {
				lua_pushlstring(state, args->param->pointer, args->param->size);
				params ++;
			}
			if(lua_pcall(state, params, 0, 1) != 0) {
				sushi_error("Error while running _threadMain: `%s'", lua_tostring(state, -1));
				rv = (void*)1;
			}
		}
		lua_close(state);
	}
	thread_args_free(args);
	return (void*)rv;
}
*/

static const luaL_Reg funcs[] = {
	{ "sleep_seconds", sleep_seconds },
	{ "sleep_microseconds", sleep_microseconds },
	{ "sleep_milliseconds", sleep_milliseconds },
	{ "get_environment_variable", get_environment_variable },
	{ "get_system_type", get_system_type },
	{ "get_system_time_seconds", get_system_time_seconds },
	{ "get_system_time_milliseconds", get_system_time_milliseconds },
	{ "get_system_timezone_seconds", get_system_timezone_seconds },
	{ "start_process", start_process },
	{ "start_piped_process", start_piped_process },
	{ "replace_process", replace_process },
	{ "wait_for_process", wait_for_process },
	{ "check_process", check_process },
	{ "send_process_signal", send_process_signal },
	{ "disown_process", disown_process },
	{ NULL, NULL }
};

void lib_os_init(lua_State* state)
{
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_os");
}
