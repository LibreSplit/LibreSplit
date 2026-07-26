## LibreSplit was unable to read memory from the target process.
* This is because in linux, a process cannot read the memory of another process that are unrelated
* This fix should ONLY be used if you REALLY want to run the linux native version of a game with a linux native auto splitter
* To fix this: **Run the game/program trough stam**
* If the above doesnt work for some reason, keep reading
### THIS WORKAROUND IS A HUGE SECURITY RISK, SO PLEASE ONLY DO IT IF ABSOLUTELY NECESSARY.
#### You should give such permission only to programs you fully trust: A vulnerability in a program with such permission could give full system-wide access to malicious actors
* Run `sudo setcap cap_sys_ptrace+ep /path/to/libresplit`
    * Replace `path/to/libresplit` with the actual path of the libresplit binary
* To revert back this capability run:
* `sudo setcap -r /path/to/libresplit`
    * Replace `path/to/libresplit` with the actual path of the libresplit binary

## Global hotkeys on wayland
* Global hotkeys on wayland are disabled by default when the `WAYLAND_DISPLAY` environment variable is set, if you want to enable them regardless of it, you can set `LIBRESPLIT_FORCE_GLOBAL_HOTKEYS` to `1` or anything and they will be enabled, expect it to be somewhat unreliable

## Memory offsets are wrong/dont work
* This might be to some bug in fetching maps with ioctl
* You can disable ioctl behaviour by setting `LIBRESPLIT_DISABLE_IOCTL_MAPS` environment variable to `1`.

## Auto splitter errors

### `ERROR: Floating-point argument is too large to represent a valid integer`
Lua `'number'`s are 64-bit floating-point values, and thus cannot represent all possible 64-bit
addresses. Values larger than `2^53` are rounded to the nearest multiple of some power of two,
wreaking havoc on memory reads and bitwise operations. For this reason, you must use 64-bit
[FFI integers](https://luajit.org/ext_ffi_api.html#literals) to pass large values to functions:
```lua
print(2^53 == 2^53 + 1)      --> true
print(2LL^53 == 2LL^53 + 1)  --> false

local far_away = readAddress('uint', 0x0020000000000000)
    --> [readAddress] ERROR: Floating-point argument is too large to represent a valid integer. Please write 0x20000000000000LL instead of 0x20000000000000. Check your autosplitter code.
    --> far_away == nil
local far_away = readAddress('uint', 0x0020000000000000LL)
    --> OK
```
Here, `readAddress()` throws an error because it cannot distinguish between the addresses
`0x0020000000000000` (`2^53`) and `0x0020000000000001` (`2^53 + 1`) as floating-point values. By
passing 64-bit integers with the `LL` suffix, we can represent all possible addresses.

### `bad argument #1 to 'abs' (number expected, got cdata)`
For reasons described in the previous section, many auto splitter functions (`sizeOf()`,
`readAddress('long', ...)`, etc.) return 64-bit [FFI integers](https://luajit.org/ext_ffi_api.html#literals)
rather than regular (floating-point) `'number'`s. These values have a `type()` of `'cdata'` and
cannot be passed directly to most Lua standard library functions. Pass them first to `tonumber()`,
but be aware that this may result in rounding if the input is larger than `2^53`.

```lua
local index = readAddress('ulong', 0x1234)
print(index)  --> '10ULL'
local substring = buffer:sub(index)
    --> bad argument #1 to 'sub' (number expected, got cdata)
local substring = buffer:sub(tonumber(index))
    --> OK
```
