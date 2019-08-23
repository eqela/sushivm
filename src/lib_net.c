
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
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "lib_net.h"

static int create_io_manager(lua_State* state)
{
	lua_pushnumber(state, epoll_create(1));
	return 1;
}

static int register_io_listener(lua_State* state)
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

static int remove_io_listener(lua_State* state)
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
		if(epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event) != 0) {
			v = 0;
		}
	}
	lua_pushnumber(state, v);
	return 1;
}

static void _callLuaMethodWithObjref(lua_State* state, int objref, const char* method)
{
	lua_remove(state, 1);
	int otop = lua_gettop(state);
	lua_rawgeti(state, LUA_REGISTRYINDEX, objref);
	if(otop == lua_gettop(state)) {
		return;
	}
	if(lua_isnil(state, -1)) {
		return;
	}
	lua_pushstring(state, method);
	lua_gettable(state, -2);
	lua_rawgeti(state, LUA_REGISTRYINDEX, objref);
	lua_call(state, 1, 0);
	lua_pop(state, 1);
}

static int execute_io_manager(lua_State* state)
{
	lua_remove(state, 1);
	int epollfd = luaL_checknumber(state, 1);
	int timeout = luaL_checknumber(state, 2);
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
		if(event->events & EPOLLIN) {
			_callLuaMethodWithObjref(state, objref, "onReadReady");
		}
		if(event->events & EPOLLOUT) {
			_callLuaMethodWithObjref(state, objref, "onWriteReady");
		}
	}
	return r;
}

static int close_io_manager(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd >= 0) {
		close(fd);
	}
	return 0;
}

static int create_tcp_socket(lua_State* state)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	lua_pushnumber(state, fd);
	return 1;
}

static int listen_tcp_socket(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	int port = luaL_checknumber(state, 2);
	if(port < 1) {
		lua_pushnumber(state, 1);
		return 1;
	}
	int v = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int));
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) != 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	if(listen(fd, 256) != 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	lua_pushnumber(state, 0);
	return 1;
}

static int connect_tcp_socket(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	const char* host = lua_tostring(state, 2);
	if(host == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int port = luaL_checknumber(state, 3);
	if(port < 1) {
		lua_pushnumber(state, -1);
		return 1;
	}
	char sport[256];
	snprintf(sport, 255, "%d", port);
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo(host, sport, &hints, &res) != 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	if(res == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int r = connect(fd, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	if(r != 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	/*
	struct hostent* hostent = gethostbyname(host);
	if(hostent == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memcpy(&addr.sin_addr, hostent->h_addr_list[0], hostent->h_length);
	if(connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) != 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	*/
	lua_pushnumber(state, 0);
	return 1;
}

static int get_tcp_socket_peer_address(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		return 0;
	}
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	socklen_t addrlen = sizeof(struct sockaddr_in);
	int r = getpeername(fd, (struct sockaddr*)&addr, &addrlen);
	if(r < 0) {
		return 0;
	}
	lua_pushstring(state, inet_ntoa(addr.sin_addr));
	return 1;
}

static int get_tcp_socket_peer_port(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		return 0;
	}
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	socklen_t addrlen = sizeof(struct sockaddr_in);
	int r = getpeername(fd, (struct sockaddr*)&addr, &addrlen);
	if(r < 0) {
		return 0;
	}
	lua_pushnumber(state, ntohs(addr.sin_port));
	return 1;
}

static int read_from_tcp_socket(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	void* ptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int size = luaL_checknumber(state, 3);
	if(size < 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	if(size == 0) {
		lua_pushnumber(state, 0);
		return 1;
	}
	int timeout = luaL_checknumber(state, 4);
	// FIXME: Handle timeout
	int r = read(fd, ptr+sizeof(long), size);
	if(r > 0) {
		; // all good
	}
	else if(r == 0) {
		r = -1;
	}
	else {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			r = 0;
		}
	}
	lua_pushnumber(state, r);
	return 1;
}

static int write_to_tcp_socket(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	void* ptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	long size = luaL_checknumber(state, 3);
	if(size < 0) {
		memcpy(&size, ptr, sizeof(long));
	}
	if(size == 0) {
		lua_pushnumber(state, 0);
		return 1;
	}
	int r = write(fd, ptr + sizeof(long), size);
	if(r == 0) {
		r = -1;
	}
	lua_pushnumber(state, r);
	return 1;
}

static int accept_tcp_socket(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int r = accept(fd, NULL, NULL);
	lua_pushnumber(state, r);
	return 1;
}

static int close_tcp_socket(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd >= 0) {
		close(fd);
	}
	return 0;
}

static const luaL_Reg funcs[] = {
	{ "create_io_manager", create_io_manager },
	{ "register_io_listener", register_io_listener },
	{ "remove_io_listener", remove_io_listener },
	{ "execute_io_manager", execute_io_manager },
	{ "close_io_manager", close_io_manager },
	{ "create_tcp_socket", create_tcp_socket },
	{ "listen_tcp_socket", listen_tcp_socket },
	{ "connect_tcp_socket", connect_tcp_socket },
	{ "get_tcp_socket_peer_address", get_tcp_socket_peer_address },
	{ "get_tcp_socket_peer_port", get_tcp_socket_peer_port },
	{ "read_from_tcp_socket", read_from_tcp_socket },
	{ "write_to_tcp_socket", write_to_tcp_socket },
	{ "accept_tcp_socket", accept_tcp_socket },
	{ "close_tcp_socket", close_tcp_socket },
	{ NULL, NULL }
};

void lib_net_init(lua_State* state)
{
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_net");
}
