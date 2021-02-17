_io:write_to_stdout("\n")
_io:write_to_stdout("Sushi version:   " .. _vm:get_sushi_version() .. "\n")
_io:write_to_stdout("Executable path: " .. _vm:get_sushi_executable_path() .. "\n")
_io:write_to_stdout("Program path:    " .. _vm:get_program_path() .. "\n")
_io:write_to_stdout("System type:     " .. _os:get_system_type() .. "\n")
_io:write_to_stdout("\n")

rv = 0
thistest = ""

function error(message)
	_io:write_to_stdout("[ERROR:" .. thistest .. "] " .. message .. "\n")
end

function info(message)
	_io:write_to_stdout("[INFO:" .. thistest .. "] " .. message .. "\n")
end

function execute(name, test)
	thistest = name
	local r = test()
	thistest = ""
	if not r then
		_io:write_to_stdout("[FAILED TEST] " .. name .. "\n")
		rv = 1
	end
end

function test_global()
	a = "xy"
	local s = a .. _g["a"] .. _g.a
	if s == "xyxyxy" then
		return true
	end
	return false
end

function test_zlib()
	local original = "test string asdfjaslkdf @#$@#$ @#$@# $ aslkjfasdd #@$!_(^&%($ asdfasdfdasfasd"
	local buffer = _util:convert_string_to_buffer(original)
	local buffer_size = _util:get_buffer_size(buffer)
	local compressed = _util:deflate(buffer)
	local compressed_size = _util:get_buffer_size(compressed)
	local restored = _util:inflate(compressed)
	local restored_size = _util:get_buffer_size(restored)
	local newstring = _util:convert_buffer_to_string(restored)
	if newstring ~= original then
		error("restored string different from original: `" .. newstring .. "'")
		return false
	end
	if compressed_size > buffer_size then
		error("compressed_size " .. compressed_size .. " > buffer_size " .. buffer_size)
		return false
	end
	if restored_size ~= buffer_size then
		error("restored_size " .. restored_size .. " != buffer_size " .. buffer_size)
		return false
	end
	info("buffer_size: " .. buffer_size)
	info("compressed_size: " .. compressed_size)
	info("restored_size: " .. restored_size)
	return true
end

function test_bcrypt()
	local factor = 12
	local original = "florentinoortegaIII"
	local salt = _bcrypt:generate_salt(factor)
	local hash = _bcrypt:hash_password(original, salt)
	local validation = _bcrypt:check_password(original, hash)
	info("generated salt: " .. salt)
	info("hashed password: " .. hash)
	info("password validation: " .. validation)
	return true
end

function test_image()
	local width = 100
	local height = 100
	local sz = width * height * 4
	local buffer = _util:allocate_buffer(sz)
	local ii = 0
	while ii < sz do
		do buffer[(function() local v = ii ii = ii + 1 return v end)() + 1] = 0 end
		do buffer[(function() local v = ii ii = ii + 1 return v end)() + 1] = 0 end
		do buffer[(function() local v = ii ii = ii + 1 return v end)() + 1] = 0 end
		do buffer[(function() local v = ii ii = ii + 1 return v end)() + 1] = 255 end
	end
	local png = _image:encode_png_data(buffer, width, height)
	if png == nil then
		error("Failed to encode png data")
		return false
	end
	return true
end

execute("test_global", test_global)
execute("test_zlib", test_zlib)
execute("test_bcrypt", test_bcrypt)
execute("test_image", test_image)

return rv
