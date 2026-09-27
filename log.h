/**man

    MODULE
        log.h - File-backed logging utility for C

    AUTHOR
        cmg189

    DESCRIPTION
        This module contains the typedefs, macros and function declarations for the
        file-backed logger

endman**/


#ifndef LOG_H
#define LOG_H

#include <stdio.h>


// Severity levels for logging output
typedef enum log_level
{
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_INFO  = 2,
    LOG_LEVEL_DEBUG = 3
} log_level_e;


// Opens the given path in append mode and sets the severity level
// Returns 0 on success, -1 otherwise with errno set
int log_init(const char *path, log_level_e level);


// Flush and close the log file
void log_close(void);


// Sets the severity level used to filter subsequent log calls
void log_set_level(log_level_e level);


// Returns the severity level currently in effect
log_level_e log_get_level(void);

// Parses a level name case-insensitive
// Returns 0 on success, -1 otherwise
int log_level_from_string(const char *name, log_level_e *level_out);


// Returns the padded display name for a severity level or "UNKNOWN" if out of range
const char *log_level_to_string(log_level_e level);


// Formats and writes a single log line
// It is the implementation behind the LOG_* macros and is not normally called directly
void log_write(log_level_e level, const char *file, int line, const char *func, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 5, 6)))
#endif
    ;


// Emit a log line at an explicit severity
// This macro is the shared body of the LOG_ERROR, LOG_WARN, LOG_INFO and LOG_DEBUG macros
#define LOG_AT(level, fmt, ...) \
    do { \
        log_write((level), __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
    } while (0)


// These macros emit a log line at their respective severity level tagged with
// the file, line and function they were called from
#define LOG_ERROR(fmt, ...) LOG_AT(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG_AT(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG_AT(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)


// This macro emits a log line at DEBUG severity in a normal build
// Under NDEBUG it compiles to nothing, but the call is placed inside if(0) rather than removed outright
#ifdef NDEBUG
#  define LOG_DEBUG(fmt, ...) \
      do { \
          if (0) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, \
                           fmt, ##__VA_ARGS__); \
      } while (0)
#else
#  define LOG_DEBUG(fmt, ...) LOG_AT(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#endif

#endif /* LOG_H */

