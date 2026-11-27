/* Desktop and browser frontend: raylib window, keyboard, mouse and touch input,
 * cell-grid drawing.
 * The same file builds natively and with Emscripten (__EMSCRIPTEN__). */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <raylib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#include "core/game.h"
#include "core/render.h"
#include "core/ui.h"
#include "platform/common/store.h"

#define FONT_FILE "assets/fonts/JetBrainsMono-Regular.ttf"
/* raylib's font size is the line height (ascent + descent), in logical pixels;
 * it is also the cell height. Letters are about 3/4 of it. */
#define FONT_SIZE 22
#define START_COLS 100
#define START_ROWS 45 /* messages + map height + status line */

typedef struct
{
    Game game;
    Ui ui;
    ScoreTable scores;
    Settings settings;
    bool use_store; /* false for --screenshot: leave saves and scores alone */
    Font font;
    float scale;     /* drawing units per logical pixel: devicePixelRatio on the web, else 1 */
    float font_size; /* FONT_SIZE * scale */
    int cell_w;
    int cell_h;
    Cell *cells;
    int cols;
    int rows;
    bool quit;
    bool touch;          /* a touch screen: the ui shows buttons */
    bool finger_down;    /* web: one finger on the screen since touch_start */
    Vector2 touch_start; /* ... where it landed, in canvas pixels */
    bool tap_pending;    /* web: a tap at `tap` for the next frame */
    Vector2 tap;
    double touch_ms; /* web: time of the last touch event */
} App;

/* Command-line arguments (desktop only). */
typedef struct
{
    uint64_t seed;
    bool seed_given;        /* a new game from this seed, not the saved one */
    const char *screenshot; /* save one frame here and exit */
    const char *keys;       /* typed before the screenshot */
} Args;

/* ---- input ---------------------------------------------------------------- */

/* Keys that type no character, as ui keys. Letters, digits and the numpad
 * arrive as characters (GetCharPressed), including the system's key repeat. */
static const struct
{
    int key;
    int ui_key;
} SPECIAL_KEYS[] = {
    {KEY_UP, UI_KEY_UP},
    {KEY_DOWN, UI_KEY_DOWN},
    {KEY_LEFT, UI_KEY_LEFT},
    {KEY_RIGHT, UI_KEY_RIGHT},
    {KEY_HOME, UI_KEY_HOME},
    {KEY_END, UI_KEY_END},
    {KEY_PAGE_UP, UI_KEY_PAGE_UP},
    {KEY_PAGE_DOWN, UI_KEY_PAGE_DOWN},
    {KEY_ESCAPE, 27},
    {KEY_ENTER, '\n'},
    {KEY_KP_ENTER, '\n'},
};

static void press(App *app, int key)
{
    ui_key(&app->ui, &app->game, key);
    if (!app->game.running)
        app->quit = true;
}

/* The ui key for a raylib key; arrows and Home/End/PgUp/PgDn with Shift run. */
static int special_key(int key)
{
    for (size_t i = 0; i < sizeof SPECIAL_KEYS / sizeof SPECIAL_KEYS[0]; i++)
        if (SPECIAL_KEYS[i].key == key)
        {
            int ui_key = SPECIAL_KEYS[i].ui_key;
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            return shift && ui_key >= UI_KEY_UP && ui_key <= UI_KEY_PAGE_DOWN
                       ? ui_key | UI_KEY_SHIFT
                       : ui_key;
        }
    return 0;
}

/* The grid's top left corner: it is centred in the window. */
static Vector2 grid_origin(const App *app)
{
    return (Vector2){(float)((GetScreenWidth() - app->cols * app->cell_w) / 2),
                     (float)((GetScreenHeight() - app->rows * app->cell_h) / 2)};
}

/* A tap or click at pixel p: to the ui as a cell, if it is on the grid. */
static void tap_at(App *app, Vector2 p)
{
    Vector2 o = grid_origin(app);
    if (p.x < o.x || p.y < o.y)
        return;
    int x = (int)((p.x - o.x) / (float)app->cell_w), y = (int)((p.y - o.y) / (float)app->cell_h);
    if (x >= app->cols || y >= app->rows)
        return;
    ui_tap(&app->ui, &app->game, x, y);
    if (!app->game.running)
        app->quit = true;
}

static void handle_input(App *app)
{
    /* Mouse: raylib reports clicks in the same pixels it draws in. It reports
     * touches as clicks too; those come through on_touch instead. */
    bool from_touch = false;
#ifdef __EMSCRIPTEN__
    from_touch = emscripten_get_now() - app->touch_ms < 1000.0;
#endif
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !from_touch)
        tap_at(app, GetMousePosition());
    if (app->tap_pending) /* web touch: see on_touch */
    {
        app->tap_pending = false;
        tap_at(app, app->tap);
    }

    for (int ch = GetCharPressed(); ch != 0; ch = GetCharPressed())
        press(app, ch);

    /* Presses from raylib's queue: IsKeyPressed compares key states between
     * frames and misses a key pressed and released within one frame. */
    for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed())
        if (special_key(key))
            press(app, special_key(key));

    for (size_t i = 0; i < sizeof SPECIAL_KEYS / sizeof SPECIAL_KEYS[0]; i++)
        if (IsKeyPressedRepeat(SPECIAL_KEYS[i].key))
            press(app, special_key(SPECIAL_KEYS[i].key));
}

/* ---- drawing -------------------------------------------------------------- */

/* Loads printable ASCII at `pixel_size`; the wall glyph (full block) is drawn
 * as a rectangle. Falls back to raylib's built-in font. */
static Font load_font(int pixel_size)
{
    static int codepoints[95];
    int count = 0;
    for (int c = 32; c < 127; c++)
        codepoints[count++] = c;

    const char *path = FONT_FILE;
    if (!FileExists(path)) /* started from elsewhere: look relative to build/<mode>/ */
        path = TextFormat("%s../../%s", GetApplicationDirectory(), FONT_FILE);

    Font font = LoadFontEx(path, pixel_size, codepoints, count);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

/* (Re)loads the font for the current pixel density and derives the cell size.
 * Desktop: raylib draws in logical pixels and scales them by the display's DPI
 * factor, so the font is loaded at that density and drawn at FONT_SIZE.
 * Web: raylib's HiDPI mode doesn't scale the canvas, so the canvas is sized in
 * device pixels (fit_canvas) and everything here is scaled by devicePixelRatio. */
static void setup_text(App *app)
{
#ifdef __EMSCRIPTEN__
    app->scale = (float)emscripten_get_device_pixel_ratio();
    float density = 1.0f;
#else
    app->scale = 1.0f;
    float density = GetWindowScaleDPI().x;
#endif
    if (app->font.texture.id != 0)
        UnloadFont(app->font);

    app->font_size = FONT_SIZE * app->scale;
    app->font = load_font((int)lroundf(app->font_size * fmaxf(density, 1.0f)));
    app->cell_w = (int)ceilf(MeasureTextEx(app->font, "M", app->font_size, 0).x);
    app->cell_h = (int)lroundf(app->font_size);
}

/* Resizes the cell grid to the window; returns false if out of memory. */
static bool fit_grid(App *app)
{
    int cols = GetScreenWidth() / app->cell_w;
    int rows = GetScreenHeight() / app->cell_h;
    if (cols < 1)
        cols = 1;
    if (rows < 1)
        rows = 1;
    if (cols == app->cols && rows == app->rows && app->cells)
        return true;

    Cell *cells = realloc(app->cells, (size_t)cols * (size_t)rows * sizeof *cells);
    if (!cells)
        return false;
    app->cells = cells;
    app->cols = cols;
    app->rows = rows;
    return true;
}

static Color color(Rgb c)
{
    return (Color){c.r, c.g, c.b, 255};
}

static void draw(const App *app)
{
    Vector2 o = grid_origin(app);
    int ox = (int)o.x, oy = (int)o.y;
    float text_dy = ((float)app->cell_h - app->font_size) / 2.0f;

    BeginDrawing();
    ClearBackground(color(PALETTE_BACKGROUND));

    for (int y = 0; y < app->rows; y++)
        for (int x = 0; x < app->cols; x++)
        {
            const Cell *c = &app->cells[y * app->cols + x];
            int px = ox + x * app->cell_w, py = oy + y * app->cell_h;
            const StyleColor *sc = &STYLE_COLORS[c->style];
            Color fg = color(sc->fg);

            if (sc->has_bg)
                DrawRectangle(px, py, app->cell_w, app->cell_h, color(sc->bg));

            if (c->glyph == GLYPH_WALL) /* exact cell, no gaps between walls */
                DrawRectangle(px, py, app->cell_w, app->cell_h, fg);
            else if (c->glyph != ' ')
                DrawTextCodepoint(app->font, (int)c->glyph,
                                  (Vector2){(float)px, (float)py + text_dy}, app->font_size, fg);
        }

    EndDrawing();
}

/* ---- games and storage ------------------------------------------------------ */

static void new_ui(App *app)
{
    ui_init(&app->ui, &app->scores, &app->settings);
    app->ui.touch = app->touch;
#ifndef __EMSCRIPTEN__
    app->ui.can_exit = true; /* a page can't close itself */
#endif
}

/* A new game (the saved one if `resume`); false if out of memory. */
static bool start_game(App *app, bool resume, uint64_t seed)
{
    bool ok = app->use_store ? store_start_game(&app->game, resume, seed, &app->settings)
                             : game_init(&app->game, seed);
    new_ui(app);
    return ok;
}

/* The frontend's side of the ui's requests: saving and deleting. */
static void store_requests(App *app)
{
    Ui *ui = &app->ui;
    if (app->use_store)
    {
        if (ui->scores_changed)
            store_save_scores(&app->scores);
        if (ui->settings_changed)
            store_save_settings(&app->settings);
        if (ui->save_wanted)
            store_save_game(&app->game);
        if (ui->save_delete)
            store_delete_game();
    }
    ui->scores_changed = ui->settings_changed = ui->save_wanted = ui->save_delete = false;
}

/* The window or tab is closing: keep the game. */
static void save_on_exit(App *app)
{
    if (app->use_store && !app->ui.save_delete)
        store_save_game(&app->game); /* does nothing if the game is over */
}

#ifdef __EMSCRIPTEN__
static const char *on_before_unload(int type, const void *reserved, void *arg)
{
    (void)type;
    (void)reserved;
    save_on_exit(arg);
    return NULL; /* no "leave this page?" question */
}

/* Touches, handled here rather than by raylib: one finger lifted near where it
 * landed is a tap there (a slide is not). The first touch turns on the touch
 * buttons. Returning true stops the browser making mouse events of it too. */
static bool on_touch(int type, const EmscriptenTouchEvent *e, void *arg)
{
    App *app = arg;
    app->touch_ms = emscripten_get_now();
    if (!app->touch)
        app->touch = app->ui.touch = true;

    const EmscriptenTouchPoint *t = NULL;
    for (int i = 0; i < e->numTouches && !t; i++)
        if (e->touches[i].isChanged)
            t = &e->touches[i];
    if (!t)
        return true;
    Vector2 p = {(float)t->targetX * app->scale, (float)t->targetY * app->scale};

    if (type == EMSCRIPTEN_EVENT_TOUCHSTART)
    {
        app->finger_down = e->numTouches == 1;
        app->touch_start = p;
    }
    else if (type == EMSCRIPTEN_EVENT_TOUCHEND && app->finger_down)
    {
        app->finger_down = false;
        if (fabsf(p.x - app->touch_start.x) < 1.5f * (float)app->cell_w &&
            fabsf(p.y - app->touch_start.y) < 1.5f * (float)app->cell_h)
        {
            app->tap_pending = true;
            app->tap = app->touch_start;
        }
    }
    else
        app->finger_down = false;
    return true;
}

/* Phones may close a hidden tab without an unload event: save when hidden. */
static bool on_visibility_change(int type, const EmscriptenVisibilityChangeEvent *e, void *arg)
{
    (void)type;
    if (e->hidden)
        save_on_exit(arg);
    return false;
}
#endif

/* ---- main loop ------------------------------------------------------------ */

#ifdef __EMSCRIPTEN__
/* The page's CSS stretches the canvas over the browser window. Give the canvas
 * that size in device pixels, so text is sharp on HiDPI screens, and reload the
 * font when the density changes (browser zoom, moving to another screen). */
static void fit_canvas(App *app)
{
    if ((float)emscripten_get_device_pixel_ratio() != app->scale)
        setup_text(app);

    double w, h;
    if (emscripten_get_element_css_size("#canvas", &w, &h) != EMSCRIPTEN_RESULT_SUCCESS)
        return;
    int pw = (int)lround(w * app->scale), ph = (int)lround(h * app->scale);
    if (pw != GetScreenWidth() || ph != GetScreenHeight())
        SetWindowSize(pw, ph);
}
#endif

static void frame(void *arg)
{
    App *app = arg;

#ifdef __EMSCRIPTEN__
    fit_canvas(app);
#endif
    handle_input(app);
    ui_update(&app->ui, &app->game, (int)(GetFrameTime() * 1000.0f));
    store_requests(app);
    if (app->ui.new_game) /* from the end screen, or a game given up */
    {
        game_free(&app->game);
        app->quit = !start_game(app, false, (uint64_t)time(NULL));
    }

#ifdef __EMSCRIPTEN__
    if (app->quit || !fit_grid(app))
    {
        emscripten_cancel_main_loop();
        return;
    }
#else
    if (!fit_grid(app))
    {
        app->quit = true;
        return;
    }
#endif
    ui_render(&app->ui, &app->game, app->cells, app->cols, app->rows);
    draw(app);
}

#ifndef __EMSCRIPTEN__ /* desktop only: --screenshot */
/* Saves what the window shows, at full pixel density. (raylib's TakeScreenshot
 * applies the HiDPI factor twice on macOS and captures a 2x area.) */
static void save_screenshot(const char *path)
{
    Image shot = LoadImageFromScreen();
    if (!ExportImage(shot, path))
        fprintf(stderr, "Could not save the screenshot to %s\n", path);
    UnloadImage(shot);
}
#endif

static Args parse_args(int argc, char **argv)
{
    Args o = {.seed = (uint64_t)time(NULL)};
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
            o.screenshot = argv[++i];
        else if (strcmp(argv[i], "--keys") == 0 && i + 1 < argc)
            o.keys = argv[++i];
        else
        {
            o.seed = strtoull(argv[i], NULL, 10);
            o.seed_given = true;
        }
    }
    return o;
}

int main(int argc, char **argv)
{
    static App app; /* static: outlives main's frame under Emscripten */
    Args opt = parse_args(argc, argv);

    app.use_store = !opt.screenshot;
#ifdef __EMSCRIPTEN__
    /* A phone or tablet: buttons from the start, not only after the first touch. */
    app.touch = emscripten_run_script_int("matchMedia('(pointer: coarse)').matches ? 1 : 0") != 0;
#endif
    app.settings = app.use_store ? store_load_settings() : settings_default();
    if (app.use_store)
        store_load_scores(&app.scores);
    if (!start_game(&app, !opt.seed_given, opt.seed))
    {
        fprintf(stderr, "Out of memory while generating the level.\n");
        return EXIT_FAILURE;
    }

    SetTraceLogLevel(LOG_WARNING);
#ifdef __EMSCRIPTEN__
    SetConfigFlags(FLAG_VSYNC_HINT); /* size and density: fit_canvas */
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT);
#endif
    InitWindow(800, 600, "RogueC");
    SetExitKey(KEY_NULL); /* Esc doesn't close the game */

    setup_text(&app);

#ifdef __EMSCRIPTEN__
    emscripten_set_beforeunload_callback(&app, on_before_unload);
    emscripten_set_visibilitychange_callback(&app, false, on_visibility_change);
    emscripten_set_touchstart_callback("#canvas", &app, false, on_touch);
    emscripten_set_touchend_callback("#canvas", &app, false, on_touch);
    emscripten_set_touchcancel_callback("#canvas", &app, false, on_touch);
    emscripten_set_main_loop_arg(frame, &app, 0, true); /* never returns */
#else
    /* START_COLS x START_ROWS cells, or less on a small screen */
    int monitor = GetCurrentMonitor();
    int cols = START_COLS, rows = START_ROWS;
    if (cols * app.cell_w > GetMonitorWidth(monitor) * 9 / 10)
        cols = GetMonitorWidth(monitor) * 9 / 10 / app.cell_w;
    if (rows * app.cell_h > GetMonitorHeight(monitor) * 85 / 100)
        rows = GetMonitorHeight(monitor) * 85 / 100 / app.cell_h;
    SetWindowSize(cols * app.cell_w, rows * app.cell_h);
    SetWindowMinSize(20 * app.cell_w, 8 * app.cell_h);

    if (opt.screenshot)
    {
        for (const char *k = opt.keys; k && *k; k++)
            press(&app, *k);
        for (int i = 0; i < 3; i++) /* a few frames so both buffers hold the image */
            frame(&app);
        save_screenshot(opt.screenshot);
    }
    else
    {
        SetTargetFPS(60);
        bool waiting = false;
        while (!app.quit && !WindowShouldClose())
        {
            /* Turn-based: sleep until there's input. Real time: keep the clock running. */
            if (waiting == app.game.realtime)
            {
                waiting = !app.game.realtime;
                if (waiting)
                    EnableEventWaiting();
                else
                    DisableEventWaiting();
            }
            frame(&app);
        }
        if (!app.quit) /* the window was closed */
            save_on_exit(&app);
    }

    UnloadFont(app.font);
    CloseWindow();
    free(app.cells);
    game_free(&app.game);
#endif
    return EXIT_SUCCESS;
}
