
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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include "lib_net.h"

#if defined(SUSHI_SUPPORT_LINUX)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#endif

#if defined(SUSHI_SUPPORT_MACOS)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#endif

#if defined(SUSHI_SUPPORT_WIN32)
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#endif

int create_io_manager(lua_State* state);
int register_io_listener(lua_State* state);
int update_io_listener(lua_State* state);
int remove_io_listener(lua_State* state);
int execute_io_manager(lua_State* state);
int close_io_manager(lua_State* state);

static int create_udp_socket(lua_State* state)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	lua_pushnumber(state, fd);
	return 1;
}

static int send_udp_data(lua_State* state)
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
	const char* address = lua_tostring(state, 4);
	if(address == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int port = luaL_checknumber(state, 5);
	if(port < 1) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int broadcastFlag = luaL_checknumber(state, 6);
	if(broadcastFlag != 0 && broadcastFlag != 1) {
		broadcastFlag = 0;
	}
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(address);
	server_addr.sin_port = htons(port);
	if(broadcastFlag == 1) {
		setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void*)&broadcastFlag, sizeof(int));
	}
	int r = sendto(fd, ptr + sizeof(long), size, 0, (struct sockaddr*)(&server_addr), sizeof(struct sockaddr_in));
	if(broadcastFlag == 1) {
		broadcastFlag = 0;
		setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void*)&broadcastFlag, sizeof(int));
	}
	if(r == 0) {
		r = -1;
	}
	lua_pushnumber(state, r);
	return 1;
}

static int read_udp_data(lua_State* state)
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
	long bsz;
	memcpy(&bsz, ptr, sizeof(long));
	int size = luaL_checknumber(state, 3);
	if(size < 0 || size > bsz) {
		size = bsz;
	}
	if(size == 0) {
		lua_pushnumber(state, 0);
		return 1;
	}
	int timeout = luaL_checknumber(state, 4);
	if(timeout > 0) {
#if defined(SUSHI_SUPPORT_WIN32)
		DWORD toval = timeout / 1000;
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&toval, sizeof(DWORD));
#else
		struct timeval tv;
		tv.tv_sec = timeout / 1000000;
		tv.tv_usec = timeout % 1000000;
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(struct timeval));
#endif
	}
	struct sockaddr_in peeraddr;
	memset(&peeraddr, 0, sizeof(struct sockaddr_in));
	socklen_t s = sizeof(struct sockaddr_in);
	int r = recvfrom(fd, ptr+sizeof(long), size, 0, (struct sockaddr*)&peeraddr, &s);
	if(timeout > 0) {
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, NULL, 0);
	}
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
	if(r > 0) {
		lua_pushstring(state, inet_ntoa(peeraddr.sin_addr));
		lua_pushnumber(state, ntohs(peeraddr.sin_port));
		return 3;
	}
	return 1;
}

static int bind_udp_port(lua_State* state)
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
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	if(bind(fd, (struct sockaddr*)(&server_addr), sizeof(struct sockaddr_in)) != 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	lua_pushnumber(state, 0);
	return 1;
}

static int get_udp_socket_local_address(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		return 0;
	}
	socklen_t s = (socklen_t)sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	int r = getsockname(fd, (struct sockaddr*)(&addr), &s);
	if(r < 0) {
		return 0;
	}
	lua_pushstring(state, inet_ntoa(addr.sin_addr));
	lua_pushnumber(state, ntohs(addr.sin_port));
	return 2;
}

static int close_udp_socket(lua_State* state)
{
	int fd = luaL_checknumber(state, 2);
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

static int set_socket_non_blocking(lua_State* state)
{
#if defined(SUSHI_SUPPORT_LINUX) || defined(SUSHI_SUPPORT_MACOS)
	int fd = luaL_checknumber(state, 2);
	int flags = fcntl(fd, F_GETFL);
	if(flags < 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if(r < 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	lua_pushnumber(state, 0);
	return 1;
#else
	lua_pushnumber(state, 1);
	return 1;
#endif
}

static int set_socket_blocking(lua_State* state)
{
#if defined(SUSHI_SUPPORT_LINUX) || defined(SUSHI_SUPPORT_MACOS)
	int fd = luaL_checknumber(state, 2);
	int flags = fcntl(fd, F_GETFL);
	if(flags < 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	int r = fcntl(fd, F_SETFL, flags & (~O_NONBLOCK));
	if(r < 0) {
		lua_pushnumber(state, 1);
		return 1;
	}
	lua_pushnumber(state, 0);
	return 1;
#else
	lua_pushnumber(state, 1);
	return 1;
#endif
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
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&v, sizeof(int));
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
	hints.ai_family = AF_INET;
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
	lua_pushnumber(state, 0);
	return 1;
}

static int get_tcp_socket_peer_address(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		lua_pushnil(state);
		return 1;
	}
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	socklen_t addrlen = sizeof(struct sockaddr_in);
	int r = getpeername(fd, (struct sockaddr*)&addr, &addrlen);
	if(r < 0) {
		lua_pushnil(state);
		return 1;
	}
	lua_pushstring(state, inet_ntoa(addr.sin_addr));
	return 1;
}

static int get_tcp_socket_peer_port(lua_State* state)
{
	lua_remove(state, 1);
	int fd = luaL_checknumber(state, 1);
	if(fd < 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	socklen_t addrlen = sizeof(struct sockaddr_in);
	int r = getpeername(fd, (struct sockaddr*)&addr, &addrlen);
	if(r < 0) {
		lua_pushnumber(state, -1);
		return 1;
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
	long bsz;
	memcpy(&bsz, ptr, sizeof(long));
	int size = luaL_checknumber(state, 3);
	if(size < 0 || size > bsz) {
		size = bsz;
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
	int fd = luaL_checknumber(state, 2);
	if(fd < 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int r = accept(fd, NULL, NULL);
	lua_pushnumber(state, r);
	if(r < 0 && errno != EAGAIN) {
		lua_pushstring(state, strerror(errno));
	}
	else {
		lua_pushnil(state);
	}
	return 2;
}

static int close_tcp_socket(lua_State* state)
{
	int fd = luaL_checknumber(state, 2);
	if(fd >= 0) {
		close(fd);
	}
	return 0;
}

static const luaL_Reg funcs[] = {
	{ "create_io_manager", create_io_manager },
	{ "register_io_listener", register_io_listener },
	{ "update_io_listener", update_io_listener },
	{ "remove_io_listener", remove_io_listener },
	{ "execute_io_manager", execute_io_manager },
	{ "close_io_manager", close_io_manager },
	{ "create_tcp_socket", create_tcp_socket },
	{ "set_socket_non_blocking", set_socket_non_blocking },
	{ "set_socket_blocking", set_socket_blocking },
	{ "listen_tcp_socket", listen_tcp_socket },
	{ "connect_tcp_socket", connect_tcp_socket },
	{ "get_tcp_socket_peer_address", get_tcp_socket_peer_address },
	{ "get_tcp_socket_peer_port", get_tcp_socket_peer_port },
	{ "read_from_tcp_socket", read_from_tcp_socket },
	{ "write_to_tcp_socket", write_to_tcp_socket },
	{ "accept_tcp_socket", accept_tcp_socket },
	{ "close_tcp_socket", close_tcp_socket },
	{ "create_udp_socket", create_udp_socket },
	{ "send_udp_data", send_udp_data },
	{ "read_udp_data", read_udp_data },
	{ "bind_udp_port", bind_udp_port },
	{ "get_udp_socket_local_address", get_udp_socket_local_address },
	{ "close_udp_socket", close_udp_socket },
	{ NULL, NULL }
};

void lib_net_init(lua_State* state)
{
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_net");
}
