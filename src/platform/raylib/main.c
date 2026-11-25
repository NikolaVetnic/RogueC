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

#define FONT_FILE "assets/fonts/JetBrainsMono-Regular.ttf"
/* raylib's font size is the line height (ascent + descent), in logical pixels;
 * it is also the cell height. Letters are about 3/4 of it. */
#define FONT_SIZE 22
#define START_COLS 100
#define START_ROWS 43 /* map height + status line */

#define GLYPH_FULL_BLOCK 0x2588u

static const Color BACKGROUND = {12, 12, 16, 255};

/* Foreground per Style; the status line also gets a background. */
static const Color STYLE_FG[STYLE_COUNT] = {
    [STYLE_DEFAULT] = {200, 200, 200, 255}, [STYLE_FLOOR_LIT] = {170, 170, 160, 255},
    [STYLE_FLOOR_SEEN] = {60, 66, 84, 255}, [STYLE_WALL_LIT] = {150, 140, 125, 255},
    [STYLE_WALL_SEEN] = {44, 48, 62, 255},  [STYLE_PLAYER] = {255, 210, 70, 255},
    [STYLE_STATUS] = {20, 20, 24, 255},
};
static const Color STATUS_BG = {170, 170, 160, 255};

typedef struct
{
    Game game;
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

static Action char_to_action(int ch)
{
    // clang-format off
    switch (ch)
    {
    case 'w': case 'W': case 'k': case '8': return ACTION_MOVE_N;
    case 's': case 'S': case 'j': case '2': return ACTION_MOVE_S;
    case 'a': case 'A': case 'h': case '4': return ACTION_MOVE_W;
    case 'd': case 'D': case 'l': case '6': return ACTION_MOVE_E;
    case 'y': case '7': return ACTION_MOVE_NW;
    case 'u': case '9': return ACTION_MOVE_NE;
    case 'b': case '1': return ACTION_MOVE_SW;
    case 'n': case '3': return ACTION_MOVE_SE;
    case '.': case '5': return ACTION_WAIT;
    case 'q': case 'Q': return ACTION_QUIT;
    default:            return ACTION_NONE;
    }
    // clang-format on
}

/* Keys that type no character. Letters, digits and the numpad arrive as
 * characters (GetCharPressed), including the operating system's key repeat. */
static const struct
{
    int key;
    Action action;
} KEY_ACTIONS[] = {
    {KEY_UP, ACTION_MOVE_N},    {KEY_DOWN, ACTION_MOVE_S},       {KEY_LEFT, ACTION_MOVE_W},
    {KEY_RIGHT, ACTION_MOVE_E}, {KEY_HOME, ACTION_MOVE_NW},      {KEY_PAGE_UP, ACTION_MOVE_NE},
    {KEY_END, ACTION_MOVE_SW},  {KEY_PAGE_DOWN, ACTION_MOVE_SE},
};

static void apply(App *app, Action a)
{
    game_step(&app->game, a);
    if (!app->game.running)
        app->quit = true;
}

static void handle_input(App *app)
{
    for (int ch = GetCharPressed(); ch != 0; ch = GetCharPressed())
        apply(app, char_to_action(ch));

    for (size_t i = 0; i < sizeof KEY_ACTIONS / sizeof KEY_ACTIONS[0]; i++)
        if (IsKeyPressed(KEY_ACTIONS[i].key) || IsKeyPressedRepeat(KEY_ACTIONS[i].key))
            apply(app, KEY_ACTIONS[i].action);
}

/* ---- drawing -------------------------------------------------------------- */

/* Loads printable ASCII at `pixel_size`; the full block is drawn as a
 * rectangle, not a glyph. Falls back to raylib's built-in font. */
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

static void draw(const App *app)
{
    /* Centre the grid in the window. */
    int ox = (GetScreenWidth() - app->cols * app->cell_w) / 2;
    int oy = (GetScreenHeight() - app->rows * app->cell_h) / 2;
    float text_dy = ((float)app->cell_h - app->font_size) / 2.0f;

    BeginDrawing();
    ClearBackground(BACKGROUND);

    for (int y = 0; y < app->rows; y++)
        for (int x = 0; x < app->cols; x++)
        {
            const Cell *c = &app->cells[y * app->cols + x];
            int px = ox + x * app->cell_w, py = oy + y * app->cell_h;
            Color fg = STYLE_FG[c->style];

            if (c->style == STYLE_STATUS)
                DrawRectangle(px, py, app->cell_w, app->cell_h, STATUS_BG);

            if (c->glyph == GLYPH_FULL_BLOCK) /* exact cell, no gaps between walls */
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

#ifdef __EMSCRIPTEN__
    if (app->quit) /* a page can't close itself: start a new game instead */
    {
        game_free(&app->game);
        app->quit = !game_init(&app->game, (uint64_t)time(NULL));
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
    render_game(&app->game, app->cells, app->cols, app->rows);
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
    InitWindow(800, 600, "RogueC");
    SetExitKey(KEY_NULL); /* Esc doesn't close the game */

    setup_text(&app);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(frame, &app, 0, true); /* never returns */
#else
    SetWindowSize(START_COLS * app.cell_w, START_ROWS * app.cell_h);
    SetWindowMinSize(20 * app.cell_w, 8 * app.cell_h);

    if (opt.screenshot)
    {
        for (const char *k = opt.keys; k && *k; k++)
            apply(&app, char_to_action(*k));
        for (int i = 0; i < 3; i++) /* a few frames so both buffers hold the image */
            frame(&app);
        save_screenshot(opt.screenshot);
    }
    else
    {
        SetTargetFPS(60);
        EnableEventWaiting(); /* turn-based: sleep until there's input */
        while (!app.quit && !WindowShouldClose())
            frame(&app);
    }

    UnloadFont(app.font);
    CloseWindow();
    free(app.cells);
    game_free(&app.game);
#endif
    return EXIT_SUCCESS;
}
