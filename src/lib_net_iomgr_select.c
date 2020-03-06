
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

#include <string.h>
#include <sys/select.h>
#include <errno.h>
#include "sushi.h"
#include "lib_net.h"

#define MAX_IOMGR_ENTRIES 1024

struct entry
{
	int fd;
	int mode;
	int objref;
};

struct iomgr
{
	struct entry entries[MAX_IOMGR_ENTRIES];
};

int find_free_entry_index(struct iomgr* iomgr)
{
	if(iomgr == NULL) {
		return -1;
	}
	for(int n=0; n<MAX_IOMGR_ENTRIES; n++) {
		if(iomgr->entries[n].fd < 0) {
			return n;
		}
	}
	return -1;
}

int find_entry_index_for_fd(struct iomgr* iomgr, int fd)
{
	if(iomgr == NULL) {
		return -1;
	}
	for(int n=0; n<MAX_IOMGR_ENTRIES; n++) {
		if(iomgr->entries[n].fd == fd) {
			return n;
		}
	}
	return -1;
}

int create_io_manager(lua_State* state)
{
	luaL_newmetatable(state, "_sushi_iomgr");
	struct iomgr* v = (struct iomgr*)lua_newuserdata(state, sizeof(struct iomgr));
	luaL_getmetatable(state, "_sushi_iomgr");
	lua_setmetatable(state, -2);
	memset(v, 0, sizeof(struct iomgr));
	for(int n=0; n<MAX_IOMGR_ENTRIES; n++) {
		v->entries[n].fd = -1;
	}
	return 1;
}

int register_io_listener(lua_State* state)
{
	struct iomgr* iomgr = (struct iomgr*)luaL_checkudata(state, 2, "_sushi_iomgr");
	int fd = luaL_checknumber(state, 3);
	int mode = luaL_checknumber(state, 4);
	if(iomgr == NULL || fd < 0) {
		lua_pushnumber(state, 0);
		return 1;
	}
	if(fd > 1023) {
		sushi_error("register_io_listener: tried to register fd %d > 1023", fd);
		lua_pushnumber(state, 0);
		return 1;
	}
	if(mode < 0 || mode > 2) {
		lua_pushnumber(state, 0);
		return 1;
	}
	int index = find_free_entry_index(iomgr);
	if(index < 0) {
		sushi_error("register_io_listener: failed to find free entry index");
		lua_pushnumber(state, 0);
		return 1;
	}
	int objref = luaL_ref(state, LUA_REGISTRYINDEX);
	if(objref < 1) {
		lua_pushnumber(state, 0);
		return 1;
	}
	iomgr->entries[index].fd = fd;
	iomgr->entries[index].mode = mode;
	iomgr->entries[index].objref = objref;
	lua_pushnumber(state, objref);
	return 1;
}

int update_io_listener(lua_State* state)
{
	struct iomgr* iomgr = (struct iomgr*)luaL_checkudata(state, 2, "_sushi_iomgr");
	int fd = luaL_checknumber(state, 3);
	int mode = luaL_checknumber(state, 4);
	int objref = luaL_checknumber(state, 5);
	if(iomgr == NULL || fd < 0 || objref < 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	if(mode < 0 || mode > 2) {
		lua_pushnumber(state, 1);
		return 1;
	}
	int index = find_entry_index_for_fd(iomgr, fd);
	if(index < 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	iomgr->entries[index].mode = mode;
	iomgr->entries[index].objref = objref;
	lua_pushnumber(state, 0);
	return 1;
}

int remove_io_listener(lua_State* state)
{
	int v = 1;
	struct iomgr* iomgr = (struct iomgr*)luaL_checkudata(state, 2, "_sushi_iomgr");
	int fd = luaL_checknumber(state, 3);
	int objref = luaL_checknumber(state, 4);
	if(objref > 0) {
		luaL_unref(state, LUA_REGISTRYINDEX, objref);
	}
	if(iomgr != NULL && fd >= 0) {
		int index = find_entry_index_for_fd(iomgr, fd);
		if(index >= 0) {
			iomgr->entries[index].fd = -1;
			iomgr->entries[index].objref = 0;
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
	struct iomgr* iomgr = (struct iomgr*)luaL_checkudata(state, 2, "_sushi_iomgr");
	if(iomgr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int timeout = luaL_checknumber(state, 3);
	fd_set readset;
	fd_set writeset;
	FD_ZERO(&readset);
	FD_ZERO(&writeset);
	int hfd = 0;
	for(int n=0; n<MAX_IOMGR_ENTRIES; n++) {
		int fd = iomgr->entries[n].fd;
		if(fd >= 0) {
			int mode = iomgr->entries[n].mode;
			if(mode == 0) {
				FD_SET(fd, &readset);
			}
			else if(mode == 1) {
				FD_SET(fd, &writeset);
			}
			else if(mode == 2) {
				FD_SET(fd, &readset);
				FD_SET(fd, &writeset);
			}
			if(fd > hfd) {
				hfd = fd;
			}
		}
	}
	struct timeval* tp = NULL;
	struct timeval tv;
	if(timeout < 0) {
		// block indefinitely
		tp = NULL;
	}
	else if(timeout == 0) {
		// return immediately
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		tp = &tv;
	}
	else {
		// timeout
		tv.tv_sec = timeout / 1000;
		int tr = timeout % 1000;
		tv.tv_usec = tr * 1000;
		tp = &tv;
	}
	int r = select(hfd + 1, &readset, &writeset, NULL, tp);
	if(r < 0) {
		if(errno == EINTR) {
			lua_pushnumber(state, 0);
		}
		else {
			sushi_error("select: %s", strerror(errno));
			lua_pushnumber(state, -1);
		}
		return 1;
	}
	if(r > 0) {
		for(int n=0; n<MAX_IOMGR_ENTRIES; n++) {
			int fd = iomgr->entries[n].fd;
			if(fd >= 0) {
				int objref = iomgr->entries[n].objref;
				if(objref > 0) {
					int rr = FD_ISSET(fd, &readset);
					int wr = FD_ISSET(fd, &writeset);
					if(rr && wr) {
						_callLuaMethodWithObjref(state, objref, "onReadWriteReady");
					}
					else if (rr) {
						_callLuaMethodWithObjref(state, objref, "onReadReady");
					}
					else if(wr) {
						_callLuaMethodWithObjref(state, objref, "onWriteReady");
					}
				}
			}
		}
	}
	lua_pushnumber(state, r);
	return 1;
}

int close_io_manager(lua_State* state)
{
	struct iomgr* iomgr = (struct iomgr*)luaL_checkudata(state, 2, "_sushi_iomgr");
	if(iomgr != NULL) {
		for(int n=0; n<MAX_IOMGR_ENTRIES; n++) {
			if(iomgr->entries[n].fd >= 0 && iomgr->entries[n].objref > 0) {
				luaL_unref(state, LUA_REGISTRYINDEX, iomgr->entries[n].objref);
			}
		}
	}
	return 0;
}
