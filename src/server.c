#include "shared.h"

#include <arpa/inet.h>
#include <gtk/gtk.h>
#include <linux/limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

extern atomic_bool exit_requested;

/**
 * Structure to pass command data to main thread
 */
typedef struct CommandData {
    CTLCommand command; /*!< The command to send to the main thread */
} CommandData;

/**
 * External functions from main.c to handle commands
 *
 * @param command The command to be handled.
 */
extern void handle_ctl_command(CTLCommand command);

/**
 * Command execution function that runs on the main thread
 */
static gboolean execute_command_on_main_thread(gpointer data)
{
    CommandData* cmd_data = (CommandData*)data;

    // Call the main.c function to handle the command
    handle_ctl_command(cmd_data->command);

    g_free(cmd_data);
    return FALSE; // Remove from idle queue
}

// Forward declarations for command handlers

/**
 * Receives a message from the socket.
 *
 * @param sockfd The socket file descriptor.
 * @param out Pointer to the CTLMessage struct used as output.
 *
 * @return Zero if a full message was received, non-zero otherwise.
 */
int receive_message(int sockfd, CTLMessage** out)
{

    // Read header (blocking)
    uint32_t len = 0;
    ssize_t n = read(sockfd, &len, sizeof(len));
    if (n == 0)
        return 1; // client closed connection immediately
    len = ntohl(len);
    if (n != sizeof(len))
        return -1;

    CTLMessage* msg = malloc(sizeof(CTLMessage) + len);
    if (!msg) {
        fprintf(stderr, "Failed to allocate memory for control message.\n");
        return -1;
    }
    msg->length = len;

    size_t received = 0;
    while (received < len) {
        n = read(sockfd, msg->message + received, len - received);
        if (n <= 0) {
            free(msg);
            return -1;
        }
        received += n;
    }

    *out = msg;
    return 0;
}

/**
 * The remote control server thread.
 *
 * @param arg The arguments to pass to the thread
 */
void* ls_ctl_server(void* arg)
{
    prctl(PR_SET_NAME, "LS CTL Server", 0, 0, 0);

    char runtime_dir[PATH_MAX - 17];
    getXDGruntimeDir(runtime_dir, sizeof(runtime_dir));
    if (strlen(runtime_dir) == 0) {
        fprintf(stderr, "Failed to get LibreSplit socket path.\n");
        return 0;
    }

    char socket_path[PATH_MAX];
    snprintf(socket_path, PATH_MAX, "%s/%s", runtime_dir, LIBRESPLIT_SOCK_NAME);

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Failed to create socket");
        return 0;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socket_path);

    unlink(socket_path);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Failed to bind socket");
        close(server_fd);
        return 0;
    }

    if (listen(server_fd, 20) < 0) { // backlog for many short connections
        perror("Failed to listen on socket");
        close(server_fd);
        return 0;
    }

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50 * 1000; // 50ms

        int ret = select(server_fd + 1, &fds, NULL, NULL, &tv);

        if (ret < 0) {
            perror("select failed");
            continue;
        }

        if (ret == 0) { // Timeout
            if (atomic_load(&exit_requested)) {
                break;
            }
        }

        // If we reach here, the listening socket is readable
        if (FD_ISSET(server_fd, &fds)) {

            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd < 0) {
                perror("Failed to accept client connection");
                continue; // do not stop the server on accept errors
            }

            CTLMessage* msg = NULL;
            int result = receive_message(client_fd, &msg);

            if (result == 0) {
                if (msg->length == sizeof(CTLCommand)) {
                    CTLCommand command = *(CTLCommand*)msg->message;
                    CommandData* cmd_data = g_malloc(sizeof(CommandData));
                    cmd_data->command = command;

                    // Queue command execution on main thread
                    g_idle_add(execute_command_on_main_thread, cmd_data);
                } else {
                    printf("Invalid message length: %u (expected %zu)\n", msg->length, sizeof(CTLCommand));
                }

                free(msg);
            } else {
                printf("Client closed without sending a full message.\n");
            }

            close(client_fd);
        }
    }

    close(server_fd);
    unlink(socket_path);

    return 0;
}
