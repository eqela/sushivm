
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
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include "lib_os.h"

#define SIGMAX 64

static int signal_trapped[SIGMAX];
static int signal_flags[SIGMAX];
static int signal_handlers_initialized = 0;
extern char **environ;

int sleep_seconds(lua_State* state)
{
	long n = luaL_checknumber(state, 2);
	sleep(n);
	return 0;
}

int sleep_microseconds(lua_State* state)
{
	long n = luaL_checknumber(state, 2);
	usleep(n);
	return 0;
}

int sleep_milliseconds(lua_State* state)
{
	long n = luaL_checknumber(state, 2);
	usleep(n * 1000);
	return 0;
}

int get_all_environment_variables(lua_State* state)
{
	int n = 1;
	lua_newtable(state);
	int table = lua_gettop(state);
	char** p = environ;
	if(p != NULL) {
		while(*p != NULL) {
			lua_pushstring(state, *p);
			lua_rawseti(state, table, n++);
			p++;
		}
	}
	lua_pushstring(state, "n");
	lua_pushnumber(state, n-1);
	lua_rawset(state, table);
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
#if defined(SUSHI_SUPPORT_LINUX)
	lua_pushstring(state, "linux");
#elif defined(SUSHI_SUPPORT_MACOS)
	lua_pushstring(state, "macos");
#else
#error System type is not defined
#endif
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
	time_t tp = (time_t)luaL_checknumber(state, 2);
	struct tm lt;
	memset(&lt, 0, sizeof(struct tm));
	localtime_r(&tp, &lt);
	lua_pushnumber(state, lt.tm_wday + 1); // day of week
	lua_pushnumber(state, lt.tm_mday); // day of month
	lua_pushnumber(state, lt.tm_mon + 1); // month
	lua_pushnumber(state, 1900 + lt.tm_year); // year
	lua_pushnumber(state, lt.tm_hour); // hours
	lua_pushnumber(state, lt.tm_min); // minutes
	lua_pushnumber(state, lt.tm_sec); // seconds
	return 7;
}

int get_timestamp_details_utc(lua_State* state)
{
	time_t tp = (time_t)luaL_checknumber(state, 2);
	struct tm lt;
	memset(&lt, 0, sizeof(struct tm));
	gmtime_r(&tp, &lt);
	lua_pushnumber(state, lt.tm_wday + 1); // day of week
	lua_pushnumber(state, lt.tm_mday); // day of month
	lua_pushnumber(state, lt.tm_mon + 1); // month
	lua_pushnumber(state, 1900 + lt.tm_year); // year
	lua_pushnumber(state, lt.tm_hour); // hours
	lua_pushnumber(state, lt.tm_min); // minutes
	lua_pushnumber(state, lt.tm_sec); // seconds
	return 7;
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
	int detachFromTerminal = 0;
	if(lua_isnil(state, 5) == 0 && lua_isnumber(state, 5) == 1) {
		detachFromTerminal = lua_tonumber(state, 5);
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
	if(detachFromTerminal != 0) {
		if(setsid() < 0) {
			sushi_error("setsid failed");
		}
		if(fdin < 0) {
			close(STDIN_FILENO);
		}
		if(fdout < 0) {
			close(STDOUT_FILENO);
		}
		if(fderr < 0) {
			close(STDERR_FILENO);
		}
	}
	execve(cmd, args, envp);
	free(args);
	return -1;
}

int start_process(lua_State* state)
{
	lua_remove(state, 1);
	lua_pushnumber(state, do_start_process(state, 1, -1, -1, -1));
	return 1;
}

int start_piped_process(lua_State* state)
{
	lua_remove(state, 1);
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

int replace_process(lua_State* state)
{
	lua_remove(state, 1);
	lua_pushnumber(state, do_start_process(state, 0, -1, -1, -1));
	return 1;
}

int wait_for_process(lua_State* state)
{
	int pid = luaL_checknumber(state, 2);
	int status = 0;
	waitpid(pid, &status, 0);
	lua_pushnumber(state, WEXITSTATUS(status));
	return 1;
}

int is_process_alive(lua_State* state)
{
	int pid = luaL_checknumber(state, 2);
	int r = kill(pid, 0);
	if(r == 0) {
		lua_pushnumber(state, 1);
	}
	else {
		lua_pushnumber(state, 0);
	}
	return 1;
}

int send_process_signal(lua_State* state)
{
	int pid = luaL_checknumber(state, 2);
	int signal = luaL_checknumber(state, 3);
	kill(pid, signal);
	return 0;
}

void _init_signals()
{
	if(signal_handlers_initialized == 0) {
		int n;
		for(n=0; n<SIGMAX; n++) {
			signal_trapped[n] = 0;
			signal_flags[n] = 0;
		}
		signal_handlers_initialized = 1;
	}
}

void _signal_handler(int signum)
{
	if(signum >= 0 && signum < SIGMAX) {
		signal_flags[signum] = 1;
	}
}

int check_signal_state(lua_State* state)
{
	_init_signals();
	int v = 0;
	int signum = luaL_checknumber(state, 2);
	if(signum >= 0 && signum < SIGMAX) {
		v = signal_flags[signum];
		if(v == 1) {
			signal_flags[signum] = 0;
		}
	}
	lua_pushnumber(state, v);
	return 1;
}

int trap_signal(lua_State* state)
{
	_init_signals();
	int v = 0;
	int signum = luaL_checknumber(state, 2);
	int enabled = luaL_checknumber(state, 3);
	if(signum >= 0 && signum < SIGMAX) {
		v = signal_trapped[signum];
		if(enabled == 1) {
			if(signal_trapped[signum] == 0) {
				signal(signum, _signal_handler);
				signal_trapped[signum] = 1;
			}
		}
		else {
			if(signal_trapped[signum] == 1) {
				signal(signum, SIG_DFL);
				signal_trapped[signum] = 0;
				signal_flags[signum] = 0;
			}
		}
	}
	lua_pushnumber(state, v);
	return 1;
}

int disown_process(lua_State* state)
{
	// FIXME
	return 0;
}

static void* _execute_in_thread_main(void* arg)
{
	lua_State* state = (lua_State*)arg;
	if(lua_isfunction(state, lua_gettop(state)) == 1) {
		if(sushi_pcall(state, 0, 0) != 0) {
			sushi_error("Error while executing thread: `%s'", sushi_error_to_string(state));
		}
	}
	lua_gc(state, LUA_GCCOLLECT, 0);
	int reuse = 0;
	lua_getglobal(state, "_reuseInterpreter");
	if(lua_isnumber(state, -1)) {
		reuse = lua_tonumber(state, -1);
	}
	lua_getglobal(state, "_pipefd");
	if(lua_isnumber(state, -1)) {
		int pipefd = lua_tonumber(state, -1);
		if(pipefd >= 0) {
			close(pipefd);
		}
	}
	if(reuse == 0) {
		lua_close(state);
	}
	return NULL;
}

int execute_in_thread(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 2, "_sushi_interpreter");
	if(ptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	lua_State* nstate = NULL;
	memcpy(&nstate, ptr, sizeof(lua_State*));
	if(nstate == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	const char *fcode = luaL_checkstring(state, 3);
	if(fcode == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	void* inputptr = NULL;
	long inputsize = 0;
	if(lua_isuserdata(state, 4)) {
		inputptr = luaL_checkudata(state, 4, "_sushi_buffer");
		if(inputptr == NULL) {
			lua_pushnumber(state, -1);
			return 1;
		}
		memcpy(&inputsize, inputptr, sizeof(long));
		inputptr += sizeof(long);
	}
	long withPipe = luaL_checknumber(state, 5);
	long reuseInterpreter = luaL_checknumber(state, 6);
	// input parameter for the thread code
	if(inputptr != NULL && inputsize > 0) {
		void* ptr = lua_newuserdata(nstate, sizeof(long) + (size_t)inputsize);
		luaL_getmetatable(nstate, "_sushi_buffer");
		lua_setmetatable(nstate, -2);
		memcpy(ptr, &inputsize, sizeof(long));
		memcpy(ptr + sizeof(long), inputptr, inputsize);
	}
	else {
		lua_pushnil(nstate);
	}
	lua_setglobal(nstate, "_input");
	lua_pushnumber(nstate, reuseInterpreter);
	lua_setglobal(nstate, "_reuseInterpreter");
	lua_pushnil(nstate);
	lua_setglobal(nstate, "_args");
	// load the given code and push it as function on top of the stack
	if(luaL_loadbuffer(nstate, fcode, strlen(fcode), "__code__") != 0) {
		sushi_error("Failed to load code buffer: `%s'", sushi_error_to_string(nstate));
		lua_pushnumber(state, -1);
		return 1;
	}
	// pipe objects
	int pipefds[2];
	pipefds[0] = -1;
	pipefds[1] = -1;
	int v = 0;
	if(withPipe == 1) {
		if(pipe(pipefds) != 0) {
			lua_pushnumber(state, -1);
			return 1;
		}
		lua_pushnumber(nstate, pipefds[1]);
		lua_setglobal(nstate, "_pipefd");
		v = pipefds[0];
	}
	// start thread
	pthread_t thread;
	if(pthread_create(&thread, NULL, _execute_in_thread_main, (void*)nstate) != 0) {
		sushi_error("Failed in pthread_create");
		if(pipefds[0] >= 0) {
			close(pipefds[0]);
		}
		if(pipefds[1] >= 0) {
			close(pipefds[1]);
		}
		lua_pushnumber(state, -1);
		return 1;
	}
	if(reuseInterpreter == 0) {
		memset(ptr, 0, sizeof(lua_State*));
	}
	pthread_detach(thread);
	lua_pushnumber(state, v);
	return 1;
}
