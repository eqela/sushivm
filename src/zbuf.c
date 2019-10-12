
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

#ifdef SUSHI_SUPPORT_ZLIB
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "zlib.h"

static void zbuf_append(unsigned char* srcbuf, unsigned long srclen, unsigned char** dstbuf, unsigned long* dstlen)
{
	unsigned origlen = *dstlen;
	unsigned long nlen = *dstlen + srclen;
	*dstbuf = realloc(*dstbuf, nlen);
	memcpy(*dstbuf + origlen, srcbuf, srclen);
	*dstlen = nlen;
}

static void zbuf_free(unsigned char** dstbuf, unsigned long *dstlen)
{
	if(*dstbuf != NULL) {
		free(*dstbuf);
		*dstbuf = NULL;
	}
	*dstlen = 0L;
}

int zbuf_deflate(unsigned char* srcbuf, unsigned long srclen, unsigned char** dstbuf, unsigned long* dstlen)
{
	if(srcbuf == NULL || srclen < 1 || dstbuf == NULL || dstlen == NULL) {
		return 0;
	}
	*dstbuf = NULL;
	*dstlen = 0;
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	if(deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
		return 0;
	}
	strm.avail_in = srclen;
	strm.next_in = srcbuf;
	while(1) {
		int outbufsize = 16384;
		unsigned char outbuf[outbufsize];
		strm.avail_out = outbufsize;
		strm.next_out = outbuf;
		if(deflate(&strm, Z_FINISH) == Z_STREAM_ERROR) {
			deflateEnd(&strm);
			zbuf_free(dstbuf, dstlen);
			return 0;
		}
		zbuf_append(outbuf, outbufsize - strm.avail_out, dstbuf, dstlen);
		if(strm.avail_out > 0) {
			break;
		}
	}
	deflateEnd(&strm);
	return 1;
}

int zbuf_inflate(unsigned char* srcbuf, unsigned long srclen, unsigned char** dstbuf, unsigned long* dstlen)
{
	if(srcbuf == NULL || srclen < 1 || dstbuf == NULL || dstlen == NULL) {
		return 0;
	}
	*dstbuf = NULL;
	*dstlen = 0;
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	if(inflateInit(&strm) != Z_OK) {
		return 0;
	}
	strm.avail_in = srclen;
	strm.next_in = srcbuf;
	while(1) {
		int outbufsize = 16384;
		unsigned char outbuf[outbufsize];
		strm.avail_out = outbufsize;
		strm.next_out = outbuf;
		int ret = inflate(&strm, Z_NO_FLUSH);
		if(ret < 0) {
			inflateEnd(&strm);
			zbuf_free(dstbuf, dstlen);
			return 0;
		}
		zbuf_append(outbuf, outbufsize - strm.avail_out, dstbuf, dstlen);
		if(strm.avail_out > 0) {
			break;
		}
	}
	inflateEnd(&strm);
	return 1;
}

#else

int zbuf_deflate(unsigned char* srcbuf, unsigned long srclen, unsigned char** dstbuf, unsigned long* dstlen)
{
	return 0;
}

int zbuf_inflate(unsigned char* srcbuf, unsigned long srclen, unsigned char** dstbuf, unsigned long* dstlen)
{
	return 0;
}

#endif
