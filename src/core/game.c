#include "core/game.h"

#include <stdlib.h>

#include "core/ai.h"
#include "core/fov.h"
#include "core/inventory.h"
#include "core/path.h"

enum
{
    MAP_WIDTH = 100,
    MAP_HEIGHT = 42,
    FOV_RADIUS = 10,
    WANDER_SPAWN_ODDS = 300, /* default Game.wander_odds */
    WANDER_SPAWN_DIST = 15,  /* ... at least this many steps away, out of view */
};

/* Bare-handed and unarmoured; gear adds to it (game_player_stats). */
static const Stats PLAYER_STATS = {
    .hp_max = 20, .to_hit = 1, .defense = 0, .dmg_min = 1, .dmg_max = 2, .speed = 100};

/* Indexed by action - ACTION_MOVE_N, clockwise from north. */
static const Vec2 MOVE_DIR[8] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
};

Level *game_level(Game *g)
{
    return &g->levels[g->depth - 1];
}

Stats game_player_stats(const Game *g)
{
    const Player *p = &g->player;
    Stats s = p->base;
    for (int i = 0; i < p->pack_count; i++)
    {
        const Item *it = &p->pack[i];
        if (!it->equipped)
            continue;
        if (it->cls == ITEM_WEAPON)
        {
            const WeaponInfo *w = &WEAPONS[it->kind];
            s.to_hit += w->to_hit + it->enchant;
            s.dmg_min = w->dmg_min + it->enchant;
            s.dmg_max = w->dmg_max + it->enchant;
        }
        else if (it->cls == ITEM_ARMOUR)
            s.defense += ARMOURS[it->kind].defense + it->enchant;
    }
    if (p->haste > 0)
        s.speed *= 2;
    return s;
}

Hunger game_hunger(const Game *g)
{
    int n = g->player.nutrition;
    return n <= 0             ? HUNGER_STARVING
           : n < WEAK_BELOW   ? HUNGER_WEAK
           : n < HUNGRY_BELOW ? HUNGER_HUNGRY
                              : HUNGER_FED;
}

/* ---- setup ----------------------------------------------------------------- */

static void free_levels(Level *levels, int count)
{
    for (int i = 0; i < count; i++)
        level_free(&levels[i]);
    free(levels);
}

/* Announces monsters that just came into view. */
static void notice_monsters(Game *g)
{
    Level *lv = game_level(g);
    for (int i = 0; i < lv->monster_count; i++)
    {
        Monster *m = &lv->monsters[i];
        bool visible = map_is_visible(g->map, m->pos.x, m->pos.y);
        if (visible && !m->seen)
            msglog_add(&g->log, g->turn, "You see %s %s.", monster_article(m->kind),
                       monster_info(m)->name);
        m->seen = visible;
    }
}

void game_look(Game *g)
{
    fov_compute(g->map, g->player.pos, g->player.blind > 0 ? 1 : g->fov_radius);
    notice_monsters(g);
    Level *lv = game_level(g);
    for (int i = 0; i < lv->item_count; i++)
        if (map_is_visible(g->map, lv->items[i].pos.x, lv->items[i].pos.y))
            lv->items[i].seen = true;
}

/* Moves the player to `pos` on level `depth` and looks around. */
static void enter_level(Game *g, int depth, Vec2 pos)
{
    g->depth = depth;
    g->map = &g->levels[depth - 1].map;
    g->player.pos = pos;
    for (int i = 0; i < game_level(g)->monster_count; i++)
        game_level(g)->monsters[i].seen = false; /* announce them again */
    game_look(g);
}

bool game_init(Game *g, uint64_t seed)
{
    *g = (Game){
        .seed = seed, .fov_radius = FOV_RADIUS, .wander_odds = WANDER_SPAWN_ODDS, .running = true};
    rng_seed(&g->rng, seed);

    g->levels = calloc(DUNGEON_DEPTH, sizeof *g->levels);
    g->player_dist = malloc(MAP_WIDTH * MAP_HEIGHT * sizeof *g->player_dist);
    if (!g->levels || !g->player_dist)
    {
        free(g->levels);
        free(g->player_dist);
        return false;
    }
    for (int d = 1; d <= DUNGEON_DEPTH; d++)
        if (!level_generate(&g->levels[d - 1], seed, d, DUNGEON_DEPTH, MAP_WIDTH, MAP_HEIGHT))
        {
            free_levels(g->levels, d - 1);
            free(g->player_dist);
            g->levels = NULL;
            g->player_dist = NULL;
            return false;
        }

    g->player = (Player){.hp = PLAYER_STATS.hp_max,
                         .energy = ACT_COST,
                         .base = PLAYER_STATS,
                         .nutrition = NUTRITION_START};
    item_lore_init(&g->lore, &g->rng);
    static const Item KIT[] = {
        {.cls = ITEM_WEAPON,
         .kind = WEAPON_SHORT_SWORD,
         .count = 1,
         .enchant_known = true,
         .equipped = true},
        {.cls = ITEM_ARMOUR,
         .kind = ARMOUR_LEATHER,
         .count = 1,
         .enchant_known = true,
         .equipped = true},
        {.cls = ITEM_FOOD, .count = 1},
    };
    for (size_t i = 0; i < sizeof KIT / sizeof KIT[0]; i++)
        inv_add(&g->player, KIT[i]);
    msglog_clear(&g->log);
    msglog_add(&g->log, g->turn, "You enter the dungeon. Press ? for help.");
    enter_level(g, 1, g->levels[0].stairs_up);
    return true;
}

void game_free(Game *g)
{
    if (g->levels)
        free_levels(g->levels, DUNGEON_DEPTH);
    free(g->player_dist);
    g->levels = NULL;
    g->player_dist = NULL;
    g->map = NULL;
}

/* ---- combat ---------------------------------------------------------------- */

static void player_attack(Game *g, int i)
{
    Level *lv = game_level(g);
    Monster *m = &lv->monsters[i];
    const MonsterInfo *info = monster_info(m);

    m->awake = true;
    Stats me = game_player_stats(g);
    int dmg = combat_attack(&g->rng, &me, &info->stats);
    if (dmg == 0)
    {
        msglog_add(&g->log, g->turn, "You miss the %s.", info->name);
        return;
    }
    m->hp -= dmg;
    if (m->hp > 0)
        msglog_add(&g->log, g->turn, "You hit the %s.", info->name);
    else
    {
        msglog_add(&g->log, g->turn, "You kill the %s.", info->name);
        level_remove_monster(lv, i);
    }
}

void game_monster_attack(Game *g, Monster *m)
{
    const MonsterInfo *info = monster_info(m);
    Stats me = game_player_stats(g);
    int dmg = combat_attack(&g->rng, &info->stats, &me);
    if (dmg == 0)
    {
        msglog_add(&g->log, g->turn, "The %s misses you.", info->name);
        return;
    }
    g->player.hp -= dmg;
    msglog_add(&g->log, g->turn, "The %s %s you.", info->name, info->verb);
    if (g->player.hp <= 0)
    {
        g->killer = m->kind;
        game_kill_player(g, DEATH_MONSTER);
    }
}

void game_kill_player(Game *g, DeathCause cause)
{
    g->player.hp = 0;
    g->dead = true;
    g->death = cause;
    msglog_add(&g->log, g->turn, "You die.");
}

/* Hunger: -1 per tick, warnings at each stage, starving costs HP. */
static void get_hungrier(Game *g)
{
    Player *p = &g->player;
    Hunger before = game_hunger(g);
    if (p->nutrition > 0)
        p->nutrition--;
    Hunger now = game_hunger(g);
    if (now != before)
        msglog_add(&g->log, g->turn, "%s",
                   now == HUNGER_HUNGRY ? "You are getting hungry."
                   : now == HUNGER_WEAK ? "You feel weak from hunger."
                                        : "You are starving!");
    if (now == HUNGER_STARVING && g->turn % STARVE_TICKS == 0 && --p->hp <= 0)
        game_kill_player(g, DEATH_STARVATION);
}

/* Effects wear off one tick at a time. */
static void wear_off(Game *g, int *ticks, const char *message)
{
    if (*ticks > 0 && --*ticks == 0)
    {
        msglog_add(&g->log, g->turn, "%s", message);
        game_look(g);
    }
}

/* ---- the world ------------------------------------------------------------- */

/* Sometimes a new monster wanders in, out of view and away from the player. */
static void maybe_spawn_wanderer(Game *g)
{
    Level *lv = game_level(g);
    if (g->wander_odds <= 0 || lv->monster_count >= LEVEL_MAX_MONSTERS ||
        rng_below(&g->rng, (uint32_t)g->wander_odds) != 0)
        return;
    for (int tries = 0; tries < 50; tries++)
    {
        Vec2 p = vec2(rng_range(&g->rng, 0, g->map->w - 1), rng_range(&g->rng, 0, g->map->h - 1));
        if (map_get(g->map, p.x, p.y) == TILE_FLOOR && !map_is_visible(g->map, p.x, p.y) &&
            g->player_dist[p.y * g->map->w + p.x] >= WANDER_SPAWN_DIST &&
            level_monster_at(lv, p) < 0)
        {
            Monster m = monster_make(monster_pick_kind(&g->rng, g->depth), p, &g->rng);
            m.awake = true;
            level_add_monster(lv, m);
            return;
        }
    }
}

void game_tick(Game *g)
{
    if (g->dead)
        return;
    g->turn++;
    Player *p = &g->player;
    get_hungrier(g);
    if (g->dead)
        return;
    if (g->turn % REGEN_TURNS == 0 && p->hp < p->base.hp_max && game_hunger(g) < HUNGER_WEAK)
        p->hp++;
    wear_off(g, &p->haste, "You feel yourself slow down.");
    wear_off(g, &p->confused, "You feel less confused.");
    wear_off(g, &p->blind, "You can see again.");

    bool have_dist = path_distances(g->map, p->pos, g->player_dist);
    Level *lv = game_level(g);
    for (int i = 0; i < lv->monster_count && !g->dead; i++)
    {
        Monster *m = &lv->monsters[i];
        m->energy += monster_info(m)->stats.speed;
        while (m->energy >= ACT_COST && !g->dead)
        {
            m->energy -= ACT_COST;
            ai_act(g, m, have_dist ? g->player_dist : NULL);
        }
    }
    if (have_dist)
        maybe_spawn_wanderer(g);
    notice_monsters(g);

    /* Can't bank more than one tick's worth: idle time in real time isn't saved up. */
    int speed = game_player_stats(g).speed;
    int cap = speed > ACT_COST ? speed : ACT_COST;
    p->energy += speed;
    if (p->energy > cap)
        p->energy = cap;
}

bool game_ready(const Game *g)
{
    return !g->dead && g->player.energy >= ACT_COST;
}

void game_set_realtime(Game *g, bool on)
{
    g->realtime = on;
    msglog_add(&g->log, g->turn,
               on ? "Real time: the dungeon moves on its own. T to stop."
                  : "Turn-based: the dungeon waits for you.");
}

/* ---- player actions -------------------------------------------------------- */

static bool try_move(Game *g, Vec2 dir)
{
    if (g->player.confused > 0 && rng_chance(&g->rng, 50))
        dir = MOVE_DIR[rng_range(&g->rng, 0, 7)];
    Vec2 to = vec2_add(g->player.pos, dir);
    int target = level_monster_at(game_level(g), to);
    if (target >= 0)
    {
        player_attack(g, target);
        return true;
    }
    if (!map_walkable(g->map, to.x, to.y))
    {
        msglog_add(&g->log, g->turn, "Rock blocks the way.");
        return false;
    }
    g->player.pos = to;
    inv_step_on(g);
    return true;
}

/* Takes the stairs under the player, if they go `dir` (+1 down, -1 up). */
static bool take_stairs(Game *g, int dir)
{
    Tile here = map_get(g->map, g->player.pos.x, g->player.pos.y);
    if (here != (dir > 0 ? TILE_STAIRS_DOWN : TILE_STAIRS_UP))
    {
        msglog_add(&g->log, g->turn, "There are no stairs %s here.", dir > 0 ? "down" : "up");
        return false;
    }

    int depth = g->depth + dir;
    const Level *to = &g->levels[depth - 1];
    msglog_add(&g->log, g->turn, dir > 0 ? "You go down to depth %d." : "You climb up to depth %d.",
               depth);
    enter_level(g, depth, dir > 0 ? to->stairs_up : to->stairs_down);
    return true;
}

static bool monster_in_view(Game *g)
{
    const Level *lv = game_level(g);
    for (int i = 0; i < lv->monster_count; i++)
        if (map_is_visible(g->map, lv->monsters[i].pos.x, lv->monsters[i].pos.y))
            return true;
    return false;
}

/* Spends the player's action and, turn-based, runs the world until the
 * player can act again. */
static void end_action(Game *g)
{
    g->player.energy -= ACT_COST;
    game_look(g);
    while (!g->realtime && !g->dead && !game_ready(g))
        game_tick(g);
}

/* Waits turn after turn until healed, a monster is in view, or REST_MAX. */
static bool rest(Game *g)
{
    if (monster_in_view(g))
    {
        msglog_add(&g->log, g->turn, "You can't rest with a monster in view.");
        return false;
    }
    if (g->player.hp >= g->player.base.hp_max)
    {
        msglog_add(&g->log, g->turn, "You are already at full health.");
        return false;
    }
    int start = g->turn;
    while (g->player.hp < g->player.base.hp_max && !monster_in_view(g) && !g->dead &&
           g->turn - start < REST_MAX)
        end_action(g);
    if (g->player.hp == g->player.base.hp_max)
        msglog_add(&g->log, g->turn, "You rest and feel better.");
    return true;
}

bool game_step(Game *g, Action a)
{
    return game_step_item(g, a, -1);
}

bool game_step_item(Game *g, Action a, int item)
{
    if (a == ACTION_NONE || g->dead)
        return false;
    g->fresh_serial = msglog_next_serial(&g->log); /* earlier messages are now old */
    if (a == ACTION_QUIT)
    {
        g->running = false;
        return false;
    }
    if (!game_ready(g))
        return false;

    bool used_turn = false;
    if (a >= ACTION_MOVE_N && a <= ACTION_MOVE_NW)
        used_turn = try_move(g, MOVE_DIR[a - ACTION_MOVE_N]);
    else if (a == ACTION_WAIT || (a == ACTION_REST && g->realtime))
        used_turn = true;
    else if (a == ACTION_REST)
        return rest(g); /* acts turn by turn itself */
    else if (a == ACTION_DESCEND || a == ACTION_ASCEND)
        used_turn = take_stairs(g, a == ACTION_DESCEND ? 1 : -1);
    else if (a == ACTION_PICK_UP)
        used_turn = inv_pick_up(g);
    else if (a == ACTION_USE)
        used_turn = inv_use(g, item);
    else if (a == ACTION_DROP)
        used_turn = inv_drop(g, item);

    if (used_turn)
        end_action(g);
    return used_turn;
}
