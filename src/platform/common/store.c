#include "platform/common/store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "core/save.h"

enum
{
    SCORES_TEXT_SIZE = SCORES_MAX * 128,
};

/* ---- text by name ------------------------------------------------------------ */

#ifdef __EMSCRIPTEN__

/* localStorage "roguec_<name>". Stored text never holds quotes, backslashes
 * or '|' (scores, options and base64 saves don't), so it goes into the script
 * as is, with lines joined by '|'. */

/* malloc'd text, or NULL if there is none. */
static char *load_text(const char *name)
{
    char script[160];
    snprintf(script, sizeof script,
             "(function(){try{var t=localStorage.getItem('roguec_%s');"
             "return t===null?'\\x01':t.split('|').join('\\n');}catch(e){return '\\x01';}})()",
             name);
    const char *text = emscripten_run_script_string(script);
    if (!text || strcmp(text, "\x01") == 0)
        return NULL;
    char *copy = malloc(strlen(text) + 1);
    if (copy)
        strcpy(copy, text);
    return copy;
}

static void save_text(const char *name, const char *text)
{
    size_t size = strlen(text) + 128;
    char *script = malloc(size);
    if (!script)
        return;
    int n = snprintf(script, size, "try{localStorage.setItem('roguec_%s','", name);
    for (const char *c = text; *c; c++)
        script[n++] = *c == '\n' ? '|' : *c;
    snprintf(script + n, size - (size_t)n, "');}catch(e){}");
    emscripten_run_script(script);
    free(script);
}

static void delete_text(const char *name)
{
    char script[96];
    snprintf(script, sizeof script, "try{localStorage.removeItem('roguec_%s');}catch(e){}", name);
    emscripten_run_script(script);
}

#else

/* $HOME/.roguec_<name>; false if there is no home directory. */
static bool path(const char *name, const char *suffix, char *out, size_t size)
{
    const char *home = getenv("HOME");
    if (!home || !*home)
        return false;
    int n = snprintf(out, size, "%s/.roguec_%s%s", home, name, suffix);
    return n > 0 && (size_t)n < size;
}

static char *load_text(const char *name)
{
    char file[512];
    if (!path(name, "", file, sizeof file))
        return NULL;
    FILE *f = fopen(file, "rb");
    if (!f)
        return NULL;
    char *text = NULL;
    size_t len = 0, cap = 0;
    for (;;)
    {
        if (len + 4096 + 1 > cap)
        {
            cap = cap ? cap * 2 : 8192;
            char *grown = realloc(text, cap);
            if (!grown)
            {
                free(text);
                fclose(f);
                return NULL;
            }
            text = grown;
        }
        size_t n = fread(text + len, 1, 4096, f);
        len += n;
        if (n < 4096)
            break;
    }
    fclose(f);
    text[len] = '\0';
    return text;
}

/* Writes a new file and renames it over the old one, so a crash mid-write
 * leaves the old save, not half of the new one. */
static void save_text(const char *name, const char *text)
{
    char file[512], temp[520];
    if (!path(name, "", file, sizeof file) || !path(name, ".new", temp, sizeof temp))
        return;
    FILE *f = fopen(temp, "wb");
    if (!f)
        return;
    bool ok = fputs(text, f) >= 0;
    ok = fclose(f) == 0 && ok;
    if (!ok || rename(temp, file) != 0)
        remove(temp);
}

static void delete_text(const char *name)
{
    char file[512];
    if (path(name, "", file, sizeof file))
        remove(file);
}

#endif

/* ---- what is stored ----------------------------------------------------------- */

void store_load_scores(ScoreTable *t)
{
    char *text = load_text("scores");
    scores_parse(t, text);
    free(text);
}

void store_save_scores(const ScoreTable *t)
{
    char text[SCORES_TEXT_SIZE];
    if (scores_format(t, text, sizeof text))
        save_text("scores", text);
}

Settings store_load_settings(void)
{
    char *text = load_text("options");
    Settings s = settings_parse(text);
    free(text);
    return s;
}

void store_save_settings(const Settings *s)
{
    char text[SETTINGS_TEXT_SIZE];
    if (settings_format(s, text, sizeof text))
        save_text("options", text);
}

void store_save_game(const Game *g)
{
    char *text = game_save(g);
    if (text)
        save_text("save", text);
    free(text);
}

void store_delete_game(void)
{
    delete_text("save");
}

bool store_start_game(Game *g, bool resume, uint64_t seed, const Settings *s)
{
    char *text = resume ? load_text("save") : NULL;
    bool had_save = text != NULL, loaded = text && game_load(g, text);
    free(text);
    if (loaded)
        return true;
    if (!game_init(g, seed))
        return false;
    if (had_save)
        msglog_add(&g->log, 0,
                   "Your saved game couldn't be loaded (damaged, or from another "
                   "version). A new one begins.");
    if (s->realtime_start)
        game_set_realtime(g, true);
    return true;
}
