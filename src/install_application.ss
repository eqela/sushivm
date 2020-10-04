
#
# This file is part of SushiVM
# Copyright (c) 2019-2020 Eqela Oy
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

lib jk-core:0.19.0
lib jk-net:0.18.0
lib jk-data:0.16.0
import jk.fs
import jk.log
import jk.lang
import jk.console
import jk.http.client
import jk.archive

var ctx = new ConsoleApplicationContext()

var type = args[0]
var appName = args[1]
var appRepo = args[2]
var appDir = args[3]
if String.isEmpty(type) || String.isEmpty(appName) || String.isEmpty(appRepo) || String.isEmpty(appDir) {
	Log.error(ctx, "Invalid parameters for install_application.ss")
	return -1
}

if type == "file" {
	var ff = File.forPath(appName)
	var basename = ff.getBasenameWithoutExtension()
	var comps = String.split(basename, '-', 2)
	var appId = comps[0]
	var appVersion = comps[1]
	if String.isEmpty(appId) || String.isEmpty(appVersion) {
		Log.error(ctx, "Unsupported application file name: `" .. ff.getPath() .. "'")
		return -1
	}
	var toolDir = jk.fs.File.asFile(appDir).entry(appId).entry(appVersion)
	Log.debug(ctx, "Installing `" .. String.asString(ff) .. "' as appId=`" .. appId .. "', appVersion=`" .. appVersion .. "' to `" .. String.asString(toolDir) .. "'")
	if toolDir.exists():
		toolDir.removeRecursive()
	var listener = func(file) {
		Log.status(ctx, "[Installing] " .. file.getPath() .. " .. ")
	}
	Log.status(ctx, "[Installing] " .. appName .. " ..")
	var r = ZipReader.extractZipFileToDirectory(ff, toolDir, listener)
	Log.status(ctx, null)
	if not r {
		Log.error(ctx, "Failed to install application file: `" .. appName .. "'")
		return -1
	}
	Log.debug(ctx, "Application installed: `" .. appName .. "': `" .. String.asString(toolDir) .. "'")
	return 0
}

if type == "name" {
	Log.debug(ctx, "Installing application `" .. appName .. "' from `" .. appRepo .. "' to `" .. appDir .. "' ..")
	var comps = String.split(appName, '-', 2)
	var appId = comps[0]
	var appVersion = comps[1]
	if String.isEmpty(appId) || String.isEmpty(appVersion) {
		Log.error(ctx, "Invalid app name: `" .. appName .. "'")
		return -1
	}
	var url = appRepo .. "/" .. appId .. "-" .. appVersion .. ".sapp"
	Log.debug(ctx, "Fetching URL: `" .. url .. "' ..")
	var client = new CustomWebClient()
	var status = null
	var data = null
	Log.status(ctx, "[Downloading] " .. url .. " ..")
	client.query("GET", url, null, null, func(rstatus, rheaders, rdata) {
		status = rstatus
		data = rdata
	})
	Log.status(ctx, null)
	if status {
		Log.debug(ctx, "HTTP status received: `" .. status .. "' ..")
	}
	else {
		Log.debug(ctx, "No HTTP status received.")
	}
	if status != "200" || data == null {
		Log.error(ctx, "Failed to retrieve application installer package from URL: `" .. url .. "'.")
		return -1
	}
	var toolDir = jk.fs.File.asFile(appDir).entry(appId).entry(appVersion)
	if toolDir.exists():
		toolDir.removeRecursive()
	var listener = func(file) {
		Log.status(ctx, "[Installing] " .. file.getPath() .. " .. ")
	}
	Log.status(ctx, "[Installing] " .. appName .. " ..")
	var r = ZipReader.extractZipBufferToDirectory(data, toolDir, listener)
	Log.status(ctx, null)
	if not r {
		Log.error(ctx, "Failed to install downloaded application: `" .. appName .. "'")
		return -1
	}
	Log.debug(ctx, "Application installed: `" .. appName .. "': `" .. String.asString(toolDir) .. "'")
	return 0
}

Log.error(ctx, "Invalid type given to install_application.ss: `" .. type .. "'")
return -1
