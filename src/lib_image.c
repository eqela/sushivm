
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

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "lib_image.h"

typedef struct {
	png_bytep buffer;
	png_uint_32 size;
} IMAGE_DATA_HOLDER;

static void encode_png_data_to_memory(png_structp png_ptr, png_bytep data, png_uint_32 length) {
	IMAGE_DATA_HOLDER * p = png_get_io_ptr(png_ptr);
	png_uint_32 nsize = p->size + length;
	if(p->buffer) {
		p->buffer = (png_bytep)realloc(p->buffer, nsize);
	}
	else {
		p->buffer = (png_bytep)malloc(nsize);
	}
	if(!p->buffer) {
		png_error(png_ptr, "WriteError");
	}
	memcpy(p->buffer + p->size, data, length);
	p->size += length;
}

void flush_png(png_structp png_ptr) {
}

static int encode_png_data(lua_State* state)
{
	void* data = luaL_checkudata(state, 2, "_sushi_buffer");
	if(data == NULL) {
		lua_pushnil(state);
		return 1;
	}
	int width = luaL_checkint(state, 3);
	if(width < 1) {
		lua_pushnil(state);
		return 1;
	}
	int height = luaL_checkint(state, 4);
	if(height < 1) {
		lua_pushnil(state);
		return 1;
	}
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr){
		lua_pushnil(state);
		return 1;
	}
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		lua_pushnil(state);
		return 1;
	}
	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	int y;
	int x;
	png_byte **row_pointers = png_malloc(png_ptr, sizeof(png_byte) * height);
	for(y = 0; y < height; y++) {
		long rowcount = sizeof(png_byte) * width * 4;
		png_byte *row = png_malloc(png_ptr, rowcount);
		row_pointers[y] = row;
		for(x = 0; x < rowcount; x++) {
			row[x] = ((png_byte*)data)[(y * rowcount) + x];
		}
	}
	IMAGE_DATA_HOLDER pdh;
	pdh.buffer = NULL;
	pdh.size = 0;
	png_set_write_fn(png_ptr, &pdh, encode_png_data_to_memory, flush_png);
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	int c;
	for(c = 0; c < height; c++) {
		png_free(png_ptr, row_pointers[c]);
	}
	png_free(png_ptr, row_pointers);
	if(pdh.size <= 0) {
		lua_pushnil(state);
		return 1;
	}
	void* ptr = lua_newuserdata(state, pdh.size);
	luaL_getmetatable(state, "_sushi_buffer");
	lua_setmetatable(state, -2);
	memcpy(ptr, &pdh.size, sizeof(long));
	memcpy(ptr + sizeof(long), pdh.buffer, pdh.size);
	return 1;
}

static int decode_png_data(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

static int encode_jpg_data(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

static int decode_jpg_data(lua_State* state)
{
	// FIXME
	lua_pushnil(state);
	return 1;
}

static const luaL_Reg funcs[] = {
	{ "encode_png_data", encode_png_data },
	{ "decode_png_data", decode_png_data },
	{ "encode_jpg_data", encode_jpg_data },
	{ "decode_jpg_data", decode_jpg_data },
	{ NULL, NULL }
};

void lib_image_init(lua_State* state)
{
	luaL_newlib(state, funcs);
	lua_setglobal(state, "_image");
}
