/** \file logging.c
 * Logger Rollster
 *
 * Asynchronous Logging Library for LibreSplit based on threads, circular queues,
 * hopes and dreams.
 *
 * This library is not built to support more than one instance of LibreSplit running
 * at a time. That would require the instances to be able to either have each its own
 * logfile (maybe `libresplit.pid.log`) or inter-process communication.
 */
#include "logging.h"
#include "settings/utils.h"
#include "src/timer.h"

#include <errno.h>
#include <linux/limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/*! The log queue, used as buffer */
static LogQueue logQueue;
/*! Atomic bool used to keep the thread active, might be used for clean closing in future */
static atomic_bool logging_active;
/*! Logging settings copied from the main thread to avoid race conditions */
static LoggingConfig config;
/*! Holds the filename for the logfile */
static const char log_filename[] = "libresplit.log";

/**
 * @brief Checks if a file is due for rotation by file size.
 * If so, rotates the log file in backwards order. Deletes
 * the oldest archive file if it's equivalent to max_files.
 * If the user's max_files is 0 then the log file is truncated instead.
 *
 * @return FILE* Pointer to the new log file if rotation occurs.
 */
static FILE* rotate_logs(FILE* logfile, const char* logpath)
{
    struct stat st;
    if (!logfile || fstat(fileno(logfile), &st) != 0) {
        fprintf(stderr, "cannot get log file size\n");
        return logfile;
    }

    if (st.st_size <= LOG_FILE_SIZE_LIMIT) {
        return logfile;
    }

    char archive_file[PATH_MAX + 2];
    int max_files = config.max_log_files.value.i;

    // iterate from 9 regardless of max_files to cleanup old archives if the user lowers the setting.
    for (int i = 9; i >= 1; --i) {
        snprintf(archive_file, sizeof(archive_file), "%s.%d", logpath, i);
        int result;
        if (i >= max_files) {
            result = unlink(archive_file);
        } else {
            char renamed_file[PATH_MAX + 2];
            snprintf(renamed_file, sizeof(renamed_file), "%s.%d", logpath, i + 1);
            result = rename(archive_file, renamed_file);
        }

        if (result != 0 && errno != ENOENT) {
            fprintf(stderr, "cannot rotate log file\n");
            return logfile;
        }
    }

    if (max_files == 0) {
        if (ftruncate(fileno(logfile), 0) != 0) {
            fprintf(stderr, "cannot truncate log file\n");
        } else {
            rewind(logfile);
        }

        return logfile;
    }

    snprintf(archive_file, sizeof(archive_file), "%s.1", logpath);
    if (rename(logpath, archive_file) != 0) {
        fprintf(stderr, "cannot rotate log file\n");
        return logfile;
    }

    FILE* new_logfile = fopen(logpath, "a");
    if (!new_logfile) {
        fprintf(stderr, "cannot open new log file\n");
        if (rename(archive_file, logpath) != 0) {
            fprintf(stderr, "failed to restore the log file\n");
        }

        return logfile;
    }

    fclose(logfile);
    return new_logfile;
}

/**
 * Initializes the log queue, ready to receive messages
 */
void initLogQueue(void)
{
    logQueue.head = 0;
    logQueue.tail = 0;
    pthread_mutex_init(&logQueue.lock, NULL);
    pthread_cond_init(&logQueue.cond, NULL);
    logging_active = 1;
}

/**
 * @brief Update the config settings in the logging thread.
 * To be called by settings save and on init.
 */
void update_logging_config(void)
{
    pthread_mutex_lock(&logQueue.lock);

    // clamp values
    if (cfg.logging.max_log_files.value.i > 9) {
        cfg.logging.max_log_files.value.i = 9;
    } else if (cfg.logging.max_log_files.value.i < 0) {
        cfg.logging.max_log_files.value.i = 0;
    }

    config.write_to_file.value = cfg.logging.write_to_file.value;
    config.max_log_files.value = cfg.logging.max_log_files.value;
    pthread_mutex_unlock(&logQueue.lock);
}

/**
 * Underlying function to all the LOG_* macros
 *
 * Works as a producer.
 *
 * @param[in] fmt The message to print in the log or the format string
 */
void logMessage(const char* fmt, ...)
{
    // Lock the mutex for writing
    pthread_mutex_lock(&logQueue.lock);
    // If the queue is full, wait (bottleneck)
    while ((logQueue.tail + 1) % LOG_QUEUE_SIZE == logQueue.head && atomic_load(&logging_active)) {
        pthread_cond_wait(&logQueue.cond, &logQueue.lock);
    }

    if (!atomic_load(&logging_active)) {
        pthread_mutex_unlock(&logQueue.lock);
        return;
    }

    // Create a timestamp for the log
    char timestamp[64];
    time_t current_time = time(NULL);
    struct tm* t = localtime(&current_time);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    // There is space in the queue, add a new message
    va_list args;
    va_start(args, fmt);
    // Put the timestamp first...
    snprintf(logQueue.message_queue[logQueue.tail], LOG_STR_LEN, "%s | ", timestamp);
    // The remaining space is for the message
    size_t prefix_len = strlen(timestamp) + 3;
    int written = vsnprintf(logQueue.message_queue[logQueue.tail] + prefix_len, LOG_STR_LEN - prefix_len, fmt, args);
    if (written >= (int)(LOG_STR_LEN - prefix_len)) {
        // ensure truncated log messages preserve the new line character.
        logQueue.message_queue[logQueue.tail][LOG_STR_LEN - 2] = '\n';
    }

    va_end(args);
    logQueue.tail = (logQueue.tail + 1) % LOG_QUEUE_SIZE;

    // Signal a possibly waiting logging thread
    pthread_cond_signal(&logQueue.cond);
    // Unlock the mutex
    pthread_mutex_unlock(&logQueue.lock);
}

/**
 * Pops a message from the log queue, writing it into console
 * and the log file. Just a utility.
 * The caller must hold the logQueue.lock mutex.
 *
 * @param logfile The File pointer to write into
 * @param logpath The current log file path, used for rotating files
 * @return FILE* The current log stream, replaced when rotation succeeds
 */
static FILE* pop_message(FILE* logfile, const char* logpath)
{
    // Remove a message from the queue
    // We don't empty the whole queue to avoid being a bottleneck for the
    // addition of new messages.
    // Log to console
    printf("%s", logQueue.message_queue[logQueue.head]);
    if (config.write_to_file.value.b) {
        if (!logfile) {
            logfile = fopen(logpath, "a");
            if (!logfile) {
                perror("Failed to open log file");
                goto pop_message_cleanup;
            }
        }

        // Log to file
        if (fprintf(logfile, "%s", logQueue.message_queue[logQueue.head]) < 0) {
            perror("Failed to write to log file");
        }

        // Flush the file immediately to disk, in case something crashes
        if (fflush(logfile) != 0) {
            perror("Failed to flush the log file");
        }

        logfile = rotate_logs(logfile, logpath);
    } else if (logfile) {
        fclose(logfile);
        logfile = NULL;
    }

pop_message_cleanup:
    logQueue.head = (logQueue.head + 1) % LOG_QUEUE_SIZE;
    pthread_cond_broadcast(&logQueue.cond);
    return logfile;
}

/**
 * The logging thread, writes the queued messages in the log.
 *
 * Works as a consumer
 *
 * @param arg Unused.
 */
void* loggingThread(void* arg)
{
    prctl(PR_SET_NAME, "LS Logger", 0, 0, 0);
    char data_path[PATH_MAX];
    get_libresplit_data_folder_path(data_path);
    strcat(data_path, "/logs/");
    strcat(data_path, log_filename);
    FILE* logfile = NULL;

    pthread_mutex_lock(&logQueue.lock);
    if (config.write_to_file.value.b) {
        logfile = fopen(data_path, "a");
        if (!logfile) {
            perror("Failed to open log file");
        }
    }

    pthread_mutex_unlock(&logQueue.lock);

    while (atomic_load(&logging_active)) {
        // Lock the mutex for reading
        pthread_mutex_lock(&logQueue.lock);
        // If the queue is empty, wait
        while (logQueue.head == logQueue.tail && atomic_load(&logging_active)) {
            pthread_cond_wait(&logQueue.cond, &logQueue.lock);
        }

        // We got signalled by the main thread to close up
        if (!atomic_load(&logging_active)) {
            pthread_mutex_unlock(&logQueue.lock);
            break;
        }

        logfile = pop_message(logfile, data_path);
        // Unlock the mutex
        pthread_mutex_unlock(&logQueue.lock);
    }

    // We're closing the logger, empty the remaining logs...
    pthread_mutex_lock(&logQueue.lock);
    while (logQueue.head != logQueue.tail) {
        logfile = pop_message(logfile, data_path);
    }

    pthread_mutex_unlock(&logQueue.lock);

    if (logfile) {
        // ... and close the logfile
        fclose(logfile);
        logfile = NULL;
    }

    return 0;
}

/**
 * Function to close the logger thread.
 *
 * This is needed because while we're closing, we might be waiting for
 * the queue to fill up. If that's the case, we signal the thread to continue
 * after setting logging_active to false.
 */
void close_logger(void)
{
    LOG_DEBUG("Shutting down logger thread...");
    pthread_mutex_lock(&logQueue.lock);
    atomic_store(&logging_active, 0);
    // Wakeup the logging thread and anything waiting for queue space.
    pthread_cond_broadcast(&logQueue.cond);
    pthread_mutex_unlock(&logQueue.lock);
}
