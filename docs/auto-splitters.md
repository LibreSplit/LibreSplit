# Auto Splitters

* Auto splitters are scripts that automate the process of timing your run in a game by making splits automatically, starting, resetting, pausing, etc.

# How do they work?

* These work by reading into game's memory and determining when the timer should do something, like make a split.

* LibreSplit's autosplitting system works in a very similar way to LiveSplit's. The main difference is that LibreSplit uses Lua instead of C#. There are also some key differences:
    * Runs an entire Lua system instead of only supporting specifically named C# blocks.
        * This means you can run external functions outside of the ones LibreSplit executes.
    * Support for the entire Lua language, including the importing of libraries for tasks such as performance monitoring.

# How to make LibreSplit auto splitters

* It's somewhat easy if you know what you are doing or are porting an already existing one.

* First in the lua script goes a `process` function call with the name of the games process:

```lua
process('GameBlaBlaBla.exe')
```
* With this line, LibreSplit will repeatedly attempt to find this process and will not continue script execution until it is found.

* Next we have to define the basic functions. Not all are required and the ones that are required may change depending on the game or end goal, like if loading screens are included or not.
    * The order at which these run is the same as they are documented below.

### `startup`
 The purpose of this function is to specify how many times LibreSplit checks memory values and executes functions each second, the default is 60Hz. Usually, 60Hz is fine and this function can remain undefined. However, it's there if you need it. Its also useful to change other configuration about the script.
```lua
process('GameBlaBlaBla.exe')

function startup()
    refreshRate = 120
    useGameTime = true
end
```

### `state`
 The main purpose of this function is to assign memory values to Lua variables.
* Runs every 1000 / `refreshRate` milliseconds and when the script is enabled/loaded.

```lua
process('GameBlaBlaBla.exe')

local isLoading = false;

function startup()
    refreshRate = 120
end

function state()
    isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
end
```

* You may have noticed that we're assigning this `isLoading` variable with the result of the function `readAddress`. This function is part of LibreSplit's Lua context and its purpose is to read memory values. It's explained in detail at the bottom of this document.

### `update`
 The purpose of this function is to update local variables.
* Runs every 1000 / `refreshRate` milliseconds.
```lua
process('GameBlaBlaBla.exe')

local current = {isLoading = false};
local old = {isLoading = false};
local loadCount = 0

function startup()
    refreshRate = 120
end

function state()
    old.isLoading = current.isLoading;

    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
end

function update()
    if not current.isLoading and old.isLoading then
        loadCount = loadCount + 1;
    end
end
```
* We now have 3 variables, one represents the current state while the other the old state of isLoading, we also have loadCount getting updated in the `update` function which will store how many times we've entered the loading screen

### `start`
This tells LibreSplit when to start the timer.\
_Note: LibreSplit will ignore any start calls if the timer is running._
* Runs every 1000 / `refreshRate` milliseconds.
```lua
process('GameBlaBlaBla.exe')

local current = {isLoading = false};
local old = {isLoading = false};
local loadCount = 0

function startup()
    refreshRate = 120
end

function state()
    old.isLoading = current.isLoading;

    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
end

function update()
    if not current.isLoading and old.isLoading then
        loadCount = loadCount + 1;
    end
end

function start()
    return current.isLoading
end
```

### `split`
Tells LibreSplit to execute a split whenever it gets a true return.
    * Runs every 1000 / `refreshRate` milliseconds.
```lua
process('GameBlaBlaBla.exe')

local current = {isLoading = false};
local old = {isLoading = false};
local loadCount = 0

function startup()
    refreshRate = 120
end

function state()
    old.isLoading = current.isLoading;

    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
end

function update()
    if not current.isLoading and old.isLoading then
        loadCount = loadCount + 1;
    end
end

function start()
    return current.isLoading
end

function split()
    local shouldSplit = false;
    if current.isLoading and not old.isLoading then
        loadCount = loadCount + 1;
        shouldSplit = loadCount > 1;
    end

    return shouldSplit;
end
```
* Whoa lots of code, why didnt we just return if we are currently in a loading screen like in start? Because if we do, we will do multiple splits a second, the function runs multiple times and it would do lots of unwanted splits.
* To solve that, we only want to split when we enter a loading screen (old is false, current is true), but we also don't want to split on the first loading screen as we have the assumption that the first loading screen is when the run starts. So that's where our loadCount comes in handy, we can just check if we are on the first one and only split when we aren't.

### `isLoading`
Pauses the timer whenever true is being returned.
* Runs every 1000 / `refreshRate` milliseconds.
```lua
process('GameBlaBlaBla.exe')

local current = {isLoading = false, scene = ""};
local old = {isLoading = false, scene = ""};
local loadCount = 0

function startup()
    refreshRate = 120
end

function state()
    old.isLoading = current.isLoading;
    old.scene = current.scene;

    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    current.scene = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xBB, 0xEE, 0x55, 0xDD, 0xBA, 0x6A);
end

function update()
    if not current.isLoading and old.isLoading then
        loadCount = loadCount + 1;
    end
end

function start()
    return current.isLoading
end

function split()
    local shouldSplit = false;
    if current.isLoading and not old.isLoading then
        loadCount = loadCount + 1;
        shouldSplit = loadCount > 1;
    end

    return shouldSplit;
end

function isLoading()
    return current.isLoading
end
```
* Pretty self explanatory, since we want to return whenever we are currently in a loading screen, we can just send our current isLoading status, same as start.

# `reset`
Instantly resets the timer. Use with caution.
* Runs every 1000 / `refreshRate` milliseconds.
```lua
process('GameBlaBlaBla.exe')

local current = {isLoading = false};
local old = {isLoading = false};
local loadCount = 0
local didReset = false

function startup()
    refreshRate = 120
end

function state()
    old.isLoading = current.isLoading;

    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
end

function update()
    if not current.isLoading and old.isLoading then
        loadCount = loadCount + 1;
    end
end

function start()
    return current.isLoading
end

function split()
    local shouldSplit = false;
    if current.isLoading and not old.isLoading then
        loadCount = loadCount + 1;
        shouldSplit = loadCount > 1;
    end

    return shouldSplit;
end

function isLoading()
    return current.isLoading
end

function reset()
    if not old.scene == "MenuScene" and current.scene == "MenuScene" then
        return true
    end
    return false
end
```
* In this example we are checking for the scene, of course, the address is completely arbitrary and doesnt mean anything for this example. Specifically we are checking if we are entering the MenuScene scene.

# `gameTime`
### **When using `gameTime`, `isLoading` has to ALWAYS return true**
Function that is used to set the current timer time when `useGameTime` is `true` (`false` by default)
* The return value of this function should be the current time in milliseconds
* Runs every 1000 / `refreshRate` milliseconds.
```lua
process('GameBlaBlaBla.exe')

local current = {isLoading = false};
local old = {isLoading = false};
local loadCount = 0
local didReset = false
local IGT = 0

function startup()
    refreshRate = 120
end

function state()
    old.isLoading = current.isLoading;

    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    IGT = readAddress("int", "UnityPlayer.dll", 0x019B4878, ...);
end

function update()
    if not current.isLoading and old.isLoading then
        loadCount = loadCount + 1;
    end
end

function start()
    return current.isLoading
end

function split()
    local shouldSplit = false;
    if current.isLoading and not old.isLoading then
        loadCount = loadCount + 1;
        shouldSplit = loadCount > 1;
    end

    return shouldSplit;
end

function isLoading()
    return true
end

function reset()
    if not old.scene == "MenuScene" and current.scene == "MenuScene" then
        return true
    end
    return false
end

function gameTime()
    return IGT -- Assuming IGT is the current time in milliseconds tracked by the game
end
```
* In this example we added `IGT`, which is the variable in which the game keeps track of how long you've played for by some way or another, later this IGT variable is used as a return value to the `gameTime` function. Also the `useGameTime` is set to true to be able to use this feature


## readAddress
* `readAddress` is the second function that LibreSplit defines for us and its globally available, its job is to read the memory value of a specified address.
* The first value defines what kind of value we will read:
    1. `sbyte`: signed 8 bit integer
    2. `byte`: unsigned 8 bit integer
    3. `short`: signed 16 bit integer
    4. `ushort`: unsigned 16 bit integer
    5. `int`: signed 32 bit integer
    6. `uint`: unsigned 32 bit integer
    7. `long`: signed 64 bit integer
    8. `ulong`: unsigned 64 bit integer
    9. `float`: 32 bit floating point number
    10. `double`: 64 bit floating point number
    11. `bool`: Boolean (true or false)
    12. `stringX`, A string of characters. Its usage is different compared the rest, you type "stringX" where the X is how long the string can be plus 1, this is to allocate the NULL terminator which defines when the string ends, for example, if the longest possible string to return is "cheese", you would define it as "string7". Setting X lower can result in the string terminating incorrectly and getting an incorrect result, setting it higher doesnt have any difference (aside from wasting memory).
    13. `byteX`: An array of bytes, functions the same as `stringX`, but it reads bytes instead, the result is given in the form of an "array", also known as just a table that you can access with indexes, like `result[10]` will give you the 10th byte of whatever array you read

* The second argument can be 2 things, a string or a number.
    * If its a number: The value in that memory address of the main process will be used.
    * If its a string: It will find the corresponding map of that string, for example "UnityPlayer.dll", This means that instead of reading the memory of the main map of the process (main binary .exe), it will instead read the memory of UnityPlayer.dll's memory space.
        * Next you have to add another argument, this will be the offset at which to read from from the perspective of the base address of the module, meaning if the module is mapped to 0x1000 to 0xFFFF and you put 0x0100 in the offset, it will read the value in the address 0x1010.

* The rest of arguments are memory offsets or pointer paths.
    * A Pointer Path is a list of Offsets + a Base Address. The auto splitter reads the value at the base address and interprets the value as yet another address. It adds the first offset to this address and reads the value of the calculated address. It does this over and over until there are no more offsets. At that point, it has found the value it was searching for. This resembles the way objects are stored in memory. Every object has a clearly defined layout where each variable has a consistent offset within the object, so you basically follow these variables from object to object.

        * Cheat Engine is a tool that allows you to easily find Addresses and Pointer Paths for those Addresses, so you don't need to debug the game to figure out the structure of the memory.

## sig_scan

`sig_scan` performs a signature/pattern scan using the provided IDA-style byte array and an integer offset, It returns a numeric representation of the found address.

Example:

`signature = sig_scan("89 5C 24 ?? 89 44 24 ?? 74 ?? 48 8D 15", 4)`

Returns:

`5387832857`

(Which is the decimal representation of the address `0x14123ce19`)

### Notes

* `sig_scan` may require LibreSplit to have advanced memory-reading permissions, check the [troubleshooting guide](./troubleshooting.md) to see how to enable it. If such permissions are not given, LibreSplit may not be able to find some signatures.
* Lua automatically handles the conversion of hexadecimal strings to numbers, so parsing/casting it manually is not required. You can use the result of `sig_scan` directly into `readAddress`.
* Until the address is found, `sig_scan` returns a `nil` value.
* Signature scanning is an expensive action. So in most cases, we recommend avoiding scanning for a signature all the time, but using a variable as a "guard", this way as soon as `sig_scan` returns a valid value, the auto splitter will skip the expensive signature scanning.

Mini example script with the game SPRAWL:
```lua
process('Sprawl-Win64-Shipping.exe')

local featuretest = nil

function state()
    -- If our "guard variable" is nil, we didn't find an address yet...
    -- If our "guard variable" is not nil, we already found the memory address in a previous loop,
    -- so we skip further signature scanning
    if featuretest == nil then
        -- so we perform the signature scan to find the initial address
        featuretest = sig_scan("89 5C 24 ?? 89 44 24 ?? 74 ?? 48 8D 15", 4)
        -- Print a message to warn the user
        print("Signature scan did not find the address.")
    end
    -- When sig_scan returns a valid value, our guard variable will not be nil anymore,
    -- so we can continue with the rest of the auto splitter code
    if featuretest ~= nil then
        -- Read an integer value from the found address
        local readValue = readAddress('int', featuretest)
        print("Feature test address: ", featuretest)
        print("Read value: ", readValue)
    end
end
```

**Attention:** The `sig_scan` function will return an address that is automatically offset with the process base address, so it is ready to use with the `readAddress` function **without a module name**. Using `readAddress` with a module name is not supported and using a module name might result in wrong or out-of-process reads.

## getPID
* Returns the current PID

# Experimental stuff
## `mapsCacheCycles`

* When a `readAddress` that uses a memory map the biggest bottleneck is reading every line of `/proc/pid/maps` and checking if that line is the corresponding module. This option allows you to set for how many cycles the cache of that file should be used. The cache is global so it gets reset every x number of cycles.
    * `0`: Disabled completely
    * `1` (default): Enabled for the current cycle
    * `2`: Enabled for the current cycle and the next one
    * `3`: Enabled for the current cycle and the 2 next ones
    * You get the idea

### Performance
* Every uncached map finding takes around 1ms (depends a lot on your RAM and CPU)
* Every cached map finding takes around 100us

* Mainly useful for lots of `readAddress`-es and the game has an uncapped game state update rate, where literally every millisecond matters

### Example
```lua
function startup()
    refreshRate = 60;
    mapsCacheCycles = 1;
end

-- Assume all this readAddresses are different,
-- Instead of taking near 10ms it will instead take 1-2ms, because only this cycle is cached and the first readAddress is a cache miss, if the mapsCacheCycles were higher than 1 then a cycle could take less than half a millisecond
function state()
    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
    current.isLoading = readAddress("bool", "UnityPlayer.dll", 0x019B4878, 0xD0, 0x8, 0x60, 0xA0, 0x18, 0xA0);
end

```

## `getBaseAddress`
Returns the base address of a given Module. If called without arguments, or with the only accepted argument as `nil`, it will return the base address of the main module.

This can be useful for manual pointer manipulation, if required by the auto splitter.

Usage:

```
-- This gets the base address of the main process
local main_process_base_address = getbaseaddress()
-- This also gets the base address of the main process
local main_process_base_address = getbaseaddress(nil)
-- This gets the base address of another module
local module_base_address = getbaseaddress("UnityPlayer.dll")
```

## `sizeOf`
Returns the size of a given type. Uses the same type names as [readAddress](#readAddress) and will automatically size arrays and strings according to their length too.

```
-- Get the size of a 32 bit integer
local int_size = sizeOf("int")
-- Get the size of a 20-character string
local str_size = sizeOf("string20")
-- Get the size of a 25-byte array
local array_size = sizeOf("byte25")
```

**Warning:** As it is now, `sizeOf` returns the size in bytes. This may not be what is needed to properly work with pointers and may see some changes in the future for better integration with the rest of the Auto-Splitter Runtime.

## getModuleSize

Given a certain module name (or nothing/nil), returns the size of the module.

```lua
local main_module_size = getModuleSize();
local main_module_size_2 = getModuleSize(nil);
local other_module_size = getModuleSize("other_module");
```
## getMaps

Returns an array-like table that contains other tables: one for each of the process's memory maps.

The array will have a structure similar to this:

```lua
{
    {
        name="something",
        start=123456,
        end=789456,
        size=666000
    },
    {
        name="something_else",
        start=123456,
        end=789456,
        size=666000
    }
}
```

This way you can query memory maps for any property you want, using custom Lua code.

Usage:

```lua
local maps = getMaps()
```
