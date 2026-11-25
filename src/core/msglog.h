/* Message log: what happened, newest last. A fixed ring buffer: when full,
 * the oldest message is dropped. */
#ifndef ROGUEC_MSGLOG_H
#define ROGUEC_MSGLOG_H

#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_LIKE(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define PRINTF_LIKE(fmt, args)
#endif

enum
{
    MSGLOG_CAPACITY = 100,
    MSG_TEXT_SIZE = 96, /* longer messages are cut */
};

typedef struct
{
    char text[MSG_TEXT_SIZE];
    int turn;   /* turn of the latest repeat */
    int count;  /* times in a row: "(x3)" */
    int serial; /* order of the latest add or repeat, counting from 0 */
} Message;

typedef struct
{
    Message msgs[MSGLOG_CAPACITY];
    int first; /* index of the oldest */
    int len;
    int next_serial;
} MsgLog;

void msglog_clear(MsgLog *log);
/* Adds a printf-style message. The same text as the newest message only
 * bumps its count and turn. */
void msglog_add(MsgLog *log, int turn, const char *fmt, ...) PRINTF_LIKE(3, 4);
int msglog_len(const MsgLog *log);
/* Serial the next add or repeat will get: messages at or after it are newer. */
int msglog_next_serial(const MsgLog *log);
/* age 0 is the newest message; NULL if age is out of range. */
const Message *msglog_get(const MsgLog *log, int age);

#endif
