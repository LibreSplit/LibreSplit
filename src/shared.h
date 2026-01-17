#pragma once

#include <stddef.h>
#include <stdint.h>

#define LIBRESPLIT_SOCK_NAME "libresplit.sock"

void getXDGruntimeDir(char* buffer, size_t size);

/**
 * Available Libresplitctl commands
 */
typedef enum CTLCommand : uint8_t {
    CTL_CMD_START_SPLIT, /*!< Start or split */
    CTL_CMD_STOP_RESET, /*!< Stop or reset */
    CTL_CMD_CANCEL, /*!< Cancel run */
    CTL_CMD_UNSPLIT, /*!< Undo split */
    CTL_CMD_SKIP, /*!< Skip split */
    CTL_CMD_EXIT, /*!< Exit */
} CTLCommand;

/**
 * A remote Libresplitctl message
 */
typedef struct __attribute__((__packed__)) CTLMessage {
    uint32_t length; /*!< Size of message in bytes */
    uint8_t message[]; /*!< The message sent */
} CTLMessage;
