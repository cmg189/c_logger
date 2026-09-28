/**man

    MODULE
        example.c - Demonstration of the logging utility

    AUTHOR
        cmg189

    DESCRIPTION
        This module contains an exmaple usage of the logging utility

endman**/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

int main(int argc, char **argv)
{
    const char *log_path = (argc > 1) ? argv[1] : "example.log";
    FILE *fp = NULL;

    if( log_init(log_path, LOG_LEVEL_DEBUG) )
    {
        fprintf(stderr, "log_init(%s) failed: %s\n", log_path, strerror(errno));
        return EXIT_FAILURE;
    }

    LOG_INFO("starting up, logging to %s", log_path);

    LOG_DEBUG("this line is only visible while the threshold is DEBUG");

    // logging does not disturb errno, so the real failure is still reportable
    fp = fopen("/nonexistent/path", "r");
    if(!fp)
    {
        LOG_WARN("opening the optional config failed, continuing");
        LOG_ERROR("fopen failed: %s", strerror(errno));
    }

    // threshold can be changed at any point
    log_set_level(LOG_LEVEL_WARN);

    LOG_INFO("suppressed: the threshold is now WARN");  // INFO < WARN so this wont be output
    LOG_WARN("still emitted");
    LOG_INFO("shutting down");

    log_close();

    printf("wrote %s\n", log_path);

    return EXIT_SUCCESS;
}

