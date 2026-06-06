# Tips for coding your auto splitters

Here are some tips that may make coding your auto splitters easier.

## Linux versions are different

If you're thinking of porting an ASL auto splitter 1-to-1 to the **Linux version of the same game**, you might run into issues.

Here are some tips that might avoid you some headaches:

- Native Linux games might place variables in different addresses than their Windows counterparts: if an address from the ASL doesn't work, try searching the values using tools like [GameConqueror](https://sourceforge.net/projects/gameconqueror/), [PINCE](https://github.com/korcankaraokcu/PINCE) or [ComfyEngine](https://github.com/kashithecomfy/ComfyEngine)
- Linux memory addresses are usually 64 bits wide. So if you see an auto splitter that treats addresses as 32 bits integers, you may want to change that to 64 bits or you may point to the wrong places.
- More to be added...

## Use LibreSplit's "Utils"

LibreSplit includes a small series of utilities that aim to make developing and debugging your auto splitter a bit easier.

### `print_tbl`

If you try to `print()` a table, you will receive a not-very-useful `table: 0x123456`. This utility inspects a table for you and pretty-prints it.

It works only on flat, key-value tables (so it won't print nested tables).

**Note:** If you try to print a table with `nil` values, such values won't be printed. That is because in Lua deleting a key-value pair from a table is done by assigning `nil` to the key.

### `shallow_copy_tbl`

Tons of lines of code in many auto splitters are spent just transferring the `old state` to be able to compare it to the `current state`

```.lua
old.level = current.level
old.minutes = current.minutes
old.seconds = current.seconds
old.milliseconds = current.milliseconds
-- ...
current.level = readAddress(...)
current.minutes = readAddress(...)
-- ...
```

The `old.something=current.something` calls can all be replaced by `shallow_copy_tbl`, as long as your tables are on a single level (not nested):

```.lua
old = shallow_copy_tbl(current)
current.level = readAddress(...)
current.minutes = readAddress(...)
-- ...
```

### Bitwise Binary Operators

LuaJIT doesn't yet support bitwise binary operators, in the meantime we implemented some functions that will bring such features into LibreSplit:

- `b_and(a, b)`: Performs a bitwise "and" between the integers `a` and `b`;
- `b_or(a, b)`: Performs a bitwise "or" between the integers `a` and `b`;
- `b_xor(a, b)`: Performs a bitwise "exclusive or" between the integers `a` and `b`;
- `b_not(a)`: Performs a bitwise "not" on the integer `a`;
- `b_lshift(a, b)`: Performs a bitwise "left shift" on the integer `a` by `b`;
- `b_rshift(a, b)`: Performs a bitwise "right shift" on the integer `a` by `b`;

## A small trick to find items in table

If you use a table as an array:

```.lua
my_table = {"a", "place", "with", "complex items"}
```

You may need to test if an element is inside a table, which would require iterating through the entire table with the following code:

```.lua
local function has_value(tab, val)
    for _, value in ipairs(tab) do
        if value == val then
            return true
        end
    end
    return false
end

-- ...

has_pizza = has_value(my_table, "pizza")
```

Instead, you can trade off some memory space for efficiency, by using tables as associative maps with a "fake value":

```.lua
my_table = {a=true, place=true, with=true, ["complex items"]=true}
```

Then the check for presence is reduced to:

```.lua
has_pizza = my_table["pizza"] ~= nil
```

And it's faster too!

### str2ida

When you're working with `sig_scan`, many times the signature is just an identifying string purposefully inserted by the game programmer for speedrun timers.

In those cases you may want, for the sake of readability to use the string itself instead of its binary representation. If that's the case, `str2ida` is what you're looking for.

```.lua
-- This string is actually used in Pizza Tower
local signature = str2ida("magic id for speedrun data for livesplit")
print(signature)
-- It will print "6D 61 67 69 63 20 69 64 20 66 6F 72 20 73 70 65 65 64 72 75 6E 20 64 61 74 61 20 66 6F 72 20 6C 69 76 65 73 70 6C 69 74"
local speedrundata_pointer = sig_scan(signature, 0)
-- Continue with the auto splitter
```
