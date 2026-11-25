#include "core/ui.h"

#include <stdio.h>

enum
{
    LOG_PAGE = 10,    /* lines per Page Up / Page Down in the history */
    CTRL_P = 16,      /* message history, as in Rogue */
    MAX_CATCH_UP = 4, /* real time: ticks run at most per update (after a stall) */
};

void ui_init(Ui *ui, ScoreTable *scores)
{
    *ui = (Ui){.screen = SCREEN_MAP, .scores = scores, .score_rank = -1};
}

static Action action_for_key(int key)
{
    // clang-format off
    switch (key)
    {
    case 'w': case 'W': case 'k': case '8': case UI_KEY_UP:        return ACTION_MOVE_N;
    case 's': case 'S': case 'j': case '2': case UI_KEY_DOWN:      return ACTION_MOVE_S;
    case 'a': case 'A': case 'h': case '4': case UI_KEY_LEFT:      return ACTION_MOVE_W;
    case 'd': case 'D': case 'l': case '6': case UI_KEY_RIGHT:     return ACTION_MOVE_E;
    case 'y': case '7': case UI_KEY_HOME:                          return ACTION_MOVE_NW;
    case 'u': case '9': case UI_KEY_PAGE_UP:                       return ACTION_MOVE_NE;
    case 'b': case '1': case UI_KEY_END:                           return ACTION_MOVE_SW;
    case 'n': case '3': case UI_KEY_PAGE_DOWN:                     return ACTION_MOVE_SE;
    case '.': case '5': case UI_KEY_CENTER:                        return ACTION_WAIT;
    case 'Z':                                                      return ACTION_REST;
    case '>':                                                      return ACTION_DESCEND;
    case '<':                                                      return ACTION_ASCEND;
    case ',': case 'g':                                            return ACTION_PICK_UP;
    case 'q': case 'Q':                                            return ACTION_QUIT;
    default:                                                       return ACTION_NONE;
    }
    // clang-format on
}

static void log_key(Ui *ui, const Game *g, int key)
{
    int max_scroll = msglog_len(&g->log) > 0 ? msglog_len(&g->log) - 1 : 0;

    // clang-format off
    switch (key)
    {
    case UI_KEY_UP:   case 'k': case 'w': case '8': ui->scroll++; break;
    case UI_KEY_DOWN: case 'j': case 's': case '2': ui->scroll--; break;
    case UI_KEY_PAGE_UP:   ui->scroll += LOG_PAGE; break;
    case UI_KEY_PAGE_DOWN: ui->scroll -= LOG_PAGE; break;
    case UI_KEY_HOME:      ui->scroll = max_scroll; break;
    case UI_KEY_END:       ui->scroll = 0; break;
    default:               ui->screen = SCREEN_MAP; break;
    }
    // clang-format on

    if (ui->scroll > max_scroll)
        ui->scroll = max_scroll;
    if (ui->scroll < 0)
        ui->scroll = 0;
}

/* Turn-based, or ready: act now. Real time and not ready: keep the latest
 * move for when the player is (quitting never waits). */
static void act(Ui *ui, Game *g, Action a, int item)
{
    if (a == ACTION_NONE)
        return;
    if (g->realtime && a != ACTION_QUIT && !game_ready(g))
    {
        ui->pending = a;
        ui->pending_item = item;
    }
    else
    {
        ui->pending = ACTION_NONE;
        game_step_item(g, a, item);
    }
}

/* A pack letter: use or drop that item, back to the map. Other keys: back. */
static void pack_key(Ui *ui, Game *g, int key)
{
    ui->screen = SCREEN_MAP;
    int i = key - 'a';
    if (i >= 0 && i < g->player.pack_count)
        act(ui, g, ui->pack_purpose == PACK_USE ? ACTION_USE : ACTION_DROP, i);
}

/* Once the game is over: the end screen, and the score into the table. */
static void check_death(Ui *ui, const Game *g)
{
    if (!game_over(g) || ui->screen == SCREEN_END)
        return;
    ui->screen = SCREEN_END;
    if (ui->scores)
    {
        ui->score_rank = scores_add(ui->scores, score_of(g));
        ui->scores_changed = true;
    }
}

void ui_key(Ui *ui, Game *g, int key)
{
    switch (ui->screen)
    {
    case SCREEN_MAP:
        if (key == '?')
            ui->screen = SCREEN_HELP;
        else if (key == 'm' || key == 'M' || key == CTRL_P)
        {
            ui->screen = SCREEN_LOG;
            ui->scroll = 0;
        }
        else if (key == 'i' || key == 'D')
        {
            ui->screen = SCREEN_PACK;
            ui->pack_purpose = key == 'i' ? PACK_USE : PACK_DROP;
        }
        else if (key == 'T')
        {
            game_set_realtime(g, !g->realtime);
            ui->pending = ACTION_NONE;
            ui->clock_ms = 0;
        }
        else
            act(ui, g, action_for_key(key), -1);
        check_death(ui, g);
        break;
    case SCREEN_PACK:
        pack_key(ui, g, key);
        check_death(ui, g);
        break;
    case SCREEN_HELP:
        ui->screen = SCREEN_MAP; /* any key */
        break;
    case SCREEN_LOG:
        log_key(ui, g, key);
        break;
    case SCREEN_END:
        if (key == '\n' || key == ' ')
            ui->new_game = true;
        else if (key == 'q' || key == 'Q' || key == 27)
            g->running = false;
        break;
    }
}

void ui_update(Ui *ui, Game *g, int elapsed_ms)
{
    if (!g->realtime || ui->screen != SCREEN_MAP || g->dead)
    {
        ui->clock_ms = 0;
        return;
    }
    ui->clock_ms += elapsed_ms;
    if (ui->clock_ms > MAX_CATCH_UP * UI_TICK_MS)
        ui->clock_ms = MAX_CATCH_UP * UI_TICK_MS;

    for (;;)
    {
        if (ui->pending != ACTION_NONE && game_ready(g))
            act(ui, g, ui->pending, ui->pending_item);
        if (ui->clock_ms < UI_TICK_MS || g->dead)
            break;
        ui->clock_ms -= UI_TICK_MS;
        game_tick(g);
    }
    check_death(ui, g);
}

/* ---- screens -------------------------------------------------------------- */

static const struct
{
    const char *what;
    const char *keys;
} HELP_KEYS[] = {
    {"Move", "w a s d   arrow keys   h j k l   numpad"},
    {"Move diagonally", "y u b n   Home PgUp End PgDn   numpad 7 9 1 3"},
    {"Attack", "move into a monster"},
    {"Wait a turn", ".   numpad 5"},
    {"Rest until healed", "Z"},
    {"Pick up", ",   g"},
    {"Inventory: use an item", "i   then the item's letter"},
    {"Drop an item", "D   then the item's letter"},
    {"Stairs down / up", ">   <   (standing on them)"},
    {"Message history", "m   Ctrl-P"},
    {"Real time on / off", "T   (the dungeon moves on its own; help and history pause it)"},
    {"Help", "?"},
    {"Quit", "q"},
};

static const struct
{
    uint32_t glyph;
    Style style;
    const char *what;
} HELP_LEGEND[] = {
    {GLYPH_PLAYER, STYLE_PLAYER, "you"},
    {GLYPH_FLOOR, STYLE_FLOOR_LIT, "floor in view"},
    {GLYPH_FLOOR, STYLE_FLOOR_SEEN, "floor remembered"},
    {GLYPH_WALL, STYLE_WALL_LIT, "rock in view"},
    {GLYPH_WALL, STYLE_WALL_SEEN, "rock remembered"},
    {GLYPH_STAIRS_DOWN, STYLE_STAIRS_LIT, "stairs down"},
    {GLYPH_STAIRS_UP, STYLE_STAIRS_LIT, "stairs up"},
};

static void render_help(const Game *g, Cell *cells, int cols, int rows)
{
    enum
    {
        LEFT = 2,
        KEYS_X = 27, /* column of the key names */
    };
    int y = 1;

    render_clear(cells, cols, rows, STYLE_DEFAULT);
    render_text(cells, cols, rows, LEFT, y, "Help", STYLE_HEADING);
    y += 2;
    render_text(
        cells, cols, rows, LEFT, y,
        "Goal: find the Sunstone (*) on depth 10 and carry it out by the stairs up on depth 1.",
        STYLE_TEXT);
    y += 2;
    for (size_t i = 0; i < sizeof HELP_KEYS / sizeof HELP_KEYS[0]; i++, y++)
    {
        render_text(cells, cols, rows, LEFT, y, HELP_KEYS[i].what, STYLE_TEXT);
        render_text(cells, cols, rows, KEYS_X, y, HELP_KEYS[i].keys, STYLE_KEY);
    }

    y++;
    render_text(cells, cols, rows, LEFT, y++, "Map", STYLE_HEADING);
    for (size_t i = 0; i < sizeof HELP_LEGEND / sizeof HELP_LEGEND[0]; i++, y++)
    {
        if (y < rows && LEFT < cols)
            cells[y * cols + LEFT] =
                (Cell){.glyph = HELP_LEGEND[i].glyph, .style = (uint8_t)HELP_LEGEND[i].style};
        render_text(cells, cols, rows, LEFT + 4, y, HELP_LEGEND[i].what, STYLE_TEXT);
    }

    /* Monsters, weakest first, in their colours. */
    for (int k = 0; k < MON_COUNT; k++)
        if (y < rows && LEFT + 2 * k < cols)
            cells[y * cols + LEFT + 2 * k] = (Cell){
                .glyph = MONSTERS[k].glyph, .style = (uint8_t)render_monster_style((MonsterKind)k)};
    render_text(cells, cols, rows, LEFT + 2 * MON_COUNT + 2, y++, "monsters, deeper ones tougher",
                STYLE_TEXT);

    /* Items: glyph and word. */
    static const char *const ITEM_WORDS[ITEM_CLASS_COUNT] = {
        "gold", "food", "potion", "scroll", "weapon", "armour", "the Sunstone: your goal"};
    int x = LEFT;
    for (int c = 0; c < ITEM_CLASS_COUNT; c++)
    {
        if (y < rows && x < cols)
            cells[y * cols + x] = (Cell){.glyph = item_glyph((ItemClass)c),
                                         .style = (uint8_t)(c == ITEM_GOLD   ? STYLE_GOLD
                                                            : c == ITEM_GOAL ? STYLE_GOAL
                                                                             : STYLE_ITEM)};
        x = render_text(cells, cols, rows, x + 2, y, ITEM_WORDS[c], STYLE_TEXT) + 3;
    }
    y++;

    char seed[48];
    snprintf(seed, sizeof seed, "Seed %llu", (unsigned long long)g->seed);
    render_text(cells, cols, rows, LEFT, y + 1, seed, STYLE_TEXT_DIM);
    render_text(cells, cols, rows, LEFT, y + 2, "Press any key to go back.", STYLE_TEXT_DIM);
}

/* Messages oldest first, the newest last; ui->scroll lines up from it. */
static void render_history(Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    enum
    {
        LEFT = 2,
        TOP = 3, /* heading, hint, column names */
    };
    int lines = rows - TOP;
    int count = msglog_len(&g->log);
    int max_scroll = count > lines ? count - lines : 0;
    if (ui->scroll > max_scroll)
        ui->scroll = max_scroll;

    render_clear(cells, cols, rows, STYLE_DEFAULT);
    render_text(cells, cols, rows, LEFT, 0, "Message history", STYLE_HEADING);
    render_text(cells, cols, rows, LEFT, 1,
                "Up/Down, PgUp/PgDn, Home/End scroll; any other key goes back.", STYLE_TEXT_DIM);

    render_text(cells, cols, rows, LEFT, TOP - 1, " Turn  Message", STYLE_TEXT_DIM);
    int shown = count < lines ? count : lines; /* a short history starts at the top */
    for (int line = 0; line < shown; line++)
    {
        const Message *m = msglog_get(&g->log, ui->scroll + shown - 1 - line);
        if (!m)
            continue;
        char text[MSG_TEXT_SIZE + 32];
        char body[MSG_TEXT_SIZE + 16];
        render_message_text(m, body, sizeof body);
        snprintf(text, sizeof text, "%5d  %s", m->turn, body);
        render_text(cells, cols, rows, LEFT, TOP + line, text, STYLE_TEXT);
    }
}

/* Died or won: what happened, the score, the high-score table. */
static void render_end(const Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    enum
    {
        LEFT = 2,
        TABLE_TOP = 9,
    };
    char text[128];
    Score s = score_of(g);

    render_clear(cells, cols, rows, STYLE_DEFAULT);
    render_text(cells, cols, rows, LEFT, 1, g->won ? "You won!" : "You died.", STYLE_HEADING);
    if (s.cause[0] >= 'a' && s.cause[0] <= 'z')
        s.cause[0] = (char)(s.cause[0] - 'a' + 'A');
    snprintf(text, sizeof text, "%s, turn %d, at level %d.", s.cause, g->turn, g->player.level);
    render_text(cells, cols, rows, LEFT, 3, text, STYLE_TEXT);
    snprintf(text, sizeof text, "Score %d: %d gold + 10 x %d experience + 100 x depth %d%s.",
             s.points, g->player.gold, g->player.xp, g->deepest,
             g->won ? " + 2000 for the win" : "");
    render_text(cells, cols, rows, LEFT, 4, text, STYLE_TEXT);
    snprintf(text, sizeof text, "Seed %llu", (unsigned long long)g->seed);
    render_text(cells, cols, rows, LEFT, 5, text, STYLE_TEXT_DIM);

    int x = render_text(cells, cols, rows, LEFT, 7, "Enter", STYLE_KEY);
    x = render_text(cells, cols, rows, x, 7, ": new game    ", STYLE_TEXT);
    x = render_text(cells, cols, rows, x, 7, "q", STYLE_KEY);
    render_text(cells, cols, rows, x, 7, ": quit", STYLE_TEXT);

    if (!ui->scores || ui->scores->count == 0)
        return;
    render_text(cells, cols, rows, LEFT, TABLE_TOP, "High scores", STYLE_HEADING);
    for (int i = 0; i < ui->scores->count; i++)
    {
        const Score *e = &ui->scores->entries[i];
        snprintf(text, sizeof text, "%2d. %6d  level %2d  %s", i + 1, e->points, e->level,
                 e->cause);
        render_text(cells, cols, rows, LEFT, TABLE_TOP + 2 + i, text,
                    i == ui->score_rank ? STYLE_HEADING : STYLE_TEXT);
    }
}

static void render_pack(const Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    enum
    {
        LEFT = 2,
        TOP = 3,
    };
    const Player *p = &g->player;
    render_clear(cells, cols, rows, STYLE_DEFAULT);
    render_text(cells, cols, rows, LEFT, 0,
                ui->pack_purpose == PACK_USE ? "Inventory: use which item?" : "Drop which item?",
                STYLE_HEADING);
    render_text(cells, cols, rows, LEFT, 1,
                ui->pack_purpose == PACK_USE ? "Its letter eats, drinks, reads, wields, wears, or "
                                               "takes off; any other key goes back."
                                             : "Its letter drops it; any other key goes back.",
                STYLE_TEXT_DIM);
    if (p->pack_count == 0)
        render_text(cells, cols, rows, LEFT, TOP, "(your pack is empty)", STYLE_TEXT_DIM);

    for (int i = 0; i < p->pack_count; i++)
    {
        char name[80], letter[4] = {(char)('a' + i), ' ', '-', '\0'};
        item_name(&g->lore, &p->pack[i], name, sizeof name);
        int x = render_text(cells, cols, rows, LEFT, TOP + i, letter, STYLE_KEY);
        render_text(cells, cols, rows, x + 1, TOP + i, name, STYLE_TEXT);
    }
    char gold[32];
    snprintf(gold, sizeof gold, "Gold: %d", p->gold);
    render_text(cells, cols, rows, LEFT, TOP + (p->pack_count > 0 ? p->pack_count : 1) + 1, gold,
                STYLE_GOLD);
}

void ui_render(Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    if (cols <= 0 || rows <= 0)
        return;

    switch (ui->screen)
    {
    case SCREEN_MAP:
        render_game(g, cells, cols, rows);
        break;
    case SCREEN_HELP:
        render_help(g, cells, cols, rows);
        break;
    case SCREEN_LOG:
        render_history(ui, g, cells, cols, rows);
        break;
    case SCREEN_END:
        render_end(ui, g, cells, cols, rows);
        break;
    case SCREEN_PACK:
        render_pack(ui, g, cells, cols, rows);
        break;
    }
}
