#ifndef LMARSHAL_H
#define LMARSHAL_H

int mar_encode(lua_State* L, char** rbuf, unsigned long* rlen);
int mar_decode(lua_State* L, const char* buf, unsigned long len);

#endif
