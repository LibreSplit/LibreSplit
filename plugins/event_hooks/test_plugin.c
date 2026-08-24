#include "plugins/plugin.h"
#include "plugins/plugin_utils.h"
#include "timer.h"
#include <stdio.h>

const abi_version_t abi_version = 0 << 16 | 1; // v0.1
const char plugin_name[] = "Test Plugin 2";
const char plugin_description[] = "Does something, it exists and yells at you for splitting";
const char plugin_version[] = "0.1";
const char plugin_author[] = "The LibreSplit Core Team";

int do_something_split(const ls_timer* timer)
{
    printf("Message from a plugin: you have successfully split!\n");
    fflush(stdout);
    fflush(stderr);
    return 0;
}

int do_something_stop(const ls_timer* timer)
{
    printf("Message from a plugin: why have you stopped?\n");
    fflush(stdout);
    fflush(stderr);
    return 0;
}

int do_something_start(const ls_timer* timer)
{
    printf("Message from a plugin: Here we go!\n");
    fflush(stdout);
    fflush(stderr);
    return 0;
}

int do_something_reset(const ls_timer* timer)
{
    printf("Message from a plugin: No reset run next time?\n");
    fflush(stdout);
    fflush(stderr);
    return 0;
}

int do_something_cancel(const ls_timer* timer)
{
    printf("Message from a plugin: We all make mistakes. Run cancelled.\n");
    fflush(stdout);
    fflush(stderr);
    return 0;
}

int do_something_skip(const ls_timer* timer)
{
    printf("Message from a plugin: skipping splits now?\n");
    fflush(stdout);
    fflush(stderr);
    return 0;
}

int do_something_unsplit(const ls_timer* timer)
{
    printf("Message from a plugin: We all make mistakes. Unsplit is there for you.\n");
    fflush(stdout);
    fflush(stderr);
    return 0;
}

int do_something_pause(const ls_timer* timer)
{
    printf("Message from a plugin: Stop! Hammertime.\n");
    fflush(stdout);
    fflush(stderr);
    return 0;
}

int do_something_unpause(const ls_timer* timer)
{
    printf("Message from a plugin: Enough hammers. Let's resume!\n");
    fflush(stdout);
    fflush(stderr);
    return 0;
}

int plug_init(PlugAPI* api)
{
    api->register_event_hook(START, do_something_start);
    api->register_event_hook(STOP, do_something_stop);
    api->register_event_hook(SPLIT, do_something_split);
    api->register_event_hook(RESET, do_something_reset);
    api->register_event_hook(CANCEL, do_something_cancel);
    api->register_event_hook(SKIP, do_something_skip);
    api->register_event_hook(UNSPLIT, do_something_unsplit);
    api->register_event_hook(PAUSE, do_something_pause);
    api->register_event_hook(UNPAUSE, do_something_unpause);
    return 0;
}
