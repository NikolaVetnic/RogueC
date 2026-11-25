/* Desktop and browser frontend: raylib window, keyboard input, cell-grid drawing.
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
    Font font;
    float scale;     /* drawing units per logical pixel: devicePixelRatio on the web, else 1 */
    float font_size; /* FONT_SIZE * scale */
    int cell_w;
    int cell_h;
    Cell *cells;
    int cols;
    int rows;
    bool quit;
} App;

/* Command-line options (desktop only). */
typedef struct
{
    uint64_t seed;
    const char *screenshot; /* save one frame here and exit */
    const char *keys;       /* typed before the screenshot */
} Options;

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

static int special_key(int key)
{
    for (size_t i = 0; i < sizeof SPECIAL_KEYS / sizeof SPECIAL_KEYS[0]; i++)
        if (SPECIAL_KEYS[i].key == key)
            return SPECIAL_KEYS[i].ui_key;
    return 0;
}

static void handle_input(App *app)
{
    for (int ch = GetCharPressed(); ch != 0; ch = GetCharPressed())
        press(app, ch);

    /* Presses from raylib's queue: IsKeyPressed compares key states between
     * frames and misses a key pressed and released within one frame. */
    for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed())
        if (special_key(key))
            press(app, special_key(key));

    for (size_t i = 0; i < sizeof SPECIAL_KEYS / sizeof SPECIAL_KEYS[0]; i++)
        if (IsKeyPressedRepeat(SPECIAL_KEYS[i].key))
            press(app, SPECIAL_KEYS[i].ui_key);
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
    /* Centre the grid in the window. */
    int ox = (GetScreenWidth() - app->cols * app->cell_w) / 2;
    int oy = (GetScreenHeight() - app->rows * app->cell_h) / 2;
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
    if (app->ui.new_game) /* from the death screen */
    {
        game_free(&app->game);
        app->quit = !game_init(&app->game, (uint64_t)time(NULL));
        ui_init(&app->ui);
    }

#ifdef __EMSCRIPTEN__
    if (app->quit) /* a page can't close itself: start a new game instead */
    {
        game_free(&app->game);
        app->quit = !game_init(&app->game, (uint64_t)time(NULL));
        ui_init(&app->ui);
    }
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

static Options parse_options(int argc, char **argv)
{
    Options o = {.seed = (uint64_t)time(NULL)};
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
            o.screenshot = argv[++i];
        else if (strcmp(argv[i], "--keys") == 0 && i + 1 < argc)
            o.keys = argv[++i];
        else
            o.seed = strtoull(argv[i], NULL, 10);
    }
    return o;
}

int main(int argc, char **argv)
{
    static App app; /* static: outlives main's frame under Emscripten */
    Options opt = parse_options(argc, argv);

    if (!game_init(&app.game, opt.seed))
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
    ui_init(&app.ui);
    InitWindow(800, 600, "RogueC");
    SetExitKey(KEY_NULL); /* Esc doesn't close the game */

    setup_text(&app);

#ifdef __EMSCRIPTEN__
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
    }

    UnloadFont(app.font);
    CloseWindow();
    free(app.cells);
    game_free(&app.game);
#endif
    return EXIT_SUCCESS;
}
