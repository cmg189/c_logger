# c-logger

Logging utility for C programs. Writes timestamped,
level-tagged lines to a file, tagged with the source location they came from.

Around 250 lines of implementation. Written to be read in a couple of minutes.

## Quick start

Requires a C11 compiler and pthreads. Nothing else to install.

Execute the following commands in the directory where the repository has been cloned

```sh
make example     # builds ./example
./example        # writes example.log in the current directory
cat example.log  # outputs example.log
```

Example output from log file

```
2026-09-17 14:09:04.971 [INFO ] example.c:24 main: starting up, logging to example.log
2026-09-17 14:09:04.971 [DEBUG] example.c:25 main: this line is only visible while the threshold is DEBUG
2026-09-17 14:09:04.971 [WARN ] example.c:31 main: opening the optional config failed, continuing
2026-09-17 14:09:04.971 [ERROR] example.c:32 main: fopen failed: No such file or directory
2026-09-17 14:09:04.971 [WARN ] example.c:38 main: still emitted
```

## Make targets

```sh
make          # build liblog.a
make example  # build the example program
make run      # build and run the example
make check    # run the example under Address/UB sanitizers
make strict   # build with -Werror
make clean    # remove build artifacts and example.log
```

`make strict` adds nine warning flags beyond `-Wall -Wextra` and treats every
warning as an error.


## Using it in your own code

Copy `log.c` and `log.h` into your project, or build `liblog.a` with `make` and
link against it. Compile with `-pthread`.

```c
#include "log.h"

int main(void)
{
    if( log_init("app.log", LOG_LEVEL_INFO) )
    {
        fprintf(stderr, "log_init failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    LOG_INFO("started with %d workers", worker_count);
    LOG_WARN("config not found, using defaults");
    LOG_ERROR("connect failed: %s", strerror(errno));
    LOG_DEBUG("queue depth now %zu", depth);

    log_set_level(LOG_LEVEL_DEBUG);  // threshold is changeable at runtime

    log_close();
}
```

Four levels — `ERROR`, `WARN`, `INFO`, `DEBUG` — filtered against a threshold
set at init or changed later. `log_level_from_string()` parses a level name, so
the threshold can be driven from a config file or environment variable.


## Log levels

Four levels, ordered from most to least severe

Every call is compared against the current threshold. Setting it to a level
emits that level and everything more severe, discarding the rest:

| Threshold | `ERROR` | `WARN` | `INFO` | `DEBUG` |
|---|---|---|---|---|
| `LOG_LEVEL_ERROR` | ✓ | | | |
| `LOG_LEVEL_WARN` | ✓ | ✓ | | |
| `LOG_LEVEL_INFO` | ✓ | ✓ | ✓ | |
| `LOG_LEVEL_DEBUG` | ✓ | ✓ | ✓ | ✓ |

A suppressed call costs a mutex acquire and a comparison — the format string is
never expanded and its arguments are never converted, so leaving `LOG_DEBUG`
calls in place is cheap.

Set the threshold at init:

```c
log_init("app.log", LOG_LEVEL_INFO);  // DEBUG suppressed
```

Or change it at any point, including before `log_init`:

```c
log_set_level(LOG_LEVEL_DEBUG);        // turn DEBUG on mid-run
log_level_e current = log_get_level();
```

### Driving the threshold from config

`log_level_from_string()` parses a level name case-insensitively, so the
threshold can come from a config file, a command-line flag, or the environment
without writing your own mapping:

```c
log_level_e level = LOG_LEVEL_INFO;  // default if unset
const char *env = getenv("APP_LOG_LEVEL");

if( env && log_level_from_string(env, &level) )
{
    fprintf(stderr, "unknown log level '%s', using info\n", env);
}

log_init("app.log", level);
```

It returns `-1` for an unrecognized name and leaves `level` untouched, so a typo
in a config file falls back to your default instead of silently disabling
logging. Accepted names are `"error"`, `"warn"`, `"info"` and `"debug"`.

## Design notes

The parts that aren't obvious, and why they are the way they are.

**Macros, not a variadic function.** `LOG_INFO(fmt, ...)` folds the caller's
format into a literal inside the macro, which keeps `-Wformat` checking alive at
every call site and makes a format-string vulnerability impossible to write by
accident. A `log_info(const char *fmt, ...)` function loses both.

**`do { } while (0)`.** The macros expand to a single statement, so they behave
correctly inside a braceless `if` and the body can grow later without silently
breaking existing callers.

**`LOG_DEBUG` compiles out but stays checked.** Under `NDEBUG` the call is
wrapped in `if (0)`: the optimizer removes it entirely, but the compiler still
validates the arguments. A debug line whose format no longer matches its
arguments fails the build instead of rotting unnoticed until someone turns debug
logging back on.

**`errno` is saved and restored.** `fprintf` may modify `errno` even on success,
so logging between a failing syscall and its error handling would otherwise
corrupt the error being reported. This makes the common pattern safe:

```c
if( connect(fd, ...) < 0 )
{
    LOG_WARN("retrying");
    return report(errno);  // still the connect() error
}
```

**Thread-safe by default.** A logger that interleaves output under concurrency is
worse than no logger, because the corruption appears exactly when something is
going wrong. Calls are serialized internally, so a line written from one thread
is never split by another's.

**Falls back to stderr.** Before `log_init` — or if it failed — output goes to
stderr rather than being discarded. The messages most worth keeping are often the
ones explaining why startup did not get far enough to open a log file.

**Basenames, not `__FILE__` verbatim.** `__FILE__` expands to whatever path the
compiler was handed, so an out-of-tree build would otherwise stamp an absolute
build path onto every line.

**Line-buffered, with `ERROR` flushed immediately.** Line buffering avoids a
syscall per fragment; forcing a flush on `ERROR` means the last line before a
crash actually reaches disk.

`example.c` is about forty lines and exercises every feature: the four levels,
a runtime threshold change, and logging around a failing syscall without
disturbing `errno`. Pass a path to write somewhere else — `./example /tmp/my.log`.

Note that `make clean` removes `example.log` along with the build artifacts, so
if you go looking for output afterward and find nothing, that is why.

## Limitations

- No log rotation or size cap. Pair it with `logrotate` if that matters.
- One log file per process; there is no per-subsystem logger.
- Timestamps use `CLOCK_REALTIME`, so they can step backwards across an NTP
  correction. That is the right trade for correlating with other systems' logs,
  but it means the file is not strictly monotonic.
- POSIX only — `clock_gettime`, `localtime_r` and pthreads.

## License

MIT — see [LICENSE](LICENSE).
