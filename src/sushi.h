
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

#ifndef SUSHI_H
#define SUSHI_H

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "luajit.h"

typedef struct
{
	unsigned char* data;
	unsigned long dataSize;
	char* fileName;
}
SushiCode;

const char* sushi_error_to_string(lua_State* state);
void sushi_error(const char* fmt, ...);
int sushi_get_error_count();
int sushi_has_errors();
int sushi_print_stacktrace(lua_State* state);
lua_State* sushi_create_new_state();
const char* sushi_get_executable_path();
char* sushi_get_real_path(const char* path);
SushiCode* sushi_code_read_from_stdin();
SushiCode* sushi_code_read_from_file(const char* path);
SushiCode* sushi_code_read_from_executable(const char* path);
SushiCode* sushi_code_free(SushiCode* code);
int sushi_execute_program(lua_State* state, SushiCode* code);
void* sushi_profiler_start(lua_State* state, const char* outputFile);
void* sushi_profiler_stop(lua_State* state, void* profiler);

#endif
