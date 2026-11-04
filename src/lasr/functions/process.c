#include "process.h"

#include "../auto-splitter.h"
#include "../utils.h"
#include "src/lasr/maps/maps.h"
#include "src/logging.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <lauxlib.h>
#include <limits.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <unistd.h>

extern atomic_bool auto_splitter_enabled;

#define PROCESS_FIELD_LIMIT (1024U * 1024U)

static bool select_last = true;
static bool can_select_process = false;

void set_process_selection(bool allowed)
{
    can_select_process = allowed;
}

void detach_process(void)
{
    if (process.pidfd >= 0) {
        close(process.pidfd);
    }

    process.pid = 0;
    process.pidfd = -1;
    process.base_address = 0;
    process.dll_address = 0;
    maps_clearCache();
}

void clear_process(void)
{
    if (process.pidfd >= 0) {
        close(process.pidfd);
    }

    free(process.name);
    process = (game_process) { .pidfd = -1 };
    can_select_process = false;
}

static bool resource_error(int error)
{
    return error == ENOMEM || error == EMFILE || error == ENFILE;
}

static ssize_t read_exe(int directory, char* path)
{
    ssize_t length = readlinkat(directory, "exe", path, PROCESS_FIELD_LIMIT + 1);
    if (length <= 0) {
        return length;
    }

    if (length > PROCESS_FIELD_LIMIT) {
        // A full buffer may be truncated; never compare its prefix.
        return 0;
    }

    path[length] = '\0';
    return length;
}

static ssize_t read_argv0(int directory, char* buffer)
{
    int fd = openat(directory, "cmdline", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    size_t used = 0;
    ssize_t length = 0;
    while (used < PROCESS_FIELD_LIMIT + 1) {
        ssize_t count = read(fd, buffer + used, PROCESS_FIELD_LIMIT + 1 - used);
        if (count <= 0) {
            length = count; // EOF before NUL rejects the incomplete field.
            break;
        }

        const char* end = memchr(buffer + used, '\0', (size_t)count);
        if (end) {
            length = end - buffer;
            break;
        }

        used += (size_t)count;
    }

    int error = length < 0 ? errno : 0;
    close(fd);
    errno = error;
    return length;
}

static const char* process_basename(const char* path, bool allow_backslash)
{
    const char* name = path;
    for (; *path; ++path) {
        if (*path == '/' || (allow_backslash && *path == '\\')) {
            name = path + 1;
        }
    }

    return name;
}

static bool process_matches(int directory, const char* name, char* buffer)
{
    errno = 0;
    ssize_t exe_length = read_exe(directory, buffer);
    if (exe_length < 0 && resource_error(errno)) {
        return false;
    }

    if (exe_length > 0 && strcmp(process_basename(buffer, false), name) == 0) {
        return true;
    }

    ssize_t argv0_length = read_argv0(directory, buffer);
    return argv0_length > 0 && strcmp(process_basename(buffer, true), name) == 0;
}

static pid_t get_pid(const char* name, bool last, char* buffer)
{
    DIR* proc = opendir("/proc");
    if (!proc) {
        LOG_ERRF("Cannot discover game process: %s", strerror(errno));
        return -1;
    }

    int procfd = dirfd(proc);
    if (procfd < 0) {
        LOG_ERRF("Cannot read process directory: %s", strerror(errno));
        closedir(proc);
        return -1;
    }

    pid_t* pids = NULL;
    size_t count = 0;
    int error = 0;
    while (!error) {
        errno = 0;
        struct dirent* entry = readdir(proc);
        if (!entry) {
            error = errno;
            break;
        }

        const char* text = entry->d_name;
        size_t length = strlen(text);
        if (!length || strspn(text, "0123456789") != length) {
            continue;
        }

        unsigned long number = strtoul(text, NULL, 10);
        if (errno || number == 0 || number > INT_MAX) {
            // Linux pid_t is a signed int.
            continue;
        }

        pid_t pid = (pid_t)number;
        int directory = openat(procfd, text, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (directory < 0) {
            if (!resource_error(errno)) {
                continue;
            }

            error = errno;
            break;
        }

        bool matches = process_matches(directory, name, buffer);
        if (!matches && resource_error(errno)) {
            error = errno;
        }

        close(directory);
        if (error) {
            break;
        }

        if (!matches) {
            continue;
        }

        if (count == SIZE_MAX / sizeof(*pids)) {
            error = EOVERFLOW;
            break;
        }

        pid_t* grown = realloc(pids, (count + 1) * sizeof(*pids));
        if (!grown) {
            error = ENOMEM;
            break;
        }

        pids = grown;
        pids[count++] = pid;
    }

    closedir(proc);

    pid_t result = count ? pids[0] : 0;
    for (size_t i = 1; i < count; ++i) {
        if (last ? pids[i] > result : pids[i] < result) {
            result = pids[i];
        }
    }

    free(pids);
    if (error) {
        LOG_ERRF("Cannot discover game process: %s", strerror(error));
        return -1;
    }

    return result;
}

bool attach_process(void)
{
    if (!process.name) {
        return false;
    }

    char* buffer = malloc(PROCESS_FIELD_LIMIT + 1);
    if (!buffer) {
        LOG_ERRF("Unable to allocate memory for process discovery: %s", strerror(errno));
        return false;
    }

    pid_t pid = get_pid(process.name, select_last, buffer);
    if (pid <= 0) {
        free(buffer);
        if (pid == 0) {
            printf("%s isn't running.\n", process.name);
        }

        return false;
    }

    char path[32];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    int directory = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) {
        if (resource_error(errno)) {
            LOG_ERRF("Cannot open game process: %s", strerror(errno));
        }

        free(buffer);
        return false;
    }

    int pidfd = pidfd_open(pid, 0);
    if (pidfd < 0) {
        if (errno != ESRCH && errno != EINTR) {
            LOG_ERRF("Cannot obtain game process handle: %s", strerror(errno));
        }

        close(directory);
        free(buffer);
        return false;
    }

    // Ensure the process opened still matches (i.e. did not close and become something else)
    bool matches = process_matches(directory, process.name, buffer);
    if (!matches && resource_error(errno)) {
        LOG_ERRF("Cannot validate game process: %s", strerror(errno));
    }

    free(buffer);
    close(directory);
    if (!matches) {
        close(pidfd);
        return false;
    }

    struct pollfd status = { .fd = pidfd, .events = POLLIN };
    int ready = poll(&status, 1, 0);
    if (ready != 0) {
        if (ready < 0 && errno != EINTR) {
            LOG_ERRF("Cannot check game process exit: %s", strerror(errno));
        }

        close(pidfd);
        return false;
    }

    int tries = 5;
    uintptr_t base_address = 0;

    do {
        // Keep discovery outside of lua sync to prevent access to detached process.
        lasr_lock();
        maps_clearCache();
        process.pid = (unsigned int)pid;
        base_address = find_base_address(NULL);
        if (base_address) {
            process.pidfd = pidfd;
            process.base_address = base_address;
            process.dll_address = base_address;
            lasr_unlock();
            break;
        }

        process.pid = 0;
        maps_clearCache();
        lasr_unlock();
        usleep(50000);
    } while (--tries > 0);

    if (!base_address) {
        LOG_ERR("Unable to find process base address");
        close(pidfd);
        return false;
    }

    printf("Process: %s\nPID: %u\n", process.name, process.pid);
    return true;
}

int find_process_id(lua_State* L)
{
    if (!can_select_process) {
        // TODO: Allow eventual definition of multiple game process selection for different version support etc.
        return luaL_error(L, "process() may only be called once");
    }

    can_select_process = false;
    printf("\033[2J\033[1;1H"); // Clear the console

    size_t length;
    const char* input = luaL_checklstring(L, 1, &length);
    luaL_argcheck(L, length > 0 && memchr(input, '\0', length) == NULL, 1, "process name must be non-empty and contain no NUL bytes");
    char* name = strdup(input);
    if (!name) {
        return luaL_error(L, "Unable to allocate the process name");
    }

    free(process.name);
    process.name = name;
    const char* sort = lua_tostring(L, 2);
    if (sort && strcmp(sort, "first") != 0 && strcmp(sort, "last") != 0) {
        printf("[process] Invalid sort argument '%s'. Use 'first' or 'last'. Falling back to last\n", sort);
        sort = NULL;
    }

    select_last = !sort || strcmp(sort, "last") == 0;
    maps_clearCache();
    while (atomic_load(&auto_splitter_enabled)) {
        if (attach_process()) {
            break;
        }

        usleep(100000);
    }

    return 0;
}
