
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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "lib_net.h"

int create_io_manager(lua_State* state)
{
	int v = epoll_create(1);
	if(v < 0) {
		lua_pushnil(state);
		return 1;
	}
	lua_pushnumber(state, v);
	return 1;
}

int register_io_listener(lua_State* state)
{
	lua_remove(state, 1);
	int epollfd = luaL_checknumber(state, 1);
	int fd = luaL_checknumber(state, 2);
	int mode = luaL_checknumber(state, 3);
	if(epollfd < 0 || fd < 0) {
		lua_pushnumber(state, 0);
		return 1;
	}
	struct epoll_event event;
	memset(&event, 0, sizeof(struct epoll_event));
	if(mode == 0) {
		event.events = EPOLLIN;
	}
	else if(mode == 1) {
		event.events = EPOLLOUT;
	}
	else if(mode == 2) {
		event.events = EPOLLIN | EPOLLOUT;
	}
	else {
		lua_pushnumber(state, 0);
		return 1;
	}
	int objref = luaL_ref(state, LUA_REGISTRYINDEX);
	if(objref < 1) {
		lua_pushnumber(state, 0);
		return 1;
	}
	event.data.fd = objref;
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) != 0) {
		luaL_unref(state, LUA_REGISTRYINDEX, objref);
		lua_pushnumber(state, 0);
		return 1;
	}
	lua_pushnumber(state, objref);
	return 1;
}

int update_io_listener(lua_State* state)
{
	int epollfd = luaL_checknumber(state, 2);
	int fd = luaL_checknumber(state, 3);
	int mode = luaL_checknumber(state, 4);
	int objref = luaL_checknumber(state, 5);
	if(epollfd < 0 || fd < 0 || objref < 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	struct epoll_event event;
	memset(&event, 0, sizeof(struct epoll_event));
	if(mode == 0) {
		event.events = EPOLLIN;
	}
	else if(mode == 1) {
		event.events = EPOLLOUT;
	}
	else if(mode == 2) {
		event.events = EPOLLIN | EPOLLOUT;
	}
	else {
		lua_pushnumber(state, 1);
		return 1;
	}
	event.data.fd = objref;
	if(epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) != 0) {
		// error
		lua_pushnumber(state, 1);
	}
	else {
		// success
		lua_pushnumber(state, 0);
	}
	return 1;
}

int remove_io_listener(lua_State* state)
{
	int v = 1;
	lua_remove(state, 1);
	int epollfd = luaL_checknumber(state, 1);
	int fd = luaL_checknumber(state, 2);
	int objref = luaL_checknumber(state, 3);
	if(objref > 0) {
		luaL_unref(state, LUA_REGISTRYINDEX, objref);
	}
	if(epollfd >= 0 && fd >= 0) {
		struct epoll_event event;
		memset(&event, 0, sizeof(struct epoll_event));
		if(epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event) != 0) {
			v = 0;
		}
	}
	lua_pushnumber(state, v);
	return 1;
}

void _callLuaMethodWithObjref(lua_State* state, int objref, const char* method)
{
	int otop = lua_gettop(state);
	lua_rawgeti(state, LUA_REGISTRYINDEX, objref);
	if(otop == lua_gettop(state)) {
		return;
	}
	if(!lua_istable(state, -1)) {
		return;
	}
	lua_pushstring(state, method);
	lua_gettable(state, -2);
	if(lua_isfunction(state, -1)) {
		lua_rawgeti(state, LUA_REGISTRYINDEX, objref);
		lua_call(state, 1, 0);
	}
	else {
		lua_pop(state, 1);
	}
}

int execute_io_manager(lua_State* state)
{
	int epollfd = luaL_checknumber(state, 2);
	int timeout = luaL_checknumber(state, 3);
	struct epoll_event events[1024];
	int r = epoll_pwait(epollfd, events, 1024, timeout, NULL);
	if(r < 0) {
		if(errno == EINTR) {
			lua_pushnumber(state, 0);
		}
		else {
			lua_pushnumber(state, -1);
		}
		return 1;
	}
	int n;
	for(n=0; n<r; n++) {
		struct epoll_event* event = &(events[n]);
		int objref = event->data.fd;
		if(objref < 1) {
			continue;
		}
		if(event->events & EPOLLIN && event->events & EPOLLOUT) {
			_callLuaMethodWithObjref(state, objref, "onReadWriteReady");
		}
		else if(event->events & EPOLLIN) {
			_callLuaMethodWithObjref(state, objref, "onReadReady");
		}
		else if(event->events & EPOLLOUT) {
			_callLuaMethodWithObjref(state, objref, "onWriteReady");
		}
	}
	lua_pushnumber(state, r);
	return 1;
}

int close_io_manager(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd >= 0) {
		close(fd);
	}
	return 0;
}