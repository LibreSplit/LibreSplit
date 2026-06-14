#pragma once

#include <stddef.h>
#include <stdint.h>

#define LIBRESPLIT_SOCK_NAME "libresplit.sock"

void getXDGruntimeDir(char* buffer, size_t size);

/**
 * A remote Libresplitctl message
 */
typedef struct __attribute__((__packed__)) CTLMessage {
    uint32_t length; /*!< Size of message in bytes */
    char message[]; /*!< The message sent with null termination*/
} CTLMessage;
