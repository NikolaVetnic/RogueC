#include "core/settings.h"

#include <stdio.h>
#include <string.h>

static const int TICK_MS[SPEED_COUNT] = {450, 300, 200};

Settings settings_default(void)
{
    return (Settings){.speed = SPEED_NORMAL, .log_lines = LOG_LINES_MIN, .colours = COLOURS_AUTO};
}

int settings_tick_ms(const Settings *s)
{
    return TICK_MS[s->speed >= 0 && s->speed < SPEED_COUNT ? s->speed : SPEED_NORMAL];
}

/* Sets *field to value if it lies in [lo, hi]. */
static void set_int(int *field, int value, int lo, int hi)
{
    if (value >= lo && value <= hi)
        *field = value;
}

Settings settings_parse(const char *text)
{
    Settings s = settings_default();
    while (text && *text)
    {
        const char *end = strchr(text, '\n');
        size_t len = end ? (size_t)(end - text) : strlen(text);
        char line[64], key[32];
        int value;
        if (len < sizeof line)
        {
            memcpy(line, text, len);
            line[len] = '\0';
            if (sscanf(line, "%31[a-z_]=%d", key, &value) == 2)
            {
                int realtime = s.realtime_start, speed = (int)s.speed, colours = (int)s.colours;
                if (strcmp(key, "realtime_start") == 0)
                    set_int(&realtime, value, 0, 1);
                else if (strcmp(key, "speed") == 0)
                    set_int(&speed, value, 0, SPEED_COUNT - 1);
                else if (strcmp(key, "log_lines") == 0)
                    set_int(&s.log_lines, value, LOG_LINES_MIN, LOG_LINES_MAX);
                else if (strcmp(key, "colours") == 0)
                    set_int(&colours, value, 0, COLOURS_COUNT - 1);
                s.realtime_start = realtime != 0;
                s.speed = (Speed)speed;
                s.colours = (Colours)colours;
            }
        }
        text = end ? end + 1 : text + len;
    }
    return s;
}

bool settings_format(const Settings *s, char *out, int size)
{
    int n = snprintf(out, (size_t)size, "realtime_start=%d\nspeed=%d\nlog_lines=%d\ncolours=%d\n",
                     s->realtime_start ? 1 : 0, (int)s->speed, s->log_lines, (int)s->colours);
    return n >= 0 && n < size;
}
