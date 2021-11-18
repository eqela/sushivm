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

function is_not_equal(value1, value2)
	local a = _util:create_string_for_float(value1)
	local b = _util:create_string_for_float(value2)
	return a ~= b
end

function test_math()
	if _math:abs(200) ~= 200 then
		error("abs is incorrect")
		return false
	end
	if _math:fabs(200) ~= 200 then
		error("fabs is incorrect")
		return false
	end
	if is_not_equal(_math:acos(-0.50), 2.0943951023932) then
		error("acos is incorrect")
		return false
	end
	if is_not_equal(_math:asin(-0.50), -0.5235987755983) then
		error("asin is incorrect")
		return false
	end
	if is_not_equal(_math:atan(1.0), 0.78539816339745) then
		error("atan is incorrect")
		return false
	end
	if not is_not_equal(_math:atan2(2.53, -10.2), 2.898460284234) then
		error("atan2 is incorrect")
		return false
	end
	if _math:ceil(2.3) ~= 3 then
		error("ceil is incorrect")
		return false
	end
	if is_not_equal(_math:cos(1.2), 0.36235775447667) then
		error("cos is incorrect")
		return false
	end
	if is_not_equal(_math:cosh(2.8), 8.25272841686113) then
		error("cosh is incorrect")
		return false
	end
	if is_not_equal(_math:exp(1.6), 4.95303242439511) then
		error("exp is incorrect")
		return false
	end
	if _math:floor(1.2) ~= 1 then
		error("floor is incorrect")
		return false
	end
	if is_not_equal(_math:remainder(2.8, 2.5), 0.3) then
		error("remainder is incorrect")
		return false
	end
	if is_not_equal(_math:log(0.1758, 1), -1.73840829373106) then
		error("log is incorrect")
		return false
	end
	if is_not_equal(_math:log10(0.1758, 1), -0.75498112926225) then
		error("log10 is incorrect")
		return false
	end
	if _math:max_float(2.8, 3.0) ~= 3 then
		error("max_float is incorrect")
		return false
	end
	if is_not_equal(_math:min_float(1.6, 1.2), 1.20000004768372) then
		error("min_float is incorrect")
		return false
	end
	if is_not_equal(_math:pow(2.5, 3.4), 22.54218602980021) then
		error("pow is incorrect")
		return false
	end
	if _math:round(2.8) ~= 3 then
		error("round is incorrect")
		return false
	end
	if is_not_equal(_math:round_with_mode(0.1758, 2), 0.18) then
		error("round_with_mode is incorrect")
		return false
	end
	if is_not_equal(_math:sin(0.500000), 0.4794255386042) then
		error("sin is incorrect")
		return false
	end
	if is_not_equal(_math:sinh(0.500000), 0.52109530549375) then
		error("sinh is incorrect")
		return false
	end
	if is_not_equal(_math:sqrt(0.1758), 0.41928510586473) then
		error("sqrt is incorrect")
		return false
	end
	if is_not_equal(_math:tan(0.1758), 0.17763374304578) then
		error("tan is incorrect")
		return false
	end
	if is_not_equal(_math:tanh(0.1758), 0.1740110418049) then
		error("tanh is incorrect")
		return false
	end
	return true
end

function test_udp_socket()
	local fd = _net:create_udp_socket()
	if fd < 0 then
		error("Failed to create UDP socket")
		return false
	end
	local data = _util:convert_string_to_buffer("test")
	if _net:send_udp_data(fd, data, -1, "127.0.0.1", 1234, 0) < 1 then
		error("Failed to send UDP data")
		return false
	end
	local host, port = _net:get_udp_socket_local_address(fd)
	info("UDP socket host: " .. host)
	info("UDP socket port: " .. port)
	local fd2 = _net:create_udp_socket()
	if _net:send_udp_data(fd2, data, -1, "127.0.0.1", port, 0) < 4 then
		error("Failed to send UDP data (2)")
		return false
	end
	local host2, port2 = _net:get_udp_socket_local_address(fd2)
	info("UDP socket2 host: " .. host2)
	info("UDP socket2 port: " .. port2)
	local data2 = _util:allocate_buffer(32)
	local r, peerhost, peerport = _net:read_udp_data(fd, data2, -1, 0)
	if r ~= 4 then
		error("Failed to receive UDP data")
		return false
	end
	info("UDP tests successful")
	_net:close_udp_socket(fd)
	_net:close_udp_socket(fd2)
	return true
end

execute("test_global", test_global)
execute("test_zlib", test_zlib)
execute("test_bcrypt", test_bcrypt)
execute("test_image", test_image)
execute("test_math", test_math)
execute("test_udp_socket", test_udp_socket)

return rv
