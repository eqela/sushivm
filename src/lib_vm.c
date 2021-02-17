
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
#include <stdlib.h>
#include "lj_tab.h"
#include "lj_lib.h"
#include "lj_err.h"
#include "lmarshal.h"
#include "lib_vm.h"
#define luaL_getn(L,i) ((int)lua_objlen(L,i))
#define luaL_setn(L,i,j) ((void)0)
#define aux_getn(L,n) (luaL_checktype(L,n,5), luaL_getn(L,n))

static int get_sushi_version(lua_State* state)
{
	lua_pushstring(state, SUSHI_VERSION);
	return 1;
}

static int get_sushi_executable_path(lua_State* state)
{
	lua_pushstring(state, sushi_get_executable_path());
	return 1;
}

static int get_program_path(lua_State* state)
{
	lua_getglobal(state, "_program");
	return 1;
}

static int get_program_arguments(lua_State* state)
{
	lua_getglobal(state, "_args");
	return 1;
}

static int run_garbage_collector(lua_State* state)
{
	lua_gc(state, LUA_GCCOLLECT, 0);
	return 0;
}

static int create_destructor(lua_State* state)
{
	void* ud = lua_newuserdata(state, 1);
	lua_newtable(state);
	lua_pushliteral(state, "__gc");
	lua_pushvalue(state, 2);
	lua_rawset(state, -3);
	lua_setmetatable(state, -2);
	return 1;
}

static int unpack_array(lua_State* state)
{
	int i, e, n;
	lua_remove(state, 1);
	luaL_checktype(state, 1, 5);
	i = luaL_optint(state, 2, 1);
	e = luaL_opt(state, luaL_checkint, 3, luaL_getn(state, 1));
	if(i > e) {
		return 0;
	}
	n = e - i + 1;
	if(n <= 0 || !lua_checkstack(state, n)) {
		return luaL_error(state, "too many results to unpack");
	}
	lua_rawgeti(state, 1, i);
	while( i++ < e) {
		lua_rawgeti(state, 1, i);
	}
	return n;
}

static int get_global(lua_State* state)
{
	const char* name = lua_tostring(state, 2);
	if(name == NULL) {
		lua_pushnil(state);
		return 1;
	}
	sushi_getglobal(state, name);
	return 1;
}

static int set_metatable(lua_State* state)
{
	lua_setmetatable(state, 2);
	return 1;
}

static int execute_protected_call(lua_State* state)
{
	lua_remove(state, 1);
	int r = lua_pcall(state, 0, 0, 0);
	if(r == 0) {
		lua_pushboolean(state, 1);
		return 1;
	}
	int top = lua_gettop(state);
	lua_pushboolean(state, 0);
	lua_insert(state, top);
	return 2;
}

static int get_datatype_info(lua_State* state)
{
	int tt = lua_type(state, 2);
	if(tt == LUA_TNIL) {
		lua_pushstring(state, "nil");
		return 1;
	}
	if(tt == LUA_TSTRING) {
		lua_pushstring(state, "string");
		return 1;
	}
	if(tt == LUA_TNUMBER) {
		lua_Number nn = lua_tonumber(state, 2);
		if((double)nn == (int)nn) {
			lua_pushstring(state, "integer");
		}
		else {
			lua_pushstring(state, "double");
		}
		return 1;
	}
	if(tt == LUA_TBOOLEAN) {
		lua_pushstring(state, "boolean");
		return 1;
	}
	if(tt == LUA_TTABLE) {
		lua_pushstring(state, "_qualifiedClassName");
		lua_rawget(state, 2);
		const char* className = lua_tostring(state, -1);
		if(className != NULL) {
			char typeName[1024];
			memset(typeName, 0, 1024);
			snprintf(typeName, 1024, "class:%s", className);
			lua_pushstring(state, typeName);
		}
		else {
			lua_pushstring(state, "object");
		}
		return 1;
	}
	if(tt == LUA_TFUNCTION) {
		lua_pushstring(state, "function");
		return 1;
	}
	if(tt == LUA_TUSERDATA) {
		void* ptr = luaL_testudata(state, 2, "_sushi_buffer");
		if(ptr != NULL) {
			lua_pushstring(state, "buffer");
		}
		else {
			lua_pushstring(state, "userdata");
		}
		return 1;
	}
	if(tt == LUA_TTHREAD) {
		lua_pushstring(state, "thread");
		return 1;
	}
	if(tt == LUA_TLIGHTUSERDATA) {
		lua_pushstring(state, "lightuserdata");
		return 1;
	}
	lua_pushstring(state, "unknown");
	return 1;
}

static int is_instance_of(lua_State* state)
{
	lua_remove(state, 1);
	int tt = lua_type(state, 1);
	if(tt != LUA_TTABLE) {
		lua_pushboolean(state, 0);
		return 1;
	}
	const char* typeName = lua_tostring(state, 2);
	if(typeName == NULL || strncmp(typeName, "class:", 6) != 0) {
		lua_pushboolean(state, 0);
		return 1;
	}
	const char* className = typeName+6;
	char typeKey[1024];
	memset(typeKey, 0, 1024);
	snprintf(typeKey, 1024, "_isType.%s", className);
	lua_pushstring(state, typeKey);
	lua_rawget(state, 1);
	if(lua_toboolean(state, -1) != 1) {
		lua_pushboolean(state, 0);
		return 1;
	}
	lua_pushboolean(state, 1);
	return 1;
}

static int throw_error(lua_State* state)
{
	lua_error(state);
	return 0;
}

static int to_table_with_key(lua_State* state)
{
	int tt = lua_type(state, 2);
	if(tt != LUA_TTABLE) {
		lua_pushnil(state);
		return 1;
	}
	lua_rawget(state, 2);
	if(lua_toboolean(state, -1) == 0) {
		lua_pushnil(state);
		return 1;
	}
	lua_pushvalue(state, 2);
	return 1;
}

static int get_variable_type(lua_State* state)
{
	const char* v = NULL;
	int tt = lua_type(state, 2);
	if(tt == LUA_TNIL) {
		v = "nil";
	}
	else if(tt == LUA_TNUMBER) {
		v = "number";
	}
	else if(tt == LUA_TBOOLEAN) {
		v = "boolean";
	}
	else if(tt == LUA_TSTRING) {
		v = "string";
	}
	else if(tt == LUA_TTABLE) {
		v = "table";
	}
	else if(tt == LUA_TFUNCTION) {
		v = "function";
	}
	else if(tt == LUA_TUSERDATA) {
		v = "userdata";
	}
	else if(tt == LUA_TTHREAD) {
		v = "thread";
	}
	else if(tt == LUA_TLIGHTUSERDATA) {
		v = "lightuserdata";
	}
	if(v == NULL) {
		v = "unknown";
	}
	lua_pushstring(state, v);
	return 1;
}

static int get_table_keys(lua_State* state)
{
	int n = 1;
	lua_remove(state, 1);
	lua_newtable(state);
	int table = lua_gettop(state);
	lua_pushnil(state);
	while(lua_next(state, 1) != 0) {
		lua_pop(state, 1);
		lua_pushvalue(state, lua_gettop(state));
		lua_rawseti(state, table, n++);
	}
	lua_pushstring(state, "n");
	lua_pushnumber(state, n-1);
	lua_rawset(state, table);
	return 1;
}

static int get_table_values(lua_State* state)
{
	int n = 1;
	lua_remove(state, 1);
	lua_newtable(state);
	int table = lua_gettop(state);
	lua_pushnil(state);
	while(lua_next(state, 1) != 0) {
		lua_rawseti(state, table, n++);
	}
	lua_pushstring(state, "n");
	lua_pushnumber(state, n-1);
	lua_rawset(state, table);
	return 1;
}

static int get_stack_trace(lua_State* state)
{
	luaL_traceback(state, state, NULL, 0);
	return 1;
}

static int allocate_array(lua_State* state)
{
	lua_remove(state, 1);
	int size = luaL_checknumber(state, 1);
	lua_createtable(state, size, 0);
	int n;
	for(n=0; n<size; n++) {
		lua_pushnumber(state, 0);
		lua_rawseti(state, -2, n + 1);
	}
	return 1;
}

static int insert_to_indexed_table(lua_State* state)
{
	lua_remove(state, 1);
	int e = aux_getn(state, 1) + 1;
	int pos = luaL_checkint(state, 2);
	int i;
	for(i=e; i>pos; i--) {
		lua_rawgeti(state, 1, i-1);
		lua_rawseti(state, 1, i);
	}
	luaL_setn(state, 1, e);
	lua_rawseti(state, 1, pos);
	return 0;
}

static int remove_from_indexed_table(lua_State* state)
{
	lua_remove(state, 1);
	int e = aux_getn(state, 1);
	int pos = luaL_optint(state, 2, e);
	if(!(1 <= pos && pos <= e)) {
		lua_pushnil(state);
		return 1;
	}
	luaL_setn(state, 1, e-1);
	lua_rawgeti(state, 1, pos);
	for(; pos < e ; pos++){
		lua_rawgeti(state, 1, pos+1);
		lua_rawseti(state, 1, pos);
	}
	lua_pushnil(state);
	lua_rawseti(state, 1, e);
	return 1;
}

static int clear_table(lua_State* state)
{
	lua_remove(state, 1);
	lj_tab_clear(lj_lib_checktab(state, 1));
	return 0;
}

static void set2(lua_State* state, int i, int j)
{
	lua_rawseti(state, 1, i);
	lua_rawseti(state, 1, j);
}

static int do_sort_table_comp(lua_State* state, int a, int b)
{
	if(!lua_isnil(state, 2)) {
		int res;
		lua_pushvalue(state, 2);
		lua_pushvalue(state, a-1);
		lua_pushvalue(state, b-2);
		lua_call(state, 2, 1);
		int n = lua_tonumber(state, -1);
		lua_pop(state, 1);
		if(n < 0) {
			return 1;
		}
		return 0;
	}
	else {
		return lua_lessthan(state, a, b);
	}
}

static void do_sort_table(lua_State* state, int l, int u)
{
	while (l < u) {
		int i, j;
		lua_rawgeti(state, 1, l);
		lua_rawgeti(state, 1, u);
		if (do_sort_table_comp(state, -1, -2)) {
			set2(state, l, u);
		}
		else {
			lua_pop(state, 2);
		}
		if(u - l == 1) {
			break;
		}
		i = (l+u)/2;
		lua_rawgeti(state, 1, i);
		lua_rawgeti(state, 1, l);
		if(do_sort_table_comp(state, -2, -1)) {
			set2(state, i, l);
		}
		else {
			lua_pop(state, 1);
			lua_rawgeti(state, 1, u);
			if(do_sort_table_comp(state, -1, -2)) {
				set2(state, i, u);
			}
			else {
				lua_pop(state, 2);
			}
		}
		if(u - l == 2) {
			break;
		}
		lua_rawgeti(state, 1, i);
		lua_pushvalue(state, -1);
		lua_rawgeti(state, 1, u-1);
		set2(state, i, u-1);
		i = l;
		j = u-1;
		while(1) {
			while(lua_rawgeti(state, 1, ++i), do_sort_table_comp(state, -1, -2)) {
				if (i>u) {
					lj_err_caller(state, LJ_ERR_TABSORT);
				}
				lua_pop(state, 1);
			}
			while(lua_rawgeti(state, 1, --j), do_sort_table_comp(state, -3, -1)) {
				if(j<l) {
					lj_err_caller(state, LJ_ERR_TABSORT);
				}
				lua_pop(state, 1);
			}
			if (j<i) {
				lua_pop(state, 3);
				break;
			}
			set2(state, i, j);
		}
		lua_rawgeti(state, 1, u-1);
		lua_rawgeti(state, 1, i);
		set2(state, u-1, i);
		if (i-l < u-i) {
			j=l;
			i=i-1;
			l=i+2;
		}
		else {
			j=i+1;
			i=u;
			u=j-2;
		}
		do_sort_table(state, j, i);
	}
}

static int sort_table(lua_State* state)
{
	lua_remove(state, 1);
	GCtab *t = lj_lib_checktab(state, 1);
	int32_t n = (int32_t)lj_tab_len(t);
	lua_settop(state, 2);
	if (!tvisnil(state->base+1)) {
		lj_lib_checkfunc(state, 2);
	}
	do_sort_table(state, 1, n);
	return 0;
}

static int get_subscript_value(lua_State* state)
{
	lua_remove(state, 1);
	void* bptr = lua_touserdata(state, 1);
	if(bptr != NULL) {
		long offset = luaL_checklong(state, 2);
		lua_pushnumber(state, (int)((unsigned char*)bptr)[sizeof(long) + offset]);
		return 1;
	}
	if(lua_istable(state, 1)) {
		if(lua_isnumber(state, 2)) {
			lua_Number n = lua_tonumber(state, 2);
			lua_rawgeti(state, 1, n + 1);
			return 1;
		}
		else {
			lua_rawget(state, 1);
			return 1;
		}
	}
	lua_pushnil(state);
	return 1;
}

static int set_subscript_value(lua_State* state)
{
	lua_remove(state, 1);
	void* bptr = lua_touserdata(state, 1);
	if(bptr != NULL) {
		long offset = luaL_checklong(state, 2);
		int value = luaL_checkint(state, 3);
		((unsigned char*)bptr)[sizeof(long) + offset] = value & 0xff;
		return 1;
	}
	else if(lua_isnumber(state, 2)) {
		lua_Number n = lua_tonumber(state, 2);
		lua_pushvalue(state, 3);
		lua_rawseti(state, 1, n + 1);
		return 1;
	}
	else {
		lua_pushvalue(state, 2);
		lua_pushvalue(state, 3);
		lua_rawset(state, 1);
		return 1;
	}
}

static int bitwise_and(lua_State* state)
{
	long lvalue = luaL_checknumber(state, 2);
	long rvalue = luaL_checknumber(state, 3);
	lua_pushnumber(state, lvalue & rvalue);
	return 1;
}

static int bitwise_or(lua_State* state)
{
	long lvalue = luaL_checknumber(state, 2);
	long rvalue = luaL_checknumber(state, 3);
	lua_pushnumber(state, lvalue | rvalue);
	return 1;
}

static int bitwise_not(lua_State* state)
{
	long value = luaL_checknumber(state, 2);
	lua_pushnumber(state, ~value);
	return 1;
}

static int bitwise_xor(lua_State* state)
{
	long lvalue = luaL_checknumber(state, 2);
	long rvalue = luaL_checknumber(state, 3);
	lua_pushnumber(state, lvalue ^ rvalue);
	return 1;
}

static int bitwise_left_shift(lua_State* state)
{
	long lvalue = luaL_checknumber(state, 2);
	long rvalue = luaL_checknumber(state, 3);
	lua_pushnumber(state, lvalue << rvalue);
	return 1;
}

static int bitwise_right_shift(lua_State* state)
{
	long lvalue = luaL_checknumber(state, 2);
	long rvalue = luaL_checknumber(state, 3);
	lua_pushnumber(state, lvalue >> rvalue);
	return 1;
}

static int execute_program(lua_State* ostate)
{
	void* ptr = luaL_checkudata(ostate, 2, "_sushi_buffer");
	if(ptr == NULL) {
		lua_pushnumber(ostate, -1);
		return 1;
	}
	long sz = 0;
	memcpy(&sz, ptr, sizeof(long));	
	ptr += sizeof(long);
    const char *name = luaL_checkstring(ostate, 3);
	if(name == NULL) {
		name = "__code__";
	}
	lua_State* nstate = sushi_create_new_state();
	if(nstate == NULL) {
		lua_pushnumber(ostate, -1);
		return 1;
	}
	if(lua_istable(ostate, 4)) {
		int i = 1;
		lua_createtable(nstate, 0, 0);
		lua_pushnil(ostate);
		while(lua_next(ostate, 4) != 0) {
			if(lua_isnumber(ostate, -2) && lua_isstring(ostate, -1)) {
				lua_pushstring(nstate, lua_tostring(ostate, -1));
				lua_rawseti(nstate, -2, i);
				i ++;
			}
			lua_pop(ostate, 1);
		}
		lua_setglobal(nstate, "_args");
	}
	SushiCode code;
	code.data = (unsigned char*)ptr;
	code.dataSize = sz;
	code.fileName = (char*)name;
	int rv = sushi_execute_program(nstate, &code);
	lua_gc(nstate, LUA_GCCOLLECT, 0);
	lua_close(nstate);
	lua_pushnumber(ostate, rv);
	return 1;
}

static int parse_to_function(lua_State* state)
{
	size_t len = 0;
    const char *buf = luaL_checklstring(state, 2, &len);
	if(buf == NULL) {
		lua_pushnil(state);
		return 1;
	}
	if(luaL_loadbuffer(state, buf, len, "__code__") != 0) {
		fprintf(stderr, "Error while parsing Lua code: `%s'\n", lua_tostring(state, -1));
		lua_pushnil(state);
		return 1;
	}
	return 1;
}

static int serialize_object(lua_State* state)
{
	char* buf;
	unsigned long len;
	lua_remove(state, 1);
	mar_encode(state, &buf, &len);
	lua_pushlstring(state, buf, len);
	free(buf);
	return 1;
}

static int unserialize_object(lua_State* state)
{
    size_t len;
	lua_remove(state, 1);
    const char *buf = luaL_checklstring(state, 1, &len);
	return mar_decode(state, buf, len);
}

int prepare_interpreter(lua_State* state)
{
	// parameters
	void* codeptr = luaL_checkudata(state, 2, "_sushi_buffer");
	if(codeptr == NULL) {
		lua_pushnil(state);
		return 1;
	}
	long codesize = 0;
	memcpy(&codesize, codeptr, sizeof(long));
	codeptr += sizeof(long);
	// create new lua state
	lua_State* nstate = sushi_create_new_state();
	if(nstate == NULL) {
		lua_pushnil(state);
		return 1;
	}
	// load code
	SushiCode* code = sushi_code_for_buffer((unsigned char*)codeptr, (unsigned long)codesize, "__code__");
	if(code == NULL) {
		lua_close(nstate);
		lua_pushnil(state);
		return 1;
	}
	int lcr = sushi_load_code(nstate, code);
	code->data = NULL;
	code->fileName = NULL;
	code = sushi_code_free(code);
	if(lcr != 0) {
		lua_close(nstate);
		lua_pushnil(state);
		return 1;
	}
	if(sushi_pcall(nstate, 0, 0) != LUA_OK) {
		const char* errstr = sushi_error_to_string(nstate);
		if(errstr != NULL) {
			sushi_error("Code execution failed: `%s'", errstr);
		}
		else {
			sushi_error("Code execution failed.");
		}
		lua_close(nstate);
		lua_pushnil(state);
		return 1;
	}
	void* ptr = lua_newuserdata(state, sizeof(lua_State*));
	luaL_getmetatable(state, "_sushi_interpreter");
	lua_setmetatable(state, -2);
	memcpy(ptr, &nstate, sizeof(lua_State*));
	return 1;
}

int close_interpreter_gc(lua_State* state)
{
	void* ptr = luaL_checkudata(state, 1, "_sushi_interpreter");
	if(ptr == NULL) {
		return 0;
	}
	lua_State* nstate = NULL;
	memcpy(&nstate, ptr, sizeof(lua_State*));
	if(nstate != NULL) {
		lua_close(nstate);
		nstate = NULL;
		memcpy(ptr, &nstate, sizeof(lua_State*));
	}
	return 0;
}

int close_interpreter(lua_State* state)
{
	lua_remove(state, 1);
	return close_interpreter_gc(state);
}

static void init_interpreter_type(lua_State* state)
{
	luaL_newmetatable(state, "_sushi_interpreter");
	lua_pushliteral(state, "__gc");
	lua_pushcfunction(state, close_interpreter_gc);
	lua_rawset(state, -3);
	lua_pop(state, 1);
}

static const luaL_Reg funcs[] = {
	{ "get_sushi_version", get_sushi_version },
	{ "get_sushi_executable_path", get_sushi_executable_path },
	{ "get_program_path", get_program_path },
	{ "get_program_arguments", get_program_arguments },
	{ "run_garbage_collector", run_garbage_collector },
	{ "create_destructor", create_destructor },
	{ "unpack_array", unpack_array },
	{ "get_global", get_global },
	{ "set_metatable", set_metatable },
	{ "execute_protected_call", execute_protected_call },
	{ "get_datatype_info", get_datatype_info },
	{ "is_instance_of", is_instance_of },
	{ "throw_error", throw_error },
	{ "to_table_with_key", to_table_with_key },
	{ "get_variable_type", get_variable_type },
	{ "get_table_keys", get_table_keys },
	{ "get_table_values", get_table_values },
	{ "get_stack_trace", get_stack_trace },
	{ "allocate_array", allocate_array },
	{ "insert_to_indexed_table", insert_to_indexed_table },
	{ "remove_from_indexed_table", remove_from_indexed_table },
	{ "clear_table", clear_table },
	{ "sort_table", sort_table },
	{ "get_subscript_value", get_subscript_value },
	{ "set_subscript_value", set_subscript_value },
	{ "bitwise_and", bitwise_and },
	{ "bitwise_or", bitwise_or },
	{ "bitwise_not", bitwise_not },
	{ "bitwise_xor", bitwise_xor },
	{ "bitwise_left_shift", bitwise_left_shift },
	{ "bitwise_right_shift", bitwise_right_shift },
	{ "execute_program", execute_program },
	{ "parse_to_function", parse_to_function },
	{ "serialize_object", serialize_object },
	{ "unserialize_object", unserialize_object },
	{ "prepare_interpreter", prepare_interpreter },
	{ "close_interpreter", close_interpreter },
	{ NULL, NULL }
};

void lib_vm_init(lua_State* state)
{
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_vm");
	init_interpreter_type(state);
}
