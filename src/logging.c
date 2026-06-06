/** \file logging.c
 * Logger Rollster
 *
 * Asynchronous Logging Library for LibreSplit based on threads, circular queues,
 * hopes and dreams.
 */
#include "logging.h"
#include "settings/utils.h"

#include <linux/limits.h>
#include <linux/prctl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

/*! The log queue, used as buffer */
static LogQueue logQueue;
/*! Atomic bool used to keep the thread active, might be used for clean closing in future */
static atomic_bool logging_active;

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
    while ((logQueue.tail + 1) % LOG_QUEUE_SIZE == logQueue.head) {
        pthread_cond_wait(&logQueue.cond, &logQueue.lock);
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
    vsnprintf(logQueue.message_queue[logQueue.tail] + strlen(timestamp) + 1, LOG_STR_LEN - sizeof(timestamp) - 1, fmt, args);
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
 *
 * @param logfile The File pointer to write into
 */
static void pop_message(FILE* logfile)
{
    // Remove a message from the queue
    // We don't empty the whole queue to avoid being a bottleneck for the
    // addition of new messages.
    // Log to console
    printf("%s", logQueue.message_queue[logQueue.head]);
    // Log to file
    fprintf(logfile, "%s", logQueue.message_queue[logQueue.head]);
    // Flush the file immediately to disk, in case something crashes
    fflush(logfile);
    logQueue.head = (logQueue.head + 1) % LOG_QUEUE_SIZE;
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
    strcat(data_path, "/libresplit.log");
    FILE* logfile = fopen(data_path, "a");
    if (!logfile) {
        perror("Failed to open log file");
        return NULL;
    }
    while (atomic_load(&logging_active)) {
        // Lock the mutex for reading
        pthread_mutex_lock(&logQueue.lock);
        // If the queue is empty, wait
        while (logQueue.head == logQueue.tail) {
            pthread_cond_wait(&logQueue.cond, &logQueue.lock);
            // We got signalled by the main thread to close up
            if (!atomic_load(&logging_active)) {
                break;
            }
        }
        pop_message(logfile);
        // Unlock the mutex
        pthread_mutex_unlock(&logQueue.lock);
    }
    // We're closing the logger, empty the remaining logs...
    while (logQueue.head != logQueue.tail) {
        pop_message(logfile);
    }
    // ... and close the logfile
    fclose(logfile);
    return 0;
}

/**
 * Function to close the logger thread.
 *
 * This is needed because while we're closing, we might be waiting for
 * the queue to fill up. If that's the case, we signal the thread to continue
 * after setting logging_active to false.
 */
void close_logger()
{
    atomic_store(&logging_active, 0);
    LOG_DEBUG("Shutting down logger thread...")
    // Signal the logging thread to continue, so to hit the closing condition,
    // in case it is waiting for the log queue to fill. If it isn't, the signal
    // should be ignored.
    pthread_cond_signal(&logQueue.cond);
}
