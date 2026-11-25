#include "platform/common/score_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

enum
{
    TEXT_SIZE = SCORES_MAX * 128,
};

#ifdef __EMSCRIPTEN__

/* localStorage holds one string; lines are joined with '|' (score text never
 * contains '|' or quotes: score.c keeps it to letters, digits and a few marks). */
void score_store_load(ScoreTable *t)
{
    const char *text = emscripten_run_script_string(
        "(function(){try{return "
        "(localStorage.getItem('roguec_scores')||'').split('|').join('\\n');}"
        "catch(e){return '';}})()");
    scores_parse(t, text);
}

void score_store_save(const ScoreTable *t)
{
    char text[TEXT_SIZE];
    char script[TEXT_SIZE + 128];
    if (!scores_format(t, text, sizeof text))
        return;
    for (char *c = text; *c; c++)
        if (*c == '\n')
            *c = '|';
    snprintf(script, sizeof script, "try{localStorage.setItem('roguec_scores','%s');}catch(e){}",
             text);
    emscripten_run_script(script);
}

#else

/* $HOME/.roguec_scores; false if there is no home directory. */
static bool path(char *out, size_t size)
{
    const char *home = getenv("HOME");
    if (!home || !*home)
        return false;
    return snprintf(out, size, "%s/.roguec_scores", home) < (int)size;
}

void score_store_load(ScoreTable *t)
{
    char file[512], text[TEXT_SIZE];
    *t = (ScoreTable){0};
    if (!path(file, sizeof file))
        return;
    FILE *f = fopen(file, "r");
    if (!f)
        return;
    size_t n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[n] = '\0';
    scores_parse(t, text);
}

void score_store_save(const ScoreTable *t)
{
    char file[512], text[TEXT_SIZE];
    if (!path(file, sizeof file) || !scores_format(t, text, sizeof text))
        return;
    FILE *f = fopen(file, "w");
    if (!f)
        return;
    fputs(text, f);
    fclose(f);
}

#endif
