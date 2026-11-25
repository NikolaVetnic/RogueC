#include "core/game.h"

#include <stdlib.h>

#include "core/ai.h"
#include "core/fov.h"
#include "core/path.h"

enum
{
    MAP_WIDTH = 100,
    MAP_HEIGHT = 42,
    FOV_RADIUS = 10,
    WANDER_SPAWN_ODDS = 300, /* default Game.wander_odds */
    WANDER_SPAWN_DIST = 15,  /* ... at least this many steps away, out of view */
};

static const Stats PLAYER_STATS = {
    .hp_max = 20, .to_hit = 2, .defense = 1, .dmg_min = 2, .dmg_max = 5, .speed = 100};

/* Indexed by action - ACTION_MOVE_N, clockwise from north. */
static const Vec2 MOVE_DIR[8] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
};

Level *game_level(Game *g)
{
    return &g->levels[g->depth - 1];
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

static void look(Game *g)
{
    fov_compute(g->map, g->player.pos, g->fov_radius);
    notice_monsters(g);
}

/* Moves the player to `pos` on level `depth` and looks around. */
static void enter_level(Game *g, int depth, Vec2 pos)
{
    g->depth = depth;
    g->map = &g->levels[depth - 1].map;
    g->player.pos = pos;
    for (int i = 0; i < game_level(g)->monster_count; i++)
        game_level(g)->monsters[i].seen = false; /* announce them again */
    look(g);
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

    g->player = (Player){.hp = PLAYER_STATS.hp_max, .energy = ACT_COST, .stats = PLAYER_STATS};
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
    int dmg = combat_attack(&g->rng, &g->player.stats, &info->stats);
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
    int dmg = combat_attack(&g->rng, &info->stats, &g->player.stats);
    if (dmg == 0)
    {
        msglog_add(&g->log, g->turn, "The %s misses you.", info->name);
        return;
    }
    g->player.hp -= dmg;
    msglog_add(&g->log, g->turn, "The %s %s you.", info->name, info->verb);
    if (g->player.hp <= 0)
    {
        g->player.hp = 0;
        g->dead = true;
        g->killer = m->kind;
        msglog_add(&g->log, g->turn, "You die.");
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
    if (g->turn % REGEN_TURNS == 0 && p->hp < p->stats.hp_max)
        p->hp++;

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
    int cap = p->stats.speed > ACT_COST ? p->stats.speed : ACT_COST;
    p->energy += p->stats.speed;
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
    look(g);
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
    if (g->player.hp >= g->player.stats.hp_max)
    {
        msglog_add(&g->log, g->turn, "You are already at full health.");
        return false;
    }
    int start = g->turn;
    while (g->player.hp < g->player.stats.hp_max && !monster_in_view(g) && !g->dead &&
           g->turn - start < REST_MAX)
        end_action(g);
    if (g->player.hp == g->player.stats.hp_max)
        msglog_add(&g->log, g->turn, "You rest and feel better.");
    return true;
}

bool game_step(Game *g, Action a)
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

    if (used_turn)
        end_action(g);
    return used_turn;
}
