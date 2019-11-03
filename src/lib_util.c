
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

#ifdef SUSHI_SUPPORT_WIN32
#define _CRT_RAND_S
#include <winsock.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "lib_util.h"
#include "zbuf.h"
#include "zip.h"
#include "unzip.h"
#ifdef SUSHI_SUPPORT_LINUX
#include <arpa/inet.h>
#endif

// FIXME: Fix all string operations to properly process as UTF8

static int set_buffer_byte_lua_syntax(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 1, "_sushi_buffer");
	if(ptr == NULL) {
		return 0;
	}
	long offset = luaL_checklong(state, 2) - 1;
	long sz;
	memcpy(&sz, ptr, sizeof(long));
	if(offset < 0 || offset >= sz) {
		return 0;
	}
	int value = luaL_checkint(state, 3);
	((unsigned char*)ptr)[sizeof(long) + offset] = value & 0xff;
	return 0;
}

static int set_buffer_byte(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		return 0;
	}
	long offset = luaL_checklong(state, 3);
	long sz;
	memcpy(&sz, ptr, sizeof(long));
	if(offset < 0 || offset >= sz) {
		return 0;
	}
	int value = luaL_checkint(state, 3);
	((unsigned char*)ptr)[sizeof(long) + offset] = value & 0xff;
	return 0;
}

static int get_buffer_byte_lua_syntax(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 1, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	long offset = luaL_checklong(state, 2) - 1;
	long sz;
	memcpy(&sz, ptr, sizeof(long));
	if(offset < 0 || offset >= sz) {
		lua_pushnumber(state, 0);
		return 1;
	}
	lua_pushnumber(state, (int)((unsigned char*)ptr)[sizeof(long) + offset]);
	return 1;
}

static int get_buffer_byte(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	long offset = luaL_checklong(state, 3);
	long sz;
	memcpy(&sz, ptr, sizeof(long));
	if(offset < 0 || offset >= sz) {
		lua_pushnumber(state, 0);
		return 1;
	}
	lua_pushnumber(state, (int)((unsigned char*)ptr)[sizeof(long) + offset]);
	return 1;
}

static int get_buffer_size(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	long size = 0;
	memcpy(&size, ptr, sizeof(long));
	lua_pushnumber(state, size);
	return 1;
}

static int get_buffer_size_lua_syntax(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 1, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(state, 0);
		return 1;
	}
	long size = 0;
	memcpy(&size, ptr, sizeof(long));
	lua_pushnumber(state, size);
	return 1;
}

static int copy_buffer_bytes(lua_State* state)
{
	void* src = luaL_checkudata(state, 2, "_sushi_buffer");
	if(src == NULL) {
		return 0;
	}
	void* dst = luaL_checkudata(state, 3, "_sushi_buffer");
	if(dst == NULL) {
		return 0;
	}
	long soffset = luaL_checklong(state, 4);
	if(soffset < 0) {
		soffset = 0;
	}
	long doffset = luaL_checklong(state, 5);
	if(doffset < 0) {
		doffset = 0;
	}
	long size = luaL_checklong(state, 6);
	if(size < 1) {
		return 0;
	}
	long srcsz;
	long dstsz;
	memcpy(&srcsz, src, sizeof(long));
	memcpy(&dstsz, dst, sizeof(long));
	if(soffset + size > srcsz) {
		size = srcsz - soffset;
	}
	if(doffset + size > dstsz) {
		size = dstsz - doffset;
	}
	size_t sz = sizeof(long);
	memmove(dst + sz + doffset, src + sz + soffset, size);
	return 0;
}

static int allocate_buffer(lua_State* state)
{
	long size = luaL_checklong(state, 2);
	if(size < 1) {
		lua_pushnil(state);
		return 1;
	}
	void* ptr = lua_newuserdata(state, sizeof(long) + (size_t)size);
	luaL_getmetatable(state, "_sushi_buffer");
	lua_setmetatable(state, -2);
	memcpy(ptr, &size, sizeof(long));
	return 1;
}

static int is_buffer(lua_State* state)
{
	void* ptr = luaL_testudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushboolean(state, 0);
	}
	else {
		lua_pushboolean(state, 1);
	}
	return 1;
}

static int create_buffer(lua_State* state)
{
	lua_remove(state, 1);
	long size = lua_objlen(state, 1);
	if(size < 1) {
		lua_pushnil(state);
		return 1;
	}
	void* ptr = lua_newuserdata(state, sizeof(long) + (size_t)size);
	luaL_getmetatable(state, "_sushi_buffer");
	lua_setmetatable(state, -2);
	memcpy(ptr, &size, sizeof(long));
	long n = 0;
	lua_pushnil(state);
	while(lua_next(state, 1) != 0) {
		int vv = luaL_checknumber(state, -1);
		((unsigned char*)ptr)[sizeof(long) + n] = vv & 0xff;
		n ++;
		lua_pop(state, 1);
	}
	return 1;
}

static int create_random_number_generator(lua_State* state)
{
	int seed = luaL_checkint(state, 2);
	size_t sz = sizeof(int);
	void* ptr = lua_newuserdata(state, sz);
	memcpy(ptr, &seed, sz);
	return 1;
}

static int create_random_number(lua_State* state)
{
	void* ptr = lua_touserdata(state, 2);
	int seed = 0;
	memcpy(&seed, ptr, sizeof(int));
	unsigned int result = 0;
#if defined(SUSHI_SUPPORT_WIN32)
	rand_s(&result);
#else
	result = (unsigned int)rand_r(&seed);
#endif
	lua_pushnumber(state, result);
	memcpy(ptr, &seed, sizeof(int));
	return 1;
}

static int number_to_buffer32le(lua_State* state)
{
	uint32_t number = htonl((uint32_t)luaL_checknumber(state, 2));
	size_t longsz = sizeof(long);
	void* ptr = lua_newuserdata(state, longsz + 4);
	luaL_getmetatable(state, "_sushi_buffer");
	lua_setmetatable(state, -2);
	long sz = (long)4;
	memcpy(ptr, &sz, longsz);
	char* sptr = (char*)&number;
	ptr += longsz;
	((char*)ptr)[0] = sptr[3];
	((char*)ptr)[1] = sptr[2];
	((char*)ptr)[2] = sptr[1];
	((char*)ptr)[3] = sptr[0];
	return 1;
}

static int number_to_buffer32be(lua_State* state)
{
	uint32_t number = htonl((uint32_t)luaL_checknumber(state, 2));
	size_t longsz = sizeof(long);
	void* ptr = lua_newuserdata(state, longsz + 4);
	luaL_getmetatable(state, "_sushi_buffer");
	lua_setmetatable(state, -2);
	long sz = (long)4;
	memcpy(ptr, &sz, longsz);
	char* sptr = (char*)&number;
	ptr += longsz;
	memcpy(ptr, sptr, 4);
	return 1;
}

static int network_bytes_to_host16(lua_State* state)
{
	uint8_t bytes[2];
	lua_remove(state, 1);
	bytes[0] = ((int)luaL_checknumber(state, 1)) & 0xff;
	bytes[1] = ((int)luaL_checknumber(state, 2)) & 0xff;
	uint16_t* vp = (uint16_t*)bytes;
	lua_pushnumber(state, ntohs(*vp) & 0xffff);
	return 1;
}

static int network_bytes_to_host32(lua_State* state)
{
	uint8_t bytes[4];
	lua_remove(state, 1);
	bytes[0] = ((int)luaL_checknumber(state, 1)) & 0xff;
	bytes[1] = ((int)luaL_checknumber(state, 2)) & 0xff;
	bytes[2] = ((int)luaL_checknumber(state, 3)) & 0xff;
	bytes[3] = ((int)luaL_checknumber(state, 4)) & 0xff;
	uint32_t* vp = (uint32_t*)bytes;
	lua_pushnumber(state, ntohl(*vp) & 0xffffffff);
	return 1;
}

static int convert_to_integer(lua_State* state)
{
	int n = luaL_checknumber(state, 2);
	lua_pushnumber(state, n);
	return 1;
}

static int to_number(lua_State* state)
{
	lua_pushnumber(state, lua_tonumber(state, 2));
	return 1;
}

static int get_string_length(lua_State* state)
{
	size_t len = 0;
	const char* str = lua_tolstring(state, 2, &len);
	if(str == NULL) {
		lua_pushnumber(state, 0);
	}
	else {
		lua_pushnumber(state, len);
	}
	return 1;
}

static int get_utf8_character_count(lua_State* state)
{
	size_t len = 0;
	const char* str = lua_tolstring(state, 2, &len);
	if(str == NULL) {
		lua_pushnumber(state, 0);
	}
	else {
		long v = 0;
		long n = 0;
		while(n < len) {
			unsigned char c = (unsigned char)str[n];
			if(c <= 0x7F) {
				n ++;
				v ++;
			}
			else if(c >= 0xF0) {
				v ++;
				n += 4;
			}
			else if(c >= 0xE0) {
				v ++;
				n += 3;
			}
			else if(c >= 0xC0) {
				v ++;
				n += 2;
			}
			else {
				n ++;
			}
		}
		lua_pushnumber(state, v);
	}
	return 1;
}

static int create_string_for_byte(lua_State* state)
{
	int cc = luaL_checknumber(state, 2);
	char v[8];
	memset(v, 0, 8);
	v[0] = (char)cc & 0xff;
	lua_pushstring(state, v);
	return 1;
}

static int create_octal_string_for_integer(lua_State* state)
{
	long cc = luaL_checknumber(state, 2);
	char v[512];
	memset(v, 0, 512);
	snprintf(v, 511, "%lo", cc);
	lua_pushstring(state, v);
	return 1;
}

static int create_hex_string_for_integer(lua_State* state)
{
	long cc = luaL_checknumber(state, 2);
	char v[512];
	memset(v, 0, 512);
	snprintf(v, 511, "%lx", cc);
	lua_pushstring(state, v);
	return 1;
}

static int create_decimal_string_for_integer(lua_State* state)
{
	long cc = luaL_checknumber(state, 2);
	char v[512];
	memset(v, 0, 512);
	snprintf(v, 511, "%ld", cc);
	lua_pushstring(state, v);
	return 1;
}

static int create_string_for_float(lua_State* state)
{
	double cc = luaL_checknumber(state, 2);
	char v[512];
	memset(v, 0, 512);
	snprintf(v, 511, "%.14f", cc);
	int p = strlen(v) - 1;
	while(p >= 0 && v[p] == '0') {
		if(p > 0 && v[p-1] == '.') {
			break;
		}
		v[p] = 0;
		p--;
	}
	lua_pushstring(state, v);
	return 1;
}

static int get_byte_from_string(lua_State* state)
{
	size_t len;
	const char* str = lua_tolstring(state, 2, &len);
	long index = lua_tonumber(state, 3);
	if(index < 0 || index >= len) {
		lua_pushnumber(state, 0);
		return 1;
	}
	lua_pushnumber(state, str[index] & 0xff);
	return 1;
}

static int string_starts_with(lua_State* state)
{
	size_t s1len;
	size_t s2len;
	lua_remove(state, 1);
	const char* s1 = lua_tolstring(state, 1, &s1len);
	const char* s2 = lua_tolstring(state, 2, &s2len);
	long offset = lua_tonumber(state, 3);
	if(s1 == NULL || s2 == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
	if(offset >= s1len) {
		lua_pushboolean(state, 0);
		return 1;
	}
	if(strncmp(s1 + offset, s2, strlen(s2)) != 0) {
		lua_pushboolean(state, 0);
		return 1;
	}
	lua_pushboolean(state, 1);
	return 1;
}

static int compare_string_ignore_case(lua_State* state)
{
	const char* s1 = luaL_checkstring(state, 2);
	const char* s2 = luaL_checkstring(state, 3);
	if(s1 == NULL || s2 == NULL) {
		lua_pushnil(state);
		return 1;
	}
	int v = 0;
	int n = 0;
	while(1) {
		int s1c = tolower(s1[n]);
		int s2c = tolower(s2[n]);
		if(s1c < s2c) {
			v = -1;
			break;
		}
		if(s1c > s2c) {
			v = -1;
			break;
		}
		if(s1c == 0) {
			break;
		}
		n++;
	}
	lua_pushnumber(state, v);
	return 1;
}

static int change_string_to_lowercase(lua_State* state)
{
	size_t len = 0;
	const char* str = lua_tolstring(state, 2, &len);
	if(str != NULL && len > 0) {
		char* v = (char*)malloc(len);
		size_t n;
		for(n=0; n<len; n++) {
			v[n] = tolower(str[n]);
		}
		lua_pushlstring(state, v, len);
		free(v);
	}
	else {
		lua_pushstring(state, "");
	}
	return 1;
}

static int change_string_to_uppercase(lua_State* state)
{
	size_t len = 0;
	const char* str = lua_tolstring(state, 2, &len);
	if(str != NULL && len > 0) {
		char* v = (char*)malloc(len);
		size_t n;
		for(n=0; n<len; n++) {
			v[n] = toupper(str[n]);
		}
		lua_pushlstring(state, v, len);
		free(v);
	}
	else {
		lua_pushstring(state, "");
	}
	return 1;
}

static int get_substring(lua_State* state)
{
	size_t slen;
	const char* str = lua_tolstring(state, 2, &slen);
	if(str == NULL) {
		lua_pushnil(state);
		return 1;
	}
	long start = luaL_checknumber(state, 3);
	if(start < 0) {
		start = 0;
	}
	if(start > slen) {
		lua_pushnil(state);
		return 1;
	}
	long end = luaL_checknumber(state, 4);
	if(end > slen) {
		end = slen;
	}
	long len = end - start;
	if(len > 0) {
		lua_pushlstring(state, str+start, len);
	}
	else {
		lua_pushstring(state, "");
	}
	return 1;
}

static int get_index_of_character(lua_State* state)
{
	size_t len;
	const char* str = lua_tolstring(state, 2, &len);
	if(str == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	long c = luaL_checknumber(state, 3);
	long start = luaL_checknumber(state, 4);
	if(start < 0) {
		start = 0;
	}
	if(start >= len) {
		lua_pushnumber(state, -1);
		return 1;
	}
	char* p = strchr((const char*)(str + start), (int)c);
	if(p == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	lua_pushnumber(state, (int)(p - str));
	return 1;
}

static int get_index_of_substring(lua_State* state)
{
	size_t len;
	const char* str = lua_tolstring(state, 2, &len);
	if(str == NULL) {
		lua_pushnumber(state, -1);
		return 1;
	}
	size_t sslen;
	const char* ss = lua_tolstring(state, 3, &sslen);
	if(ss == NULL || *ss == 0) {
		lua_pushnumber(state, -1);
		return 1;
	}
	long start = luaL_checknumber(state, 4);
	if(start < 0) {
		start = 0;
	}
	if(start >= len) {
		lua_pushnumber(state, -1);
		return 1;
	}
	const char* p = str;
	while(*p != 0) {
		if(!strncmp(p, ss, sslen)) {
			lua_pushnumber(state, (int)(p - str));
			return 1;
		}
		p++;
	}
	lua_pushnumber(state, -1);
	return 1;
}

static int convert_string_to_buffer(lua_State* state)
{
	size_t size;
	const char* str = luaL_checklstring(state, 2, &size);
	if(str == NULL) {
		lua_pushnil(state);
		return 1;
	}
	size_t longsz = sizeof(long);
	void* ptr = lua_newuserdata(state, longsz + size);
	luaL_getmetatable(state, "_sushi_buffer");
	lua_setmetatable(state, -2);
	long sz = (long)size;
	memcpy(ptr, &sz, longsz);
	memcpy(ptr+longsz, str, size);
	return 1;
}

static int convert_buffer_to_string(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnil(state);
		return 1;
	}
	long size = 0;
	memcpy(&size, ptr, sizeof(long));
	if(size > 0) {
		lua_pushlstring(state, (const char*)(ptr+sizeof(long)), size);
	}
	else {
		lua_pushstring(state, "");
	}
	return 1;
}

static int sushi_deflate(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnil(state);
		return 1;
	}
	long size = 0;
	memcpy(&size, ptr, sizeof(long));
	if(size < 1) {
		lua_pushnil(state);
		return 1;
	}
	unsigned char* result = NULL;
	unsigned long resultlen = 0;
	zbuf_deflate(ptr+sizeof(long), size, &result, &resultlen);
	if(result == NULL) {
		lua_pushnil(state);
		return 1;
	}
	if(resultlen < 1) {
		free(result);
		lua_pushnil(state);
		return 1;
	}
	void* rptr = lua_newuserdata(state, sizeof(long) + (size_t)resultlen);
	luaL_getmetatable(state, "_sushi_buffer");
	lua_setmetatable(state, -2);
	memcpy(rptr, &resultlen, sizeof(long));
	memcpy(rptr+sizeof(long), result, resultlen);
	free(result);
	return 1;
}

static int sushi_inflate(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnil(state);
		return 1;
	}
	long size = 0;
	memcpy(&size, ptr, sizeof(long));
	if(size < 1) {
		lua_pushnil(state);
		return 1;
	}
	unsigned char* result = NULL;
	unsigned long resultlen = 0;
	zbuf_inflate(ptr+sizeof(long), size, &result, &resultlen);
	if(result == NULL) {
		lua_pushnil(state);
		return 1;
	}
	if(resultlen < 1) {
		free(result);
		lua_pushnil(state);
		return 1;
	}
	void* rptr = lua_newuserdata(state, sizeof(long) + (size_t)resultlen);
	luaL_getmetatable(state, "_sushi_buffer");
	lua_setmetatable(state, -2);
	memcpy(rptr, &resultlen, sizeof(long));
	memcpy(rptr+sizeof(long), result, resultlen);
	free(result);
	return 1;
}

static int sushi_zip(lua_State* state)
{
	// FIXME: Incomplete
	const char* src = luaL_checkstring(state, 2);
	const char* dst = luaL_checkstring(state, 3);
	if(src == NULL || dst == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
	zipFile zip = zipOpen64(dst, APPEND_STATUS_CREATE);
	if(zip == NULL) {
		lua_pushboolean(state, 0);
		return 1;
	}
		lua_pushnil(state);
		return 1;
}

static int sushi_unzip(lua_State* state)
{
	// FIXME: Not implemented
		lua_pushnil(state);
		return 1;
}

static void initBufferType(lua_State* state)
{
	static const luaL_Reg bufferMethods[] = {
		{ "__newindex", set_buffer_byte_lua_syntax },
		{ "__index", get_buffer_byte_lua_syntax },
		{ "__len", get_buffer_size_lua_syntax },
		{ NULL, NULL }
	};
	luaL_newmetatable(state, "_sushi_buffer");
	lua_pushstring(state, "__index");
	lua_pushvalue(state, -2);
	lua_settable(state, -3);
	luaL_register(state, NULL, bufferMethods);
	lua_pop(state, 1);
}

static const luaL_Reg funcs[] = {
	{ "set_buffer_byte", set_buffer_byte },
	{ "get_buffer_byte", get_buffer_byte },
	{ "get_buffer_size", get_buffer_size },
	{ "copy_buffer_bytes", copy_buffer_bytes },
	{ "allocate_buffer", allocate_buffer },
	{ "is_buffer", is_buffer },
	{ "create_buffer", create_buffer },
	{ "create_random_number_generator", create_random_number_generator },
	{ "create_random_number", create_random_number },
	{ "number_to_buffer32le", number_to_buffer32le },
	{ "number_to_buffer32be", number_to_buffer32be },
	{ "network_bytes_to_host16", network_bytes_to_host16 },
	{ "network_bytes_to_host32", network_bytes_to_host32 },
	{ "convert_to_integer", convert_to_integer },
	{ "to_number", to_number },
	{ "get_string_length", get_string_length },
	{ "get_utf8_character_count", get_utf8_character_count },
	{ "create_string_for_byte", create_string_for_byte },
	{ "create_octal_string_for_integer", create_octal_string_for_integer },
	{ "create_hex_string_for_integer", create_hex_string_for_integer },
	{ "create_decimal_string_for_integer", create_decimal_string_for_integer },
	{ "create_string_for_float", create_string_for_float },
	{ "get_byte_from_string", get_byte_from_string },
	{ "string_starts_with", string_starts_with },
	{ "compare_string_ignore_case", compare_string_ignore_case },
	{ "change_string_to_lowercase", change_string_to_lowercase },
	{ "change_string_to_uppercase", change_string_to_uppercase },
	{ "get_substring", get_substring },
	{ "get_index_of_character", get_index_of_character },
	{ "get_index_of_substring", get_index_of_substring },
	{ "convert_string_to_buffer", convert_string_to_buffer },
	{ "convert_buffer_to_string", convert_buffer_to_string },
	{ "deflate", sushi_deflate },
	{ "inflate", sushi_inflate },
	{ "zip", sushi_zip },
	{ "unzip", sushi_unzip },
	{ NULL, NULL }
};

void lib_util_init(lua_State* state)
{
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_util");
	initBufferType(state);
}
