
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

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "lib_crypto.h"

static int ssl_initialized = 0;
static SSL_CTX* sslctx = NULL;

static void initialize_ssl()
{
	if(ssl_initialized == 0) {
		ssl_initialized = 1;
		SSL_library_init();
		SSL_load_error_strings();
		ERR_load_crypto_strings();
		OpenSSL_add_all_algorithms();
		SSLeay_add_ssl_algorithms();
	}
}

static SSL_CTX* get_ssl_client_context()
{
	if(sslctx == NULL) {
		initialize_ssl();
		sslctx = SSL_CTX_new(TLSv1_2_client_method());
	}
	return sslctx;
}

static SSL* create_ssl_for_socket_fd(int fd, const char* hostname)
{
	SSL_CTX* ctx = get_ssl_client_context();
	if(ctx == NULL) {
		return NULL;
	}
	SSL* ssl = SSL_new(ctx);
	SSL_set_fd(ssl, fd);
	if(hostname != NULL) {
		SSL_set_tlsext_host_name(ssl, hostname);
	}
	if(SSL_connect(ssl) <= 0) {
		ERR_print_errors_fp(stderr);
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return NULL;
	}
	return ssl;
}

static void destroy_ssl(SSL* ssl)
{
	SSL_shutdown(ssl);
	SSL_free(ssl);
}

static int ssl_connect(lua_State* state)
{
	int fd = luaL_checkint(state, 2);
	const char* host = lua_tostring(state, 3);
	SSL* ssl = create_ssl_for_socket_fd(fd, host);
	if(ssl == NULL) {
		return 0;
	}
	void* ptr = lua_newuserdata(state, sizeof(SSL*));
	luaL_getmetatable(state, "_sushi_ssl");
	lua_setmetatable(state, -2);
	memcpy(ptr, &ssl, sizeof(SSL*));
	return 1;
}

static int ssl_read(lua_State* state)
{
	void* sslptr = luaL_checkudata(state, 2, "_sushi_ssl");
	if(sslptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	SSL* ssl = NULL;
	memcpy(&ssl, sslptr, sizeof(SSL*));
	void* ptr = luaL_checkudata(state, 3, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	long sz = 0;
	memcpy(&sz, ptr, sizeof(long));
	if(sz < 1) {
		lua_pushnumber(state, -1);
		return 1;
	}
	int v = SSL_read(ssl, ptr+sizeof(long), sz);
	if(v < 1) {
		lua_pushnumber(state, -1);
		return 1;
	}
	lua_pushnumber(state, v);
	return 1;
}

static int ssl_write(lua_State* state)
{
	void* sslptr = luaL_checkudata(state, 2, "_sushi_ssl");
	if(sslptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	SSL* ssl = NULL;
	memcpy(&ssl, sslptr, sizeof(SSL*));
	void* ptr = luaL_checkudata(state, 3, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	long size = luaL_checknumber(state, 4);
	if(size < 0) {
		memcpy(&size, ptr, sizeof(long));
	}
	if(size == 0) {
		lua_pushnumber(state, 0);
		return 1;
	}
	int r = SSL_write(ssl, ptr + sizeof(long), size);
	if(r == 0) {
		r = -1;
	}
	lua_pushnumber(state, r);
	return 1;
}

static int ssl_close_gc(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 1, "_sushi_ssl");
	if(ptr == NULL) {
		return 0;
	}
	SSL* ssl = NULL;
	memcpy(&ssl, ptr, sizeof(SSL*));
	if(ssl != NULL) {
		destroy_ssl(ssl);
	}
	ssl = NULL;
	memcpy(ptr, &ssl, sizeof(SSL*));
	return 0;
}

static int ssl_close(lua_State* state)
{
	lua_remove(state, 1);
	return ssl_close_gc(state);
}

static const luaL_Reg funcs[] = {
	{ "ssl_connect", ssl_connect },
	{ "ssl_read", ssl_read },
	{ "ssl_write", ssl_write },
	{ "ssl_close", ssl_close },
	{ NULL, NULL }
};

void lib_crypto_init(lua_State* state)
{
	luaL_newmetatable(state, "_sushi_ssl");
	lua_pushliteral(state, "__gc");
	lua_pushcfunction(state, ssl_close_gc);
	lua_rawset(state, -3);
	lua_pop(state, 1);
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_crypto");
}
