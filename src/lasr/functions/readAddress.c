#include "readAddress.h"
#include "../utils.h"

#include <errno.h>
#include <lua.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool memory_error = false;

/**
 * Reads an address from memory and interprets it as value_type, creating the relative
 * read_memory_<value_type> function
 *
 * @param value_type The output type interpretation
 */
#define READ_MEMORY_FUNCTION(value_type)                                                         \
    value_type read_memory_##value_type(uint64_t mem_address, int32_t* err)                      \
    {                                                                                            \
        value_type value = 0;                                                                    \
                                                                                                 \
        struct iovec mem_local;                                                                  \
        struct iovec mem_remote;                                                                 \
                                                                                                 \
        mem_local.iov_base = &value;                                                             \
        mem_local.iov_len = sizeof(value);                                                       \
        mem_remote.iov_len = sizeof(value);                                                      \
        mem_remote.iov_base = (void*)(uintptr_t)mem_address;                                     \
                                                                                                 \
        ssize_t mem_n_read = process_vm_readv(process.pid, &mem_local, 1, &mem_remote, 1, 0);    \
        if (mem_n_read == -1) {                                                                  \
            *err = (int32_t)errno;                                                               \
            memory_error = true;                                                                 \
        } else if (mem_n_read != (ssize_t)mem_remote.iov_len) {                                  \
            printf("Error reading process memory: short read of %ld bytes\n", (long)mem_n_read); \
        }                                                                                        \
                                                                                                 \
        return value;                                                                            \
    }

READ_MEMORY_FUNCTION(int8_t)
READ_MEMORY_FUNCTION(uint8_t)
READ_MEMORY_FUNCTION(int16_t)
READ_MEMORY_FUNCTION(uint16_t)
READ_MEMORY_FUNCTION(int32_t)
READ_MEMORY_FUNCTION(uint32_t)
READ_MEMORY_FUNCTION(int64_t)
READ_MEMORY_FUNCTION(uint64_t)
READ_MEMORY_FUNCTION(float)
READ_MEMORY_FUNCTION(double)
READ_MEMORY_FUNCTION(bool)

/**
 * Reads an address from memory and interprets it as a string.
 *
 * @param mem_address The memory address to read from.
 * @param buffer_size The size of the buffer to allocate.
 * @param err A pointer to an error flag to write to.
 *
 * @return The string read from memory.
 */
char* read_memory_string(uint64_t mem_address, int buffer_size, int32_t* err)
{
    char* buffer = (char*)malloc(buffer_size);
    if (buffer == NULL) {
        // Handle memory allocation failure
        return NULL;
    }

    struct iovec mem_local;
    struct iovec mem_remote;

    mem_local.iov_base = buffer;
    mem_local.iov_len = buffer_size;
    mem_remote.iov_len = buffer_size;
    mem_remote.iov_base = (void*)(uintptr_t)mem_address;

    ssize_t mem_n_read = process_vm_readv(process.pid, &mem_local, 1, &mem_remote, 1, 0);
    if (mem_n_read == -1) {
        buffer[0] = '\0';
        *err = (int32_t)errno;
        memory_error = true;
    } else if (mem_n_read != (ssize_t)mem_remote.iov_len) {
        printf("Error reading process memory: short read of %ld bytes\n", (long)mem_n_read);
        exit(1);
    }

    return buffer;
}

/**
 * Reads a memory address given by the Lua Auto Splitter.
 *
 * @param L The Lua state.
 */
int readAddress(lua_State* L)
{
    if (lua_gettop(L) == 0) {
        // There must be at least 2 arguments: type and address
        printf("[readAddress] Two arguments are required: type and address. Check your auto splitter code.\n");
        lua_pushnil(L);
        return 1;
    }
    if (!lua_isstring(L, 1)) {
        // The "type" argument is not a string. This will bring a segfault if left alone.
        printf("[readAddress] The type to be read must be a string. Check your auto splitter code.\n");
        lua_pushnil(L);
        return 1;
    }
    memory_error = false;
    uint64_t address;
    const char* value_type = lua_tostring(L, 1);
    int i;

    if (lua_isnil(L, 2)) {
        // The address is NULL, this will bring a segfault if left alone
        printf("[readAddress] The address argument cannot be nil. Check your auto splitter code.\n");
        lua_pushnil(L);
        return 1;
    }

    if (lua_isnumber(L, 2)) {
        address = process.base_address + lua_tointeger(L, 2);
        i = 3;
    } else {
        const char* module = lua_tostring(L, 2);
        if (strcmp(process.name, module) != 0) {
            process.dll_address = find_base_address(module);
        }
        address = process.dll_address + lua_tointeger(L, 3);
        i = 4;
    }

    int error = 0;

    for (; i <= lua_gettop(L); i++) {
        if (address <= UINT32_MAX) {
            address = read_memory_uint32_t((uint64_t)address, &error);
            if (memory_error)
                break;
        } else {
            address = read_memory_uint64_t(address, &error);
            if (memory_error)
                break;
        }
        address += lua_tointeger(L, i);
    }

    if (strcmp(value_type, "sbyte") == 0) {
        int8_t value = read_memory_int8_t(address, &error);
        lua_pushinteger(L, value);
    } else if (strcmp(value_type, "byte") == 0) {
        uint8_t value = read_memory_uint8_t(address, &error);
        lua_pushinteger(L, value);
    } else if (strcmp(value_type, "short") == 0) {
        int16_t value = read_memory_int16_t(address, &error);
        lua_pushinteger(L, value);
    } else if (strcmp(value_type, "ushort") == 0) {
        uint16_t value = read_memory_uint16_t(address, &error);
        lua_pushinteger(L, value);
    } else if (strcmp(value_type, "int") == 0) {
        int32_t value = read_memory_int32_t(address, &error);
        lua_pushinteger(L, value);
    } else if (strcmp(value_type, "uint") == 0) {
        uint32_t value = read_memory_uint32_t(address, &error);
        lua_pushinteger(L, value);
    } else if (strcmp(value_type, "long") == 0) {
        // TODO: Fix 64 bit numbers, luajit 5.1 doesnt support 64 bit numbers natively
        int64_t value = read_memory_int64_t(address, &error);
        lua_pushinteger(L, value);
    } else if (strcmp(value_type, "ulong") == 0) {
        // TODO: Fix 64 bit numbers, luajit 5.1 doesnt support 64 bit numbers natively
        uint64_t value = read_memory_uint64_t(address, &error);
        lua_pushinteger(L, value);
    } else if (strcmp(value_type, "float") == 0) {
        float value = read_memory_float(address, &error);
        lua_pushnumber(L, value);
    } else if (strcmp(value_type, "double") == 0) {
        double value = read_memory_double(address, &error);
        lua_pushnumber(L, value);
    } else if (strcmp(value_type, "bool") == 0) {
        bool value = read_memory_bool(address, &error);
        lua_pushboolean(L, value ? 1 : 0);
    } else if (strstr(value_type, "string") != NULL) {
        int buffer_size = atoi(value_type + 6);
        if (buffer_size < 2) {
            printf("[readAddress] Invalid string size, please read documentation");
            exit(1);
        }
        char* value = read_memory_string(address, buffer_size, &error);
        lua_pushstring(L, value != NULL ? value : "");
        free(value);
    } else if (strstr(value_type, "byte")) {
        int array_size = atoi(value_type + 4);
        if (array_size < 1) {
            printf("[readAddress] Invalid byte array size, please read documentation");
            exit(1);
        }
        uint8_t* results = malloc(array_size * sizeof(uint8_t));
        if (!results) {
            printf("[readAddress] Memory allocation failed for byte array.\n");
            exit(1);
        }
        for (int j = 0; j < array_size; j++) {
            uint8_t value = read_memory_uint8_t(address + j, &error);
            if (memory_error)
                break;
            results[j] = value;
        }

        // Now that we have the results, push them to Lua table
        // This is because if the read_memory fails midway, we don't want to push partial data
        // And also want to avoid pushing the fallback result as part of the table
        if (!memory_error) {
            lua_createtable(L, array_size, 0);
            for (int j = 0; j < array_size; j++) {
                uint8_t value = results[j];
                lua_pushinteger(L, value);
                lua_rawseti(L, -2, j + 1);
            }
        }
        free(results);
    } else {
        printf("[readAddress] Invalid value type: %s\n", value_type);
        exit(1);
    }

    if (memory_error) {
        lua_pushnil(L);
        handle_memory_error(error);
    }

    return 1;
}
