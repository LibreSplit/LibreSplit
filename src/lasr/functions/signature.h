#pragma once

#include <lua.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Represents one parsed signature byte.
 *
 * `value` holds the expected bits and `mask` selects which bits are compared.
 * Examples:
 * - exact byte `AB`  => value=0xAB, mask=0xFF
 * - wildcard `??`    => value=0x00, mask=0x00
 * - nibble `A?`      => value=0xA0, mask=0xF0
 */
typedef struct SigByte {
    uint8_t value; /*!< Expected bits for this signature byte */
    uint8_t mask; /*!< Bit mask selecting which bits are compared */
} SigByte;

/**
 * Matcher metadata used to accelerate signature lookup.
 *
 * The matcher keeps:
 * - the parsed signature bytes
 * - one exact-byte anchor used for fast candidate search
 * - an optional second exact-byte check for early rejection
 */
typedef struct SigMatcher {
    const SigByte* signature; /*!< Parsed signature bytes */
    size_t signature_len; /*!< Number of bytes in signature */
    bool has_anchor; /*!< Whether an exact-byte anchor exists */
    size_t anchor_pos; /*!< Index of the anchor byte in signature */
    uint8_t anchor_byte; /*!< Exact byte value used as anchor */
    bool has_check; /*!< Whether a second exact-byte check exists */
    size_t check_pos; /*!< Index of the secondary check byte */
    uint8_t check_byte; /*!< Exact byte value used for secondary check */
} SigMatcher;

int perform_sig_scan(lua_State* L);
