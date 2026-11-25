/* Player options, kept between games. No I/O: frontends store the text from
 * settings_format and hand it back to settings_parse. */
#ifndef ROGUEC_SETTINGS_H
#define ROGUEC_SETTINGS_H

#include <stdbool.h>

typedef enum
{
    SPEED_SLOW,
    SPEED_NORMAL,
    SPEED_FAST,
    SPEED_COUNT,
} Speed;

typedef enum
{
    COLOURS_AUTO, /* what the terminal reports */
    COLOURS_FULL, /* 256 colours */
    COLOURS_8,
    COLOURS_MONO,
    COLOURS_COUNT,
} Colours;

enum
{
    LOG_LINES_MIN = 2,
    LOG_LINES_MAX = 4,
    SETTINGS_TEXT_SIZE = 128, /* enough for settings_format */
};

typedef struct
{
    bool realtime_start; /* new games start in real time */
    Speed speed;         /* real time: length of a tick */
    int log_lines;       /* messages above the map, LOG_LINES_MIN..MAX */
    Colours colours;     /* terminal only */
} Settings;

Settings settings_default(void);
/* Real time: milliseconds per tick (one normal-speed move). */
int settings_tick_ms(const Settings *s);
/* "key=value" lines. Unknown keys and bad values keep their defaults. */
Settings settings_parse(const char *text);
/* Writes them in that form; returns false if `size` is too small. */
bool settings_format(const Settings *s, char *out, int size);

#endif
