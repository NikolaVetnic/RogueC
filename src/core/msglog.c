#include "core/msglog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void msglog_clear(MsgLog *log)
{
    log->first = 0;
    log->len = 0;
    log->next_serial = 0;
}

void msglog_add(MsgLog *log, int turn, const char *fmt, ...)
{
    char text[MSG_TEXT_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof text, fmt, args);
    va_end(args);

    Message *newest =
        log->len > 0 ? &log->msgs[(log->first + log->len - 1) % MSGLOG_CAPACITY] : NULL;
    if (newest && strcmp(newest->text, text) == 0)
    {
        newest->count++;
        newest->turn = turn;
        newest->serial = log->next_serial++;
        return;
    }

    if (log->len < MSGLOG_CAPACITY)
        log->len++;
    else
        log->first = (log->first + 1) % MSGLOG_CAPACITY;

    Message *m = &log->msgs[(log->first + log->len - 1) % MSGLOG_CAPACITY];
    memcpy(m->text, text, sizeof text);
    m->turn = turn;
    m->count = 1;
    m->serial = log->next_serial++;
}

int msglog_len(const MsgLog *log)
{
    return log->len;
}

int msglog_next_serial(const MsgLog *log)
{
    return log->next_serial;
}

const Message *msglog_get(const MsgLog *log, int age)
{
    if (age < 0 || age >= log->len)
        return NULL;
    return &log->msgs[(log->first + log->len - 1 - age) % MSGLOG_CAPACITY];
}
