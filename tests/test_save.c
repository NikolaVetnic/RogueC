#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "core/save.h"
#include "core/settings.h"

/* Some moves and waits, the same for any game. */
static void play(Game *g, int steps)
{
    static const Action SCRIPT[] = {ACTION_MOVE_E, ACTION_MOVE_S,  ACTION_WAIT,    ACTION_MOVE_W,
                                    ACTION_MOVE_N, ACTION_MOVE_NE, ACTION_MOVE_SW, ACTION_REST};
    for (int i = 0; i < steps && !game_over(g); i++)
        game_step(g, SCRIPT[i % (int)(sizeof SCRIPT / sizeof SCRIPT[0])]);
}

static bool same_item(const Item *a, const Item *b)
{
    return a->cls == b->cls && a->kind == b->kind && a->count == b->count &&
           a->enchant == b->enchant && a->enchant_known == b->enchant_known &&
           a->equipped == b->equipped;
}

static bool same_player(const Player *a, const Player *b)
{
    bool same = vec2_eq(a->pos, b->pos) && a->hp == b->hp && a->energy == b->energy &&
                memcmp(&a->base, &b->base, sizeof a->base) == 0 && a->gold == b->gold &&
                a->level == b->level && a->xp == b->xp && a->nutrition == b->nutrition &&
                a->haste == b->haste && a->confused == b->confused && a->blind == b->blind &&
                a->pack_count == b->pack_count;
    for (int i = 0; same && i < a->pack_count; i++)
        same = same_item(&a->pack[i], &b->pack[i]);
    return same;
}

static bool same_level(const Level *a, const Level *b)
{
    size_t n = (size_t)a->map.w * (size_t)a->map.h;
    bool same = a->map.w == b->map.w && a->map.h == b->map.h &&
                memcmp(a->map.tiles, b->map.tiles, n) == 0 &&
                memcmp(a->map.explored, b->map.explored, n * sizeof *a->map.explored) == 0 &&
                vec2_eq(a->stairs_up, b->stairs_up) && vec2_eq(a->stairs_down, b->stairs_down) &&
                a->caves == b->caves && a->monster_count == b->monster_count &&
                a->item_count == b->item_count;
    for (int i = 0; same && i < a->monster_count; i++)
    {
        const Monster *x = &a->monsters[i], *y = &b->monsters[i];
        same = x->kind == y->kind && vec2_eq(x->pos, y->pos) && x->hp == y->hp &&
               x->energy == y->energy && x->awake == y->awake && x->track == y->track &&
               vec2_eq(x->heading, y->heading);
    }
    for (int i = 0; same && i < a->item_count; i++)
        same = same_item(&a->items[i].item, &b->items[i].item) &&
               vec2_eq(a->items[i].pos, b->items[i].pos) && a->items[i].seen == b->items[i].seen;
    return same;
}

static bool same_game(const Game *a, const Game *b)
{
    bool same = a->seed == b->seed && a->rng.state == b->rng.state && a->rng.inc == b->rng.inc &&
                a->depth == b->depth && a->turn == b->turn && a->deepest == b->deepest &&
                a->realtime == b->realtime && a->wander_odds == b->wander_odds &&
                same_player(&a->player, &b->player) &&
                memcmp(&a->lore, &b->lore, sizeof a->lore) == 0;
    for (int d = 0; same && d < DUNGEON_DEPTH; d++)
        same = same_level(&a->levels[d], &b->levels[d]);
    return same;
}

/* A loaded game equals the saved one and goes on exactly as it would have. */
static void saved_games_resume_exactly(void)
{
    for (uint64_t seed = 1; seed <= 5; seed++)
    {
        Game g, h;
        game_init(&g, seed);
        play(&g, 40);
        g.levels[3].map.explored[123] = true; /* something on a level not visited */
        char *text = game_save(&g);
        CHECK(text != NULL);
        CHECK(game_load(&h, text));
        free(text);

        CHECK(same_game(&g, &h));
        CHECK(h.running && h.map == &h.levels[h.depth - 1].map);
        CHECK(strncmp(msglog_get(&h.log, 0)->text, "Welcome back.", 13) == 0);
        CHECK(msglog_len(&h.log) ==
              (msglog_len(&g.log) < MSGLOG_CAPACITY ? msglog_len(&g.log) + 1 : MSGLOG_CAPACITY));
        CHECK(strcmp(msglog_get(&h.log, 1)->text, msglog_get(&g.log, 0)->text) == 0);

        play(&g, 60);
        play(&h, 60);
        CHECK(same_game(&g, &h));
        game_free(&g);
        game_free(&h);
    }
}

static void finished_games_are_not_saved(void)
{
    Game g;
    game_init(&g, 2);
    game_kill_player(&g, DEATH_STARVATION);
    CHECK(game_save(&g) == NULL);
    game_free(&g);
}

/* Damaged text is refused, never read past its end (the sanitizers watch). */
static void damaged_saves_are_refused(void)
{
    Game g, h;
    game_init(&g, 7);
    play(&g, 20);
    char *text = game_save(&g);
    size_t len = strlen(text);

    CHECK(!game_load(&h, ""));
    CHECK(!game_load(&h, "not a save!"));
    CHECK(!game_load(&h, "AAAA"));
    char *cut = malloc(len + 1);
    for (size_t i = 0; i < 300; i++)
    {
        size_t n = i < 100   ? i
                   : i < 200 ? len - (i - 99)
                             : (i - 200) * (len / 100); /* short, near the end, spread */
        memcpy(cut, text, n);
        cut[n] = '\0';
        CHECK(!game_load(&h, cut));
    }
    for (int i = 0; i < 300; i++) /* one character changed: the checksum catches it */
    {
        memcpy(cut, text, len + 1);
        size_t at = (size_t)(i * 7919) % (len - 4);
        cut[at] = cut[at] == 'A' ? 'B' : 'A';
        CHECK(!game_load(&h, cut));
    }
    free(cut);
    free(text);
    game_free(&g);
}

/* Out-of-range fields with a correct checksum (as an edited save would have)
 * are refused. Each is saved from a game holding one bad value. */
static void impossible_values_are_refused(void)
{
    for (int bad = 0; bad < 8; bad++)
    {
        Game g, h;
        game_init(&g, 9);
        game_level(&g)->items[0].pos = vec2(1, 1);
        switch (bad)
        {
        case 0:
            g.depth = DUNGEON_DEPTH + 1;
            break;
        case 1:
            g.player.level = 0;
            break;
        case 2:
            g.levels[2].monster_count = 1, g.levels[2].monsters[0].kind = MON_COUNT;
            break;
        case 3:
            g.levels[5].map.tiles[10] = TILE_COUNT;
            break;
        case 4:
            g.player.pack[0].kind = WEAPON_COUNT;
            break;
        case 5:
            g.player.base.speed = 0;
            break;
        case 6:
            game_level(&g)->items[0].pos = vec2(MAP_WIDTH, 3);
            break;
        default:
            g.lore.potion_look[0] = POTION_LOOK_COUNT;
            break;
        }
        char *text = game_save(&g);
        CHECK(text && !game_load(&h, text));
        free(text);
        game_free(&g);
    }
}

static void settings_round_trip(void)
{
    Settings d = settings_default();
    CHECK(!d.realtime_start && d.speed == SPEED_NORMAL && d.log_lines == 2 &&
          d.colours == COLOURS_AUTO && settings_tick_ms(&d) == 300);

    Settings s = {
        .realtime_start = true, .speed = SPEED_FAST, .log_lines = 4, .colours = COLOURS_MONO};
    char text[SETTINGS_TEXT_SIZE];
    CHECK(settings_format(&s, text, sizeof text));
    Settings t = settings_parse(text);
    CHECK(memcmp(&s, &t, sizeof s) == 0 && settings_tick_ms(&t) == 200);
    CHECK(!settings_format(&s, text, 10));

    t = settings_parse("speed=7\nlog_lines=9\ncolours=-1\nrealtime_start=1\nnonsense\nfoo=2");
    CHECK(t.realtime_start && t.speed == SPEED_NORMAL && t.log_lines == 2 &&
          t.colours == COLOURS_AUTO); /* bad values keep the defaults */
    t = settings_parse(NULL);
    CHECK(memcmp(&t, &d, sizeof t) == 0);
}

void run_save_tests(void)
{
    RUN(saved_games_resume_exactly);
    RUN(finished_games_are_not_saved);
    RUN(damaged_saves_are_refused);
    RUN(impossible_values_are_refused);
    RUN(settings_round_trip);
}
