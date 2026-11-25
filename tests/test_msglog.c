#include "test.h"

#include <string.h>

#include "core/msglog.h"

static MsgLog log_; /* ~10 KB: static, not on the stack */

static void newest_first_and_repeats_merge(void)
{
    msglog_clear(&log_);
    CHECK(msglog_len(&log_) == 0 && msglog_get(&log_, 0) == NULL);

    msglog_add(&log_, 1, "Hello %d", 7);
    msglog_add(&log_, 2, "Bump");
    msglog_add(&log_, 3, "Bump");
    CHECK(msglog_len(&log_) == 2);
    CHECK(strcmp(msglog_get(&log_, 0)->text, "Bump") == 0);
    CHECK(msglog_get(&log_, 0)->count == 2 && msglog_get(&log_, 0)->turn == 3);
    CHECK(strcmp(msglog_get(&log_, 1)->text, "Hello 7") == 0);
    CHECK(msglog_get(&log_, 2) == NULL && msglog_get(&log_, -1) == NULL);

    /* A repeat counts as new: its serial moves past the older message's. */
    CHECK(msglog_get(&log_, 0)->serial == 2 && msglog_next_serial(&log_) == 3);
}

static void full_log_drops_the_oldest(void)
{
    msglog_clear(&log_);
    for (int i = 0; i < MSGLOG_CAPACITY + 5; i++)
        msglog_add(&log_, i, "m%d", i);
    CHECK(msglog_len(&log_) == MSGLOG_CAPACITY);
    CHECK(strcmp(msglog_get(&log_, 0)->text, "m104") == 0);
    CHECK(strcmp(msglog_get(&log_, MSGLOG_CAPACITY - 1)->text, "m5") == 0);
}

static void long_messages_are_cut(void)
{
    char longer[300];
    memset(longer, 'x', sizeof longer - 1);
    longer[sizeof longer - 1] = '\0';
    msglog_clear(&log_);
    msglog_add(&log_, 0, "%s", longer);
    CHECK(strlen(msglog_get(&log_, 0)->text) == MSG_TEXT_SIZE - 1);
}

void run_msglog_tests(void)
{
    RUN(newest_first_and_repeats_merge);
    RUN(full_log_drops_the_oldest);
    RUN(long_messages_are_cut);
}
