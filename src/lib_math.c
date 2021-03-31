
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

#include "lib_math.h"
#include <math.h>

static int lua_abs(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, fabs(number));
	return 1;
}

static int lua_fabs(lua_State* state)
{
	float number = (float)luaL_checknumber(state, 2);
	lua_pushnumber(state, fabs(number));
	return 1;
}

static int lua_acos(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, acos(number));
	return 1;
}

static int lua_asin(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, asin(number));
	return 1;
}

static int lua_atan(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, atan(number));
	return 1;
}

static int lua_atan2(lua_State* state)
{
	double y = (double)luaL_checknumber(state, 2);
	double x = (double)luaL_checknumber(state, 3);
	lua_pushnumber(state, atan2(y, x));
	return 1;
}

static int lua_ceil(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, (double)ceil(number));
	return 1;
}

static int lua_cos(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, cos(number));
	return 1;
}

static int lua_cosh(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, cosh(number));
	return 1;
}

static int lua_exp(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, exp(number));
	return 1;
}

static int lua_floor(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, floor(number));
	return 1;
}

static int lua_remainder(lua_State* state)
{
	double x = (double)luaL_checknumber(state, 2);
	double y = (double)luaL_checknumber(state, 3);
	lua_pushnumber(state, fmod(x, y));
	return 1;
}

static int lua_log(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, log(number));
	return 1;
}

static int lua_log10(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, log10(number));
	return 1;
}

static int lua_max_float(lua_State* state)
{
	float x = (float)luaL_checknumber(state, 2);
	float y = (float)luaL_checknumber(state, 3);
	lua_pushnumber(state, fmaxf(x, y));
	return 1;
}

static int lua_min_float(lua_State* state)
{
	float x = (float)luaL_checknumber(state, 2);
	float y = (float)luaL_checknumber(state, 3);
	lua_pushnumber(state, fminf(x, y));
	return 1;
}

static int lua_pow(lua_State* state)
{
	double x = (double)luaL_checknumber(state, 2);
	double y = (double)luaL_checknumber(state, 3);
	lua_pushnumber(state, pow(x, y));
	return 1;
}

static int lua_round(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, (double)round(number));
	return 1;
}

static int lua_round_with_mode(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	int mode = (int)luaL_checknumber(state, 3);
	if(mode == 2) {
		lua_pushnumber(state, (double)roundf(number * 100) / 100);
		return 1;
	}
	lua_pushnumber(state, 0);
	return 1;
}

static int lua_sin(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, sin(number));
	return 1;
}

static int lua_sinh(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, sinh(number));
	return 1;
}

static int lua_sqrt(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, sqrt(number));
	return 1;
}

static int lua_tan(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, tan(number));
	return 1;
}

static int lua_tanh(lua_State* state)
{
	double number = (double)luaL_checknumber(state, 2);
	lua_pushnumber(state, tanh(number));
	return 1;
}

static const luaL_Reg funcs[] = {
	{ "abs", lua_abs },
	{ "fabs", lua_fabs },
	{ "acos", lua_acos },
	{ "asin", lua_asin },
	{ "atan", lua_atan },
	{ "atan2", lua_atan2 },
	{ "ceil", lua_ceil },
	{ "cos", lua_cos },
	{ "cosh", lua_cosh },
	{ "exp", lua_exp },
	{ "floor", lua_floor },
	{ "remainder", lua_remainder },
	{ "log", lua_log },
	{ "log10", lua_log10 },
	{ "max_float", lua_max_float },
	{ "min_float", lua_min_float },
	{ "pow", lua_pow },
	{ "round", lua_round },
	{ "round_with_mode", lua_round_with_mode },
	{ "sin", lua_sin },
	{ "sinh", lua_sinh },
	{ "sqrt", lua_sqrt },
	{ "tan", lua_tan },
	{ "tanh", lua_tanh },
	{ NULL, NULL }
};

void lib_math_init(lua_State* state)
{
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_math");
}
