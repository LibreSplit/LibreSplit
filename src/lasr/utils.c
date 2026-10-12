#include "utils.h"
#include "../gui/dialogs.h"
#include "./auto-splitter.h"
#include "./maps/maps.h"

#include <elf.h>
#include <endian.h>
#include <errno.h>
#include <glib.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static const char dos_signature[] = { 'M', 'Z' };
static const char pe_signature[] = { 'P', 'E', '\0', '\0' };

#define DOS_HEADER_SIZE 64
#define DOS_E_LFANEW_OFFSET 0x3c
#define COFF_HEADER_SIZE 20
#define COFF_OPTIONAL_HEADER_OFFSET 16
#define PE_OPTIONAL_HEADER_OFFSET (sizeof(pe_signature) + COFF_HEADER_SIZE)
#define PE32_MAGIC_BYTE 0x10b
#define PE32_PLUS_MAGIC_BYTE 0x20b

game_process process;

/**
 * Restarts the auto splitter by disabling it and re-enabling it again
 *
 * @return true if the auto splitter was enabled before the restart, false otherwise
 */
bool restart_auto_splitter(void)
{
    const bool was_asl_enabled = atomic_load(&auto_splitter_enabled);
    if (was_asl_enabled) {
        stop_auto_splitter();
        atomic_store(&auto_splitter_enabled, true);
    }
    return was_asl_enabled;
}

/**
 * Gets the base address of a module.
 *
 * @param module The module name for which to find the base address of. If NULL, the main process is used.
 *
 * @return The base address of the chosen module.
 */
uintptr_t find_base_address(const char* module)
{
    const char* module_to_grep = module == 0 ? process.name : module;

    ProcessMap map;
    const bool found = maps_findMapByName(module_to_grep, &map);
    if (found) {
        return map.start;
    }
    return 0;
}

/**
 * Prints a memory error to stdout.
 *
 * @param err The error code to print.
 *
 * @return True if the error was printed, false if the error is unknown.
 */
bool handle_memory_error(uint32_t err)
{
    static bool shownDialog = false;
    if (err == 0)
        return false;
    switch (err) {
        case EFAULT:
            printf("[readAddress] EFAULT: Invalid memory space/address\n");
            break;
        case EINVAL:
            printf("[readAddress] EINVAL: An error ocurred while reading memory\n");
            break;
        case ENOMEM:
            printf("[readAddress] ENOMEM: Please get more memory\n");
            break;
        case EPERM:
            printf("[readAddress] EPERM: Permission denied\n");

            if (!shownDialog) {
                shownDialog = true;
                g_idle_add(display_non_capable_mem_read_dialog, NULL);
            }

            break;
        case ESRCH:
            printf("[readAddress] ESRCH: No process with specified PID exists\n");
            break;
    }
    return true;
}

/**
 * Utility function to convert a lua value to a string.
 *
 * Converts a value to a printable C string according to its type.
 * This is due to lua_tostring returning "null" for booleans and
 * other non-string types.
 */
const char* value_to_c_string(lua_State* L, int index)
{
    switch (lua_type(L, index)) {
        case LUA_TSTRING:
            return lua_tostring(L, index);
        case LUA_TNUMBER:
            return lua_tostring(L, index);
        case LUA_TBOOLEAN:
            return lua_toboolean(L, index) ? "true" : "false";
        case LUA_TNIL:
            return "nil";
        default:
            return "??";
    }
}

static bool read_header(uint64_t base, uint32_t offset, void* buffer, size_t size, int32_t* err)
{
    // The remote address must also fit in the reader's iovec without truncation.
    if (base > UINTPTR_MAX || offset > UINTPTR_MAX - base || size - 1 > UINTPTR_MAX - base - offset) {
        *err = EOVERFLOW;
        return false;
    }

    struct iovec local = { .iov_base = buffer, .iov_len = size };
    struct iovec remote = { .iov_base = (void*)(uintptr_t)(base + offset), .iov_len = size };
    ssize_t count = process_vm_readv(process.pid, &local, 1, &remote, 1, 0);
    if (count != (ssize_t)size) {
        *err = count == -1 ? errno : EIO;
        return false;
    }

    return true;
}

static uint16_t read_le16(const uint8_t* bytes)
{
    return (uint16_t)bytes[0] | (uint16_t)bytes[1] << 8;
}

static uint32_t read_le32(const uint8_t* bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

/**
 * @brief Detect the size a pointer should use based on the module's mapped ELF or PE header.
 * Reads the header information from the process' pid. For native Linux modules, uses the
 * ELF class to distinguish 32-bit or 64-bit pointers. For windows modules, use the PE header
 * magic bytes to follow the game's actual format.
 *
 * @param module_address Base address of the mapped binary.
 * @param err Pointer to an integer to hold any error code.
 *            0         = success
 *            ENOEXEC   = invalid/unsupported header
 *            EIO       = incomplete header read
 *            EOVERFLOW = header address too big
 * @return PointerSize The size of the pointer or POINTER_SIZE_UNKNOWN on failure.
 */
PointerSize detect_pointer_size(uint64_t module_address, int32_t* err)
{
    uint8_t header[DOS_HEADER_SIZE];
    *err = 0;
    if (!read_header(module_address, 0, header, sizeof(header), err)) {
        return POINTER_SIZE_UNKNOWN;
    }

    if (memcmp(header, ELFMAG, SELFMAG) == 0) {
        switch (header[EI_CLASS]) {
            case ELFCLASS32:
                return POINTER_SIZE_32;

            case ELFCLASS64:
                return POINTER_SIZE_64;
        }
    } else if (memcmp(header, dos_signature, sizeof(dos_signature)) == 0) {
        *err = ENOEXEC;

        // decode e_lfanew from little-endian bytes
        uint32_t pe_offset = read_le32(header + DOS_E_LFANEW_OFFSET);
        if (pe_offset < sizeof(header)) {
            return POINTER_SIZE_UNKNOWN;
        }

        // PE signature + coff header + header magic
        uint8_t pe_header[PE_OPTIONAL_HEADER_OFFSET + sizeof(uint16_t)];
        if (!read_header(module_address, pe_offset, pe_header, sizeof(pe_header), err)) {
            return POINTER_SIZE_UNKNOWN;
        }

        if (memcmp(pe_header, pe_signature, sizeof(pe_signature)) != 0
            || read_le16(pe_header + sizeof(pe_signature) + COFF_OPTIONAL_HEADER_OFFSET) < sizeof(uint16_t)) {
            return POINTER_SIZE_UNKNOWN;
        }

        *err = 0;
        switch (read_le16(pe_header + PE_OPTIONAL_HEADER_OFFSET)) {
            case PE32_MAGIC_BYTE:
                return POINTER_SIZE_32;

            case PE32_PLUS_MAGIC_BYTE:
                return POINTER_SIZE_64;
        }
    }

    *err = ENOEXEC;
    return POINTER_SIZE_UNKNOWN;
}
