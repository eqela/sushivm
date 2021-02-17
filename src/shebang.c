
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char* read_first_line(const char* fileName, char* line, int lsz)
{
	if(fileName == NULL || line == NULL || lsz < 1) {
		return 0;
	}
	FILE* fp = fopen(fileName, "rb");
	if(fp == NULL) {
		return 0;
	}
	if(fgets(line, lsz, fp) == NULL) {
		fclose(fp);
		return 0;
	}
	int sl = strlen(line);
	while(sl > 0 && (line[sl-1] == '\n' || line[sl-1] == '\r')) {
		line[sl-1] = 0;
		sl--;
	}
	fclose(fp);
	return line;
}

static char* next_word(char** pp)
{
	char* p = *pp;
	while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
		p++;
	}
	if(*p == 0) {
		*pp = p;
		return NULL;
	}
	char* v = p;
	while(*p) {
		if(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
			*p = 0;
			p++;
			break;
		}
		p++;
	}
	*pp = p;
	return v;
}

static int is_sushi_shebang_word(const char* word)
{
	if(strcmp(word, "sushi") == 0) {
		return 1;
	}
	char* sp = strstr(word, "/sushi");
	if(sp != NULL) {
		if(strcmp(sp, "/sushi") == 0) {
			return 1;
		}
	}
	if(strcmp(word, "eqela") == 0) {
		return 1;
	}
	char* ep = strstr(word, "/eqela");
	if(ep != NULL) {
		if(strcmp(ep, "/eqela") == 0) {
			return 1;
		}
	}
	return 0;
}

static int count_words(char* str)
{
	int v = 0;
	char* p = str;
	while(1) {
		while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
			p++;
		}
		if(*p == 0) {
			break;
		}
		v++;
		while(*p) {
			if(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
				p++;
				break;
			}
			p++;
		}
	}
	return v;
}

int shebang_process(const char* fileName, int argc, const char** argv, int argcReserved, int* nArgc, char*** nArgv)
{
	if(fileName == NULL) {
		return 0;
	}
	char line[32768];
	if(read_first_line(fileName, line, 32768) == NULL) {
		return 0;
	}
	if(strncmp(line, "#!", 2) != 0) {
		return 0;
	}
	char* p = line + 2;
	char* first = next_word(&p);
	if(is_sushi_shebang_word(first) == 0) {
		return 0;
	}
	int words = count_words(p);
	int nc = words + 1 + argc - argcReserved;
	char** nv = (char**)malloc(sizeof(char*) * (nc + 1));
	int n = 0;
	while(1) {
		char* word = next_word(&p);
		if(word == NULL) {
			break;
		}
		nv[n++] = strdup(word);
	}
	nv[n++] = strdup(fileName);
	for(int x=argcReserved; x<argc; x++) {
		nv[n++] = strdup(argv[x]);
	}
	nv[n++] = NULL;
	*nArgc = nc;
	*nArgv = nv;
	return nc;
}

char** shebang_argv_free(char** argv)
{
	if(argv != NULL) {
		int n = 0;
		while(argv[n]) {
			free(argv[n]);
			n++;
		}
		free(argv);
	}
	return NULL;
}
