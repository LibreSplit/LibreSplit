/** \file ctl.c
 * Implementation of the Libresplitctl executable
 */
#include "shared.h"

#include <arpa/inet.h>
#include <endian.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/**
 * Prints a small help screen.
 *
 * Shows the available commands to the user.
 */
void print_help()
{
    printf("Available commands:\n");
    printf("  startorsplit  - Start the timer/Split if timer is running\n");
    printf("  stoporreset   - Stop the timer/Reset the timer if its stopped\n");
    printf("  cancel        - Cancel the run\n");
    printf("  unsplit       - Unsplit the timer\n");
    printf("  skipsplit     - Skip the current split\n");
    printf("  exit          - Closes LibreSplit\n");
    printf("  help          - Show this help message\n");
}

/**
 * Sends a command to LibreSplit via Unix Socket.
 *
 * @param cmd The LibreSplit command to send
 * @return True if the command is successfully sent, false otherwise.
 */
bool sendToLibreSplit(const CTLCommand cmd)
{
    char runtime_dir[PATH_MAX - 17];
    getXDGruntimeDir(runtime_dir, sizeof(runtime_dir));
    if (strlen(runtime_dir) == 0) {
        fprintf(stderr, "Failed to get LibreSplit socket path.\n");
        return false;
    }

    char socket_path[PATH_MAX];
    snprintf(socket_path, PATH_MAX, "%s/%s", runtime_dir, LIBRESPLIT_SOCK_NAME);

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Failed to create socket");
        return false;
    }

    struct sockaddr_un addr = { 0 };
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socket_path);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("Failed to connect to LibreSplit socket");
        close(sockfd);
        return false;
    }

    // We can send arbitrarily long messages, but for now we only need to send a single byte
    // Could be useful for future extensions though
    CTLMessage* ctl_msg = (CTLMessage*)malloc(sizeof(CTLMessage) + sizeof(cmd));
    if (!ctl_msg) {
        fprintf(stderr, "Failed to allocate memory for control message.\n");
        close(sockfd);
        return false;
    }
    ctl_msg->length = htonl(sizeof(cmd));
    memcpy(ctl_msg->message, &cmd, sizeof(cmd));

    int written = write(sockfd, ctl_msg, sizeof(CTLMessage) + sizeof(cmd));
    if (written != sizeof(CTLMessage) + sizeof(cmd)) {
        fprintf(stderr, "Failed to send command to LibreSplit.\n");
        close(sockfd);
        free(ctl_msg);
        return false;
    }
    free(ctl_msg);
    close(sockfd);

    return true;
}

/**
 * The main entrypoint for Libresplitctl
 */
int main(int argc, char* argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Error: This program accepts exactly 1 argument.\n");
        fprintf(stderr, "Try 'help' for a list of commands.\n");
        return 1;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "help") == 0) {
        print_help();
        return 0;
    }

    bool success = false;

    if (strcmp(cmd, "startorsplit") == 0) {
        success = sendToLibreSplit(CTL_CMD_START_SPLIT);
    } else if (strcmp(cmd, "stoporreset") == 0) {
        success = sendToLibreSplit(CTL_CMD_STOP_RESET);
    } else if (strcmp(cmd, "cancel") == 0) {
        success = sendToLibreSplit(CTL_CMD_CANCEL);
    } else if (strcmp(cmd, "unsplit") == 0) {
        success = sendToLibreSplit(CTL_CMD_UNSPLIT);
    } else if (strcmp(cmd, "skipsplit") == 0) {
        success = sendToLibreSplit(CTL_CMD_SKIP);
    } else if (strcmp(cmd, "exit") == 0) {
        success = sendToLibreSplit(CTL_CMD_EXIT);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        fprintf(stderr, "Try 'help' for a list of valid commands.\n");
        return 1;
    }

    if (!success) {
        return 1;
    }

    return 0;
}
