/**man

    MODULE
        log.c - Implementation of the file-backed logger

    AUTHOR
        cmg189

    DESCRIPTION
        This module contains the implementation of the logging functions declared in log.h

endman**/


#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "log.h"


// Room for "YYYY-MM-DD HH:MM:SS.mmm" plus terminator
#define TIMESTAMP_LEN 32


static FILE            *log_fp    = NULL;
static log_level_e      log_level = LOG_LEVEL_INFO;
static pthread_mutex_t  log_mutex = PTHREAD_MUTEX_INITIALIZER;


// Level names are padded to five characters so the message column lines up regardless of severity
static const char *const level_names[] =
{
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_WARN]  = "WARN ",
    [LOG_LEVEL_INFO]  = "INFO ",
    [LOG_LEVEL_DEBUG] = "DEBUG"
};

// Formats the current wall-clock time with millisecond resolution
static void timestamp_now(char *buf, size_t buf_sz)
{
    char time_buff[20] = { 0 };
    struct timespec ts;
    struct tm tm_buf;
    long ms = 0;

    // initalize time structs and time_buff
    if( clock_gettime(CLOCK_REALTIME, &ts) || !localtime_r(&ts.tv_sec, &tm_buf) || !strftime(time_buff, sizeof(time_buff), "%Y-%m-%d %H:%M:%S", &tm_buf) )
    {
        snprintf(buf, buf_sz, "(no timestamp)");
        return;
    }

    // tv_nsec is a long, so the compiler cannot prove the millisecond field is three digits
    // clamping makes the bound explicit rather than relying on the kernel to honour its own contract
    ms = ts.tv_nsec / 1000000L;
    if(ms < 0)
    {
        ms = 0;
    }

    if(ms > 999)
    {
        ms = 999;
    }

    snprintf(buf, buf_sz, "%s.%03d", time_buff, (int)ms);

    return;
}

// Trims a __FILE__ path down to its basename
static const char *basename_of(const char *path)
{
    if(!path)
    {
        return "?";
    }

    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}


// Opens the given path in append mode and sets the severity level
int log_init(const char *path, log_level_e level)
{
    FILE *fp = NULL;

    if(!path)
    {
        errno = EINVAL;
        return -1;
    }

    fp = fopen(path, "a");  // open file
    if(!fp)
    {
        return -1;  // errno set by fopen
    }

    // Line buffering: a crash loses at most a partial final line, but avoids
    // the syscall-per-fragment cost of unbuffered output
    setvbuf(fp, NULL, _IOLBF, 0);

    pthread_mutex_lock(&log_mutex);

    if(log_fp)
    {
        fclose(log_fp);
    }
    log_fp = fp;
    log_level = level;
    pthread_mutex_unlock(&log_mutex);

    return 0;
}

// Flush and close the log file
void log_close(void)
{
    pthread_mutex_lock(&log_mutex);

    if(log_fp)
    {
        fflush(log_fp);
        fclose(log_fp);
        log_fp = NULL;
    }

    pthread_mutex_unlock(&log_mutex);

    return;
}

// Sets the severity level used to filter subsequent log calls
void log_set_level(log_level_e level)
{
    pthread_mutex_lock(&log_mutex);

    log_level = level;

    pthread_mutex_unlock(&log_mutex);

    return;
}

// Returns the severity level currently in effect
log_level_e log_get_level(void)
{
    pthread_mutex_lock(&log_mutex);

    log_level_e level = log_level;

    pthread_mutex_unlock(&log_mutex);

    return level;
}

// Returns the padded display name, or "UNKNOWN" if out of range
const char *log_level_to_string(log_level_e level)
{
    if(level < LOG_LEVEL_ERROR || level > LOG_LEVEL_DEBUG)
    {
        return "UNKNOWN";
    }

    return level_names[level];
}

// Parses a level name case-insensitive
int log_level_from_string(const char *name, log_level_e *level_out)
{
    static const struct
    {
        const char *name;
        log_level_e level;
    } table[] = {
        { "error", LOG_LEVEL_ERROR },
        { "warn",  LOG_LEVEL_WARN  },
        { "info",  LOG_LEVEL_INFO  },
        { "debug", LOG_LEVEL_DEBUG }
    };

    if(!name || !level_out)
    {
        return -1;
    }

    for(size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
    {
        if( !strcasecmp(name, table[i].name) )
        {
            *level_out = table[i].level;
            return 0;
        }
    }

    return -1;
}

// Formats and writes a single log line
void log_write(log_level_e level, const char *file, int line, const char *func, const char *fmt, ...)
{
    const int saved_errno = errno;  // save and restore errno because fprintf may modify errno even on success
    FILE *out = log_fp ? log_fp : stderr;  // Fall back to stderr so messages logged before log_init are not lost
    va_list ap;  // cursor over the unnamed arguments
    char stamp[TIMESTAMP_LEN] = { 0 };

    // check for error
    if(level > log_level)
    {
        errno = saved_errno;
        return;
    }

    pthread_mutex_lock(&log_mutex);  // lock the given mutex

    // format the log line
    timestamp_now(stamp, sizeof(stamp));
    fprintf(out, "%s [%s] %s:%d %s: ", stamp, log_level_to_string(level), basename_of(file), line, func ? func : "?");

    // position cursor past the last named parameter 'fmt', hand the whole pack to printf and release whatever va_start set up
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);

    fputc('\n', out);

    // errors are flushed immediately so that in the event of a crash the log can be written to disk
    if(level == LOG_LEVEL_ERROR)
    {
        fflush(out);
    }

    pthread_mutex_unlock(&log_mutex);  //  unlock the given mutex

    errno = saved_errno;

    return;
}

