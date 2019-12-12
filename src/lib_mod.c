
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
#ifndef SUSHI_SUPPORT_WIN32
#include <dlfcn.h>
#endif
#include "lib_mod.h"

int mod_open(lua_State* state)
{
	const char* file = lua_tostring(state, 2);
	if(file == NULL || *file == 0) {
		lua_pushnil(state);
		return 1;
	}
	void* handle = NULL;
#ifdef SUSHI_SUPPORT_WIN32
	// Not implemented.
#else
	handle = dlopen(file, RTLD_LAZY);
#endif
	if(handle == NULL) {
		lua_pushnil(state);
		return 1;
	}
	void* ptr = lua_newuserdata(state, sizeof(void*));
	luaL_getmetatable(state, "_sushi_mod");
	lua_setmetatable(state, -2);
	memcpy(ptr, &handle, sizeof(void*));
	return 1;
}

int mod_exec(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 2, "_sushi_mod");
	void* symp = NULL;
#ifdef SUSHI_SUPPORT_WIN32
	// Not implemented.
#else
	const char* sym = lua_tostring(state, 3);
	if(ptr == NULL || sym == NULL) {
		lua_pushnil(state);
		return 1;
	}
	void* handle = NULL;
	memcpy(&handle, ptr, sizeof(void*));
	symp = dlsym(handle, sym);
#endif
	if(symp == NULL) {
		lua_pushnil(state);
		return 1;
	}
	unsigned char* arg = (unsigned char*)luaL_testudata(state, 4, "_sushi_buffer");
	if(arg != NULL) {
		unsigned long arglen = 0;
		memcpy(&arglen, arg, sizeof(long));
		arg += sizeof(long);
		void (*funcp)(unsigned char*,unsigned long,unsigned char**,unsigned long*) = (void(*)(unsigned char*,unsigned long,unsigned char**,unsigned long*))symp;
		unsigned char* rv = NULL;
		unsigned long rvlen = 0;
		funcp(arg, arglen, &rv, &rvlen);
		if(rv != NULL && rvlen > 0) {
			void* ptr = lua_newuserdata(state, sizeof(long) + (size_t)rvlen);
			luaL_getmetatable(state, "_sushi_buffer");
			lua_setmetatable(state, -2);
			memcpy(ptr, &rvlen, sizeof(long));
			memcpy(ptr + sizeof(long), rv, rvlen);
		}
		else {
			lua_pushnil(state);
		}
		if(rv != NULL) {
			free(rv);
		}
		return 1;
	}
	size_t slen;
	const char* sarg = lua_tolstring(state, 4, &slen);
	if(sarg != NULL) {
		char* (*funcp)(const char*) = (char*(*)(const char*))symp;
		char* rv = funcp(sarg);
		if(rv != NULL) {
			lua_pushstring(state, rv);
			free(rv);
		}
		else {
			lua_pushnil(state);
		}
		return 1;
	}
	void (*funcp)() = (void(*)())symp;
	funcp();
	return 0;
}

int mod_close_gc(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 1, "_sushi_mod");
	if(ptr == NULL) {
		return 0;
	}
	void* handle = NULL;
	memcpy(&handle, ptr, sizeof(void*));
	if(handle != NULL) {
#ifdef SUSHI_SUPPORT_WIN32
	// Not implemented.
#else
		dlclose(handle);
#endif
	}
	handle = NULL;
	memcpy(ptr, &handle, sizeof(void*));
	return 0;
}

int mod_close(lua_State* state)
{
	lua_remove(state, 1);
	return mod_close_gc(state);
}

static const luaL_Reg funcs[] = {
	{ "open", mod_open },
	{ "exec", mod_exec },
	{ "close", mod_close },
	{ NULL, NULL }
};

void lib_mod_init(lua_State* state)
{
	luaL_newmetatable(state, "_sushi_mod");
	lua_pushliteral(state, "__gc");
	lua_pushcfunction(state, mod_close_gc);
	lua_rawset(state, -3);
	lua_pop(state, 1);
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_mod");
}
