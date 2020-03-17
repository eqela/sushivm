
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

#include "lib_crypto.h"

int ssl_connect(lua_State* state);
int ssl_read(lua_State* state);
int ssl_write(lua_State* state);
int ssl_close_gc(lua_State* state);
int ssl_close(lua_State* state);
int rs256_sign(lua_State* state);
int rs256_verify(lua_State* state);

static const luaL_Reg funcs[] = {
	{ "ssl_connect", ssl_connect },
	{ "ssl_read", ssl_read },
	{ "ssl_write", ssl_write },
	{ "ssl_close", ssl_close },
	{ "rs256_sign", rs256_sign },
	{ "rs256_verify", rs256_verify },
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
