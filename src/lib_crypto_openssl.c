
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

#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include "sushi.h"

static SSL_CTX* context = NULL;

void lib_crypto_global_init()
{
	context = SSL_CTX_new(TLS_client_method());
}

static SSL_CTX* get_ssl_client_context()
{
	return context;
}

static SSL* create_ssl_for_socket_fd(int fd, const char* hostname)
{
	SSL_CTX* ctx = get_ssl_client_context();
	if(ctx == NULL) {
		return NULL;
	}
	SSL* ssl = SSL_new(ctx);
	if(ssl == NULL) {
		return NULL;
	}
	SSL_set_fd(ssl, fd);
	if(hostname != NULL) {
		SSL_set_tlsext_host_name(ssl, hostname);
	}
	if(SSL_connect(ssl) <= 0) {
		ERR_print_errors_fp(stderr);
		SSL_free(ssl);
		return NULL;
	}
	return ssl;
}

static void destroy_ssl(SSL* ssl)
{
	SSL_shutdown(ssl);
	SSL_free(ssl);
}

int ssl_connect(lua_State* state)
{
	int fd = luaL_checkint(state, 2);
	const char* host = lua_tostring(state, 3);
	SSL* ssl = create_ssl_for_socket_fd(fd, host);
	if(ssl == NULL) {
		lua_pushnil(state);
		return 1;
	}
	void* ptr = lua_newuserdata(state, sizeof(SSL*));
	luaL_getmetatable(state, "_sushi_ssl");
	lua_setmetatable(state, -2);
	memcpy(ptr, &ssl, sizeof(SSL*));
	return 1;
}

int ssl_read(lua_State* state)
{
	SSL** sslptr = (SSL**)luaL_checkudata(state, 2, "_sushi_ssl");
	if(sslptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	SSL* ssl = *sslptr;
	if(ssl == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
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

int ssl_write(lua_State* state)
{
	SSL** sslptr = luaL_checkudata(state, 2, "_sushi_ssl");
	if(sslptr == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	SSL* ssl = *sslptr;
	if(ssl == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
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

int ssl_close_gc(lua_State* state)
{
	SSL** ptr = (SSL**)luaL_checkudata(state, 1, "_sushi_ssl");
	if(ptr == NULL) {
		return 0;
	}
	if(*ptr != NULL) {
		destroy_ssl(*ptr);
		*ptr = NULL;
	}
	return 0;
}

int ssl_close(lua_State* state)
{
	lua_remove(state, 1);
	return ssl_close_gc(state);
}

RSA *create_rsa(unsigned char *key, int forPublic)
{
	RSA *rsa = NULL;
	BIO *bio = BIO_new_mem_buf(key, -1);
	if(bio == NULL) {
		return 0;
	}
	if(forPublic == 1) {
		rsa = PEM_read_bio_RSA_PUBKEY(bio, &rsa, NULL, NULL);
	}
	else {
		rsa = PEM_read_bio_RSAPrivateKey(bio, &rsa, NULL, NULL);
	}
	BIO_free(bio);
	if(rsa == NULL) {
		return 0;
	}
	return rsa;
}

int rs256_sign(lua_State* state)
{
	void* dataptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(dataptr == NULL) {
		lua_pushnil(state);
		lua_pushstring(state, "null data");
		return 2;
	}
	long size = 0;
	memcpy(&size, dataptr, sizeof(long));
	unsigned char *privatekeystr = luaL_checkstring(state, 3);
	if(privatekeystr == NULL) {
		lua_pushnil(state);
		lua_pushstring(state, "null private key");
		return 2;
	}
	unsigned char *datapointer = (unsigned char*)dataptr+sizeof(long);
	unsigned char *databuff[SHA256_DIGEST_LENGTH];
	SHA256_CTX ctx;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, datapointer, size);
	SHA256_Final(databuff, &ctx);
	RSA *privatersa = create_rsa(privatekeystr, 0);
	if(privatersa == NULL) {
		lua_pushnil(state);
		lua_pushstring(state, "failed to create rsa");
		return 2;
	}
	unsigned int signatureLength = 0;
	unsigned char *signature = malloc(RSA_size(privatersa));
	int sign = RSA_sign(NID_sha256, databuff, SHA256_DIGEST_LENGTH, signature, &signatureLength, privatersa);
	if(sign != 1) {
		RSA_free(privatersa);
		free(signature);
		lua_pushnil(state);
		lua_pushstring(state, ERR_reason_error_string(ERR_get_error()));
		return 2;
	}
	size_t longsz = sizeof(long);
	void* ptr = lua_newuserdata(state, (size_t)signatureLength+longsz);
	luaL_getmetatable(state, "_sushi_buffer");
	lua_setmetatable(state, -2);
	long sz = (long)signatureLength;
	memcpy(ptr, &sz, longsz);
	memcpy(ptr+longsz, signature, (size_t)signatureLength);
	RSA_free(privatersa);
	free(signature);
	lua_pushnil(state);
	return 2;
}

int rs256_verify(lua_State* state)
{
	void* dataptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(dataptr == NULL) {
		lua_pushnumber(state, 0);
		lua_pushstring(state, "null data");
		return 2;
	}
	long datasz = 0;
	memcpy(&datasz, dataptr, sizeof(long));
	void* sigptr = luaL_checkudata(state, 3, "_sushi_buffer");
	if(sigptr == NULL) {
		lua_pushnumber(state, 0);
		lua_pushstring(state, "null signature");
		return 2;
	}
	long sigsz = 0;
	memcpy(&sigsz, sigptr, sizeof(long));
	unsigned char *keyptr = luaL_checkstring(state, 4);
	if(keyptr == NULL) {
		lua_pushnumber(state, 0);
		lua_pushstring(state, "null public key");
		return 2;
	}
	unsigned char *datapointer = (unsigned char*)dataptr+sizeof(long);
	unsigned char *databuff[SHA256_DIGEST_LENGTH];
	SHA256_CTX ctx;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, datapointer, datasz);
	SHA256_Final(databuff, &ctx);
	unsigned char *signaturepointer = (unsigned char*)sigptr+sizeof(long);
	RSA *publicrsa = create_rsa(keyptr, 1);
	if(publicrsa == NULL) {
		lua_pushnumber(state, 0);
		lua_pushstring(state, "failed to create RSA");
		return 2;
	}
	int verify = RSA_verify(NID_sha256, databuff, SHA256_DIGEST_LENGTH, signaturepointer, (unsigned int)sigsz, publicrsa);
	if(verify != 1) {
		lua_pushnumber(state, 0);
		lua_pushstring(state, ERR_reason_error_string(ERR_get_error()));
	}
	else {
		lua_pushnumber(state, 1);
		lua_pushnil(state);
	}
	RSA_free(publicrsa);
	return 2;
}
