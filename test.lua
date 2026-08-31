process("libresplit")
require("busted.runner")()
local assert = require("luassert")
local say = require("say")

local function identical(_, arguments)
	local a = arguments[1]
	local b = arguments[2]
	return a == b and type(a) == type(b) and tostring(a) == tostring(b)
end

local function is_uint64(_, arguments)
	local a = arguments[1]
	return type(a) == "cdata" and tostring(a):match("ULL$")
end

local function is_sint64(_, arguments)
	local a = arguments[1]
	return type(a) == "cdata" and tostring(a):match("[^U]LL$")
end

say:set_namespace("en")
say:set("assertion.identical.positive", "Expected objects to be identical.\nPassed in:\n%s\nExpected:\n%s")
say:set("assertion.identical.negative", "Expected objects to not be identical.\nPassed in:\n%s\nDid not expect:\n%s")
assert:register("assertion", "identical", identical, "assertion.identical.positive", "assertion.identical.negative")

say:set("assertion.uint64.positive", "Expected object to be an unsigned FFI integer.\nPassed in:\n%s\nExpected:\n%s")
say:set(
	"assertion.uint64.negative",
	"Expected object to not be an unsigned FFI integer.\nPassed in:\n%s\nDid not expect:\n%s"
)
assert:register("assertion", "uint64", is_uint64, "assertion.uint64.positive", "assertion.uint64.negative")

say:set("assertion.sint64.positive", "Expected object to be a signed FFI integer.\nPassed in:\n%s\nExpected:\n%s")
say:set(
	"assertion.sint64.negative",
	"Expected object to not be a signed FFI integer.\nPassed in:\n%s\nDid not expect:\n%s"
)
assert:register("assertion", "sint64", is_sint64, "assertion.sint64.positive", "assertion.sint64.negative")

describe("bitwise function", function()
	describe("b_and", function()
		it("should support FFI integers", function()
			assert.equal(0b0110, b_and(0b1110LL, 0b0111LL))
			assert.equal(0x20, b_and(0x120, 0x71LL))
			assert.equal(0, b_and(99LL, 0))
			assert.equal(-4ULL, b_and(-3LL, -2LL))
			assert(b_and(2LL ^ 53 + 1, -1LL) > 2LL ^ 53)
		end)

		it("should return FFI integers where appropriate", function()
			assert.equal("number", type(b_and(3, 1)))
			assert.equal("cdata", type(b_and(3LL, 1LL)))
			assert.equal("cdata", type(b_and(3LL, 1)))
		end)
	end)

	describe("b_or", function()
		it("should support FFI integers", function()
			assert.equal(0b1111, b_or(0b1110LL, 0b0111LL))
			assert.equal(0x171, b_or(0x120, 0x71LL))
			assert.equal(99, b_or(99LL, 0LL))
			assert.equal(-1ULL, b_or(-3LL, -2LL))
			assert(b_or(2LL ^ 53, 1) > 2LL ^ 53)
		end)

		it("should return FFI integers where appropriate", function()
			assert.equal("number", type(b_or(2, 1)))
			assert.equal("cdata", type(b_or(2LL, 1LL)))
			assert.equal("cdata", type(b_or(2LL, 1)))
		end)
	end)

	describe("b_xor", function()
		it("should support FFI integers", function()
			assert.equal(0b1001, b_xor(0b1110LL, 0b0111LL))
			assert.equal(0x151, b_xor(0x120, 0x71LL))
			assert.equal(99, b_xor(99LL, 0LL))
			assert.equal(3, b_xor(-3LL, -2LL))
			assert(b_xor(2LL ^ 53, 1) > 2LL ^ 53)
		end)

		it("should return FFI integers where appropriate", function()
			assert.equal("number", type(b_xor(6, 3)))
			assert.equal("cdata", type(b_xor(6LL, 3LL)))
			assert.equal("cdata", type(b_xor(6LL, 3)))
		end)
	end)

	describe("b_not", function()
		it("should support FFI integers", function()
			assert.equal(-1ULL, b_not(0LL))
			assert.equal(-10ULL, b_not(10) + 1)
			assert.equal(10, b_not(-10LL - 1))
		end)

		it("should return FFI integers where appropriate", function()
			assert.equal("number", type(b_not(-1)))
			assert.equal("cdata", type(b_not(0)))
			assert.equal("cdata", type(b_not(0LL)))
			assert.equal("cdata", type(b_not(-1LL)))
			assert.equal("cdata", type(b_not(-1ULL)))
		end)
	end)

	describe("b_lshift", function()
		it("should support FFI integers", function()
			assert.equal(2 ^ 2, b_lshift(1LL, 2LL))
			assert.equal(40, b_lshift(10LL, 2))
			assert.equal(0x8000000000000000LL, b_lshift(1, 63))
			assert.equal(1, b_lshift(1, 64)) -- only lowest 6 bits used
			assert(b_lshift(1LL, 53) + 1 > b_lshift(1LL, 53))
			assert(b_lshift(1LL, 63) + 1 > b_lshift(1LL, 63))
		end)

		it("should return FFI integers where appropriate", function()
			assert.equal("number", type(b_lshift(1, 0)))
			assert.equal("number", type(b_lshift(1, 52)))
			assert.equal("cdata", type(b_lshift(1, 53)))
			assert.equal("cdata", type(b_lshift(1LL, 0)))
			assert.equal("cdata", type(b_lshift(1, 0LL)))
		end)
	end)

	describe("b_rshift", function()
		it("should support FFI integers", function()
			assert.equal(16, b_rshift(64LL, 2LL))
			assert.equal(2, b_rshift(10LL, 2))
			assert.equal(1, b_rshift(1, 0))
			assert.equal(1, b_rshift(1, 64)) -- only lowest 6 bits used
			assert.equal(1, b_rshift(-1LL, 63))
			assert(b_lshift(1LL, 53) + 1 > b_lshift(1LL, 53))
			assert(b_lshift(1LL, 63) + 1 > b_lshift(1LL, 63))
		end)

		it("should return FFI integers where appropriate", function()
			assert.equal("number", type(b_rshift(1, 0)))
			assert.equal("number", type(b_rshift(2 ^ 53 - 1, 0)))
			assert.equal("cdata", type(b_rshift(2LL ^ 53 - 1, 0)))
			assert.equal("cdata", type(b_rshift(1LL, 0)))
			assert.equal("cdata", type(b_rshift(1, 0LL)))
		end)
	end)
end)

describe("lasr function", function()
	it("getBaseAddress should return uint64", function()
		assert.uint64(getBaseAddress())
		assert.uint64(getBaseAddress("libc"))
	end)

	it("getModuleSize should return uint64", function()
		assert.uint64(getModuleSize())
		assert.uint64(getModuleSize("libc"))
	end)

	it("sizeOf should return uint64", function()
		assert.identical(4ULL, sizeOf("uint"))
		assert.identical(8ULL, sizeOf("double"))
		assert.identical(1ULL, sizeOf("bool"))
	end)

	it("getPID should still return number", function()
		assert.equal("number", type(getPID()))
	end)

	it("readAddress should take and return FFI integers", function()
		local elf_magic = 0x464c457f -- '\x7fELF' in little endian
		local elf_mask = 0xffffffff
		assert.equal("number", type(readAddress("uint", 0LL)))
		assert.equal(elf_magic, readAddress("uint", 0LL))
		assert.sint64(readAddress("long", 0))
		assert.uint64(readAddress("ulong", 0))
		assert.equal(elf_magic, readAddress("uint", 0LL))
		assert.equal(elf_magic, b_and(elf_mask, readAddress("long", 0ULL)))
		assert.equal(elf_magic, b_and(elf_mask, readAddress("ulong", 0LL)))
	end)

	it("sig_scan should take and return FFI integers", function()
		assert.identical(-1LL, sig_scan("7f 45 4c 46", -1LL))
	end)

	it("getMaps should use uint64 for size/offset fields", function()
		for _, map in ipairs(getMaps()) do
			assert.uint64(map.start)
			assert.uint64(map["end"]) -- oh that's annoying
			assert.uint64(map.size)
		end
	end)
end)

it("functions should error when floating-point input is too large to be accurate", function()
	local bad = 2 ^ 53
	assert.equal(bad, bad + 1)
	assert.Not.equal(bad, bad - 1)

	io.write("\x1b[s") -- save cursor position
	assert(b_and(bad, 0) == nil)
	assert(b_or(bad, 0LL) == nil)
	assert(b_xor(bad, 0LL) == nil)
	assert(b_not(bad) == nil)
	assert(b_lshift(bad, 0) == nil)
	assert(b_rshift(bad, 0) == nil)
	assert.equal(nil, readAddress("uint", bad))
	assert.equal(nil, sig_scan("7f 45 4c 46", bad))
	io.write("\x1b[u") -- restore cursor position
	io.write("\x1b[J") -- clear from cursor to end of screen, erasing error messages
end)

describe("FFI integers", function()
	it("should concatenate with string", function()
		assert.equal("x = -1LL", "x = " .. -1LL)
		assert.equal("y = 2ULL", "y = " .. 2ULL)
		assert.equal("42LL <- important", 42LL .. " <- important")
		assert.equal("array[23ULL] @ offset -10LL", "array[" .. 23ULL .. "] @ offset " .. -10LL)
	end)

	it("should concatenate with numeric types", function()
		assert.equal("-2LL3", -2LL .. 3)
		assert.equal("1LL2LL", 1LL .. 2LL)
		assert.equal("4ULL5ULL", 4ULL .. 5ULL)
	end)

	-- stylua: ignore
	it("should not concatenate with other types", function()
		assert.error.matches(function() local _ = 1LL .. {} end, '^attempt to concatenate')
		assert.error.matches(function() local _ = 1LL .. false end, '^attempt to concatenate')
		assert.error.matches(function() local _ = 1LL .. function() end end, '^attempt to concatenate')
		assert.error.matches(function() local _ = 1LL .. nil end, '^attempt to concatenate')
	end)
end)
