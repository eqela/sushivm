#! eqela ss-0.17
#
# This file is part of SushiVM
# Copyright (c) 2019-2021 J42 Pte Ltd
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
lib jk-core:0.18.0
import jk.lang
import jk.fs
var src = args[0]
var varname = args[1]
var dst = args[2]
var srcfile = jk.fs.File.forPath(src)
var data = srcfile.getContentsBuffer()
if not data:
    Error.throw("failedToReadFile", srcfile)
var size = sizeof data
var sb = new StringBuilder()
sb.appendString("const unsigned char ")
sb.appendString(varname)
sb.appendString("[] = { ")
var n = 0
while n < size {
    if n > 0:
        sb.appendString(", ")
    sb.appendString("0x")
    sb.appendString(String.forIntegerHex(data[n], 0))
    n++
}
sb.appendString(" };\n")
sb.appendString("int ")
sb.appendString(varname)
sb.appendString("_size = ")
sb.appendString(String.forInteger(size))
sb.appendString(";\n")
var dstfile = jk.fs.File.forPath(dst)
if not dstfile.setContentsUTF8(sb.toString()):
    Error.throw("failedToWriteFile", dstfile)
