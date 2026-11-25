/* Format: "RGCS", a version, then every field in a fixed order as little-endian
 * integers (so it doesn't depend on struct layout or byte order), then an
 * FNV-1a checksum of all that; the bytes are stored as base64 text. Loading
 * checks every count, index and position before using it: a damaged or edited
 * save is refused, never read out of bounds. */
#include "core/save.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/path.h"

enum
{
    SAVE_VERSION = 1, /* change when the format or the meaning of a field changes */
    MAX_STAT = 100000,
};

static const char MAGIC[4] = {'R', 'G', 'C', 'S'};

/* ---- bytes ----------------------------------------------------------------- */

typedef struct
{
    uint8_t *data;
    size_t len;
    size_t cap;
    bool ok; /* false once out of memory */
} Writer;

static void put_bytes(Writer *w, const void *src, size_t n)
{
    if (!w->ok)
        return;
    if (w->len + n > w->cap)
    {
        size_t cap = w->cap ? w->cap : 4096;
        while (cap < w->len + n)
            cap *= 2;
        uint8_t *data = realloc(w->data, cap);
        if (!data)
        {
            w->ok = false;
            return;
        }
        w->data = data;
        w->cap = cap;
    }
    memcpy(w->data + w->len, src, n);
    w->len += n;
}

static void put_u32(Writer *w, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    put_bytes(w, b, sizeof b);
}

static void put_int(Writer *w, int v)
{
    put_u32(w, (uint32_t)v);
}

static void put_u64(Writer *w, uint64_t v)
{
    put_u32(w, (uint32_t)v);
    put_u32(w, (uint32_t)(v >> 32));
}

static void put_bool(Writer *w, bool v)
{
    uint8_t b = v ? 1 : 0;
    put_bytes(w, &b, 1);
}

typedef struct
{
    const uint8_t *data;
    size_t len;
    size_t pos;
    bool ok; /* false once something was missing or out of range */
} Reader;

static void get_bytes(Reader *r, void *dst, size_t n)
{
    if (!r->ok || r->len - r->pos < n)
    {
        r->ok = false;
        memset(dst, 0, n);
        return;
    }
    memcpy(dst, r->data + r->pos, n);
    r->pos += n;
}

static uint32_t get_u32(Reader *r)
{
    uint8_t b[4];
    get_bytes(r, b, sizeof b);
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static uint64_t get_u64(Reader *r)
{
    uint64_t lo = get_u32(r);
    return lo | (uint64_t)get_u32(r) << 32;
}

/* An int in [lo, hi]; anything else spoils the read. */
static int get_int(Reader *r, int lo, int hi)
{
    int v = (int)get_u32(r);
    if (v < lo || v > hi)
        r->ok = false;
    return v;
}

static bool get_bool(Reader *r)
{
    uint8_t b;
    get_bytes(r, &b, 1);
    if (b > 1)
        r->ok = false;
    return b == 1;
}

static uint32_t fnv1a(const uint8_t *data, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++)
        h = (h ^ data[i]) * 16777619u;
    return h;
}

/* ---- base64 ---------------------------------------------------------------- */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *base64_encode(const uint8_t *data, size_t n)
{
    char *out = malloc((n + 2) / 3 * 4 + 1);
    if (!out)
        return NULL;
    char *o = out;
    for (size_t i = 0; i < n; i += 3)
    {
        uint32_t v = (uint32_t)data[i] << 16;
        if (i + 1 < n)
            v |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < n)
            v |= data[i + 2];
        *o++ = B64[v >> 18 & 63];
        *o++ = B64[v >> 12 & 63];
        *o++ = i + 1 < n ? B64[v >> 6 & 63] : '=';
        *o++ = i + 2 < n ? B64[v & 63] : '=';
    }
    *o = '\0';
    return out;
}

/* Decodes into a malloc'd buffer; NULL unless the text is canonical base64
 * (so no two texts decode the same). Line breaks (a file editor may add one)
 * are skipped. */
static uint8_t *base64_decode(const char *text, size_t *n)
{
    signed char value[256];
    memset(value, -1, sizeof value);
    for (int i = 0; i < 64; i++)
        value[(unsigned char)B64[i]] = (signed char)i;

    uint8_t *out = malloc(strlen(text) / 4 * 3 + 3);
    if (!out)
        return NULL;
    uint32_t v = 0;
    int bits = 0;
    size_t chars = 0, pad = 0;
    *n = 0;
    for (const char *c = text; *c; c++)
    {
        int d = value[(unsigned char)*c];
        if (*c == '\n' || *c == '\r')
            continue;
        if (*c == '=')
            pad++;
        else if (d < 0 || pad > 0) /* a bad character, or data after the padding */
            pad = 3;
        else
        {
            chars++;
            v = v << 6 | (uint32_t)d;
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                out[(*n)++] = (uint8_t)(v >> bits);
            }
        }
    }
    if ((chars + pad) % 4 != 0 || pad > 2 || (v & ((1u << bits) - 1)) != 0)
    {
        free(out);
        return NULL;
    }
    return out;
}

/* ---- the game -------------------------------------------------------------- */

static void put_vec(Writer *w, Vec2 v)
{
    put_int(w, v.x);
    put_int(w, v.y);
}

/* A position on the map, or (-1, -1) if `none_ok`. */
static Vec2 get_pos(Reader *r, bool none_ok)
{
    Vec2 v;
    v.x = get_int(r, -1, MAP_WIDTH - 1);
    v.y = get_int(r, -1, MAP_HEIGHT - 1);
    bool none = v.x == -1 && v.y == -1;
    if ((v.x < 0 || v.y < 0) && !(none && none_ok))
        r->ok = false;
    return v;
}

static void put_stats(Writer *w, const Stats *s)
{
    put_int(w, s->hp_max);
    put_int(w, s->to_hit);
    put_int(w, s->defense);
    put_int(w, s->dmg_min);
    put_int(w, s->dmg_max);
    put_int(w, s->speed);
}

static Stats get_stats(Reader *r)
{
    Stats s;
    s.hp_max = get_int(r, 1, MAX_STAT);
    s.to_hit = get_int(r, -MAX_STAT, MAX_STAT);
    s.defense = get_int(r, -MAX_STAT, MAX_STAT);
    s.dmg_min = get_int(r, 0, MAX_STAT);
    s.dmg_max = get_int(r, s.dmg_min, MAX_STAT);
    s.speed = get_int(r, 1, 1000); /* 0 would never act */
    return s;
}

static void put_item(Writer *w, const Item *it)
{
    put_int(w, (int)it->cls);
    put_int(w, it->kind);
    put_int(w, it->count);
    put_int(w, it->enchant);
    put_bool(w, it->enchant_known);
    put_bool(w, it->equipped);
}

static Item get_item(Reader *r)
{
    static const int KINDS[ITEM_CLASS_COUNT] = {
        [ITEM_GOLD] = 1,
        [ITEM_FOOD] = 1,
        [ITEM_POTION] = POTION_COUNT,
        [ITEM_SCROLL] = SCROLL_COUNT,
        [ITEM_WEAPON] = WEAPON_COUNT,
        [ITEM_ARMOUR] = ARMOUR_COUNT,
        [ITEM_GOAL] = 1,
    };
    Item it;
    it.cls = (ItemClass)get_int(r, 0, ITEM_CLASS_COUNT - 1);
    it.kind = get_int(r, 0, r->ok ? KINDS[it.cls] - 1 : 0);
    it.count = get_int(r, 1, MAX_STAT);
    it.enchant = get_int(r, -100, 100);
    it.enchant_known = get_bool(r);
    it.equipped = get_bool(r);
    return it;
}

static void put_monster(Writer *w, const Monster *m)
{
    put_int(w, (int)m->kind);
    put_vec(w, m->pos);
    put_int(w, m->hp);
    put_int(w, m->energy);
    put_bool(w, m->awake);
    put_bool(w, m->seen);
    put_int(w, m->track);
    put_vec(w, m->heading);
}

static Monster get_monster(Reader *r)
{
    Monster m;
    m.kind = (MonsterKind)get_int(r, 0, MON_COUNT - 1);
    m.pos = get_pos(r, false);
    m.hp = get_int(r, 1, MAX_STAT);
    m.energy = get_int(r, -MAX_STAT, MAX_STAT);
    m.awake = get_bool(r);
    m.seen = get_bool(r);
    m.track = get_int(r, 0, MAX_STAT);
    m.heading.x = get_int(r, -1, 1);
    m.heading.y = get_int(r, -1, 1);
    return m;
}

static void put_level(Writer *w, const Level *lv)
{
    const Map *m = &lv->map;
    size_t n = (size_t)m->w * (size_t)m->h;
    put_int(w, m->w);
    put_int(w, m->h);
    put_bytes(w, m->tiles, n);
    for (size_t i = 0; i < n; i += 8) /* explored: one bit per cell */
    {
        uint8_t bits = 0;
        for (size_t b = 0; b < 8 && i + b < n; b++)
            if (m->explored[i + b])
                bits |= (uint8_t)(1u << b);
        put_bytes(w, &bits, 1);
    }
    put_vec(w, lv->stairs_up);
    put_vec(w, lv->stairs_down);
    put_bool(w, lv->caves);
    put_int(w, lv->monster_count);
    for (int i = 0; i < lv->monster_count; i++)
        put_monster(w, &lv->monsters[i]);
    put_int(w, lv->item_count);
    for (int i = 0; i < lv->item_count; i++)
    {
        put_item(w, &lv->items[i].item);
        put_vec(w, lv->items[i].pos);
        put_bool(w, lv->items[i].seen);
    }
}

/* Into a level whose map is allocated (MAP_WIDTH x MAP_HEIGHT). */
static void get_level(Reader *r, Level *lv)
{
    Map *m = &lv->map;
    size_t n = (size_t)m->w * (size_t)m->h;
    get_int(r, MAP_WIDTH, MAP_WIDTH);
    get_int(r, MAP_HEIGHT, MAP_HEIGHT);
    get_bytes(r, m->tiles, n);
    for (size_t i = 0; i < n && r->ok; i++)
        if (m->tiles[i] >= TILE_COUNT)
            r->ok = false;
    for (size_t i = 0; i < n; i += 8)
    {
        uint8_t bits;
        get_bytes(r, &bits, 1);
        for (size_t b = 0; b < 8 && i + b < n; b++)
            m->explored[i + b] = bits >> b & 1;
    }
    lv->stairs_up = get_pos(r, false);
    lv->stairs_down = get_pos(r, true);
    lv->caves = get_bool(r);
    lv->monster_count = get_int(r, 0, LEVEL_MAX_MONSTERS);
    for (int i = 0; i < lv->monster_count && r->ok; i++)
        lv->monsters[i] = get_monster(r);
    lv->item_count = get_int(r, 0, LEVEL_MAX_ITEMS);
    for (int i = 0; i < lv->item_count && r->ok; i++)
    {
        lv->items[i].item = get_item(r);
        lv->items[i].pos = get_pos(r, false);
        lv->items[i].seen = get_bool(r);
    }
}

static void put_game(Writer *w, const Game *g)
{
    put_bytes(w, MAGIC, sizeof MAGIC);
    put_u32(w, SAVE_VERSION);
    put_u64(w, g->seed);
    put_u64(w, g->rng.state);
    put_u64(w, g->rng.inc);
    put_int(w, g->depth);
    put_int(w, g->wander_odds);
    put_int(w, g->turn);
    put_int(w, g->fov_radius);
    put_bool(w, g->realtime);
    put_int(w, g->deepest);

    const Player *p = &g->player;
    put_vec(w, p->pos);
    put_int(w, p->hp);
    put_int(w, p->energy);
    put_stats(w, &p->base);
    put_int(w, p->gold);
    put_int(w, p->level);
    put_int(w, p->xp);
    put_int(w, p->nutrition);
    put_int(w, p->haste);
    put_int(w, p->confused);
    put_int(w, p->blind);
    put_int(w, p->pack_count);
    for (int i = 0; i < p->pack_count; i++)
        put_item(w, &p->pack[i]);

    put_bytes(w, g->lore.potion_look, sizeof g->lore.potion_look);
    put_bytes(w, g->lore.scroll_title, sizeof g->lore.scroll_title);
    for (int k = 0; k < POTION_COUNT; k++)
        put_bool(w, g->lore.potion_known[k]);
    for (int k = 0; k < SCROLL_COUNT; k++)
        put_bool(w, g->lore.scroll_known[k]);

    put_int(w, msglog_len(&g->log));
    put_int(w, msglog_next_serial(&g->log));
    put_int(w, g->fresh_serial);
    for (int age = msglog_len(&g->log) - 1; age >= 0; age--) /* oldest first */
    {
        const Message *m = msglog_get(&g->log, age);
        put_bytes(w, m->text, sizeof m->text);
        put_int(w, m->turn);
        put_int(w, m->count);
        put_int(w, m->serial);
    }

    for (int d = 0; d < DUNGEON_DEPTH; d++)
        put_level(w, &g->levels[d]);
}

/* Into a Game whose levels and player_dist are allocated. */
static void get_game(Reader *r, Game *g)
{
    char magic[sizeof MAGIC];
    get_bytes(r, magic, sizeof magic);
    if (memcmp(magic, MAGIC, sizeof magic) != 0 || get_u32(r) != SAVE_VERSION)
        r->ok = false;
    g->seed = get_u64(r);
    g->rng.state = get_u64(r);
    g->rng.inc = get_u64(r);
    g->depth = get_int(r, 1, DUNGEON_DEPTH);
    g->wander_odds = get_int(r, 0, MAX_STAT);
    g->turn = get_int(r, 0, INT32_MAX);
    g->fov_radius = get_int(r, 1, 50);
    g->realtime = get_bool(r);
    g->deepest = get_int(r, g->depth, DUNGEON_DEPTH);

    Player *p = &g->player;
    p->pos = get_pos(r, false);
    p->hp = get_int(r, 1, MAX_STAT);
    p->energy = get_int(r, -MAX_STAT, MAX_STAT);
    p->base = get_stats(r);
    p->gold = get_int(r, 0, INT32_MAX);
    p->level = get_int(r, 1, MAX_LEVEL);
    p->xp = get_int(r, 0, INT32_MAX);
    p->nutrition = get_int(r, 0, NUTRITION_MAX);
    p->haste = get_int(r, 0, MAX_STAT);
    p->confused = get_int(r, 0, MAX_STAT);
    p->blind = get_int(r, 0, MAX_STAT);
    p->pack_count = get_int(r, 0, PACK_SIZE);
    for (int i = 0; i < p->pack_count && r->ok; i++)
        p->pack[i] = get_item(r);

    get_bytes(r, g->lore.potion_look, sizeof g->lore.potion_look);
    for (int k = 0; k < POTION_COUNT; k++)
        if (g->lore.potion_look[k] >= POTION_LOOK_COUNT)
            r->ok = false;
    get_bytes(r, g->lore.scroll_title, sizeof g->lore.scroll_title);
    for (int k = 0; k < SCROLL_COUNT; k++)
        g->lore.scroll_title[k][TITLE_SIZE - 1] = '\0';
    for (int k = 0; k < POTION_COUNT; k++)
        g->lore.potion_known[k] = get_bool(r);
    for (int k = 0; k < SCROLL_COUNT; k++)
        g->lore.scroll_known[k] = get_bool(r);

    MsgLog *log = &g->log;
    log->first = 0;
    log->len = get_int(r, 0, MSGLOG_CAPACITY);
    log->next_serial = get_int(r, 0, INT32_MAX);
    g->fresh_serial = get_int(r, 0, INT32_MAX);
    for (int i = 0; i < log->len && r->ok; i++)
    {
        Message *m = &log->msgs[i];
        get_bytes(r, m->text, sizeof m->text);
        m->text[MSG_TEXT_SIZE - 1] = '\0';
        m->turn = get_int(r, 0, INT32_MAX);
        m->count = get_int(r, 1, INT32_MAX);
        m->serial = get_int(r, 0, INT32_MAX);
    }

    for (int d = 0; d < DUNGEON_DEPTH && r->ok; d++)
        get_level(r, &g->levels[d]);
}

char *game_save(const Game *g)
{
    if (game_over(g) || !g->levels)
        return NULL;
    Writer w = {.ok = true};
    put_game(&w, g);
    put_u32(&w, fnv1a(w.data, w.len));
    char *text = w.ok ? base64_encode(w.data, w.len) : NULL;
    free(w.data);
    return text;
}

/* Frees what game_load allocated (levels come zeroed: map_free is safe on all). */
static void free_loaded(Game *g)
{
    if (g->levels)
        for (int d = 0; d < DUNGEON_DEPTH; d++)
            map_free(&g->levels[d].map);
    free(g->levels);
    free(g->player_dist);
}

bool game_load(Game *g, const char *text)
{
    size_t n;
    uint8_t *data = base64_decode(text, &n);
    if (!data)
        return false;
    Reader sum = {.data = data, .len = n, .pos = n >= 4 ? n - 4 : 0, .ok = n >= 4};
    if (get_u32(&sum) != fnv1a(data, sum.ok ? n - 4 : 0) || !sum.ok)
    {
        free(data);
        return false;
    }

    Game t = {.running = true};
    t.levels = calloc(DUNGEON_DEPTH, sizeof *t.levels);
    t.player_dist = malloc(MAP_WIDTH * MAP_HEIGHT * sizeof *t.player_dist);
    bool ok = t.levels && t.player_dist;
    for (int d = 0; ok && d < DUNGEON_DEPTH; d++)
        ok = map_init(&t.levels[d].map, MAP_WIDTH, MAP_HEIGHT);
    if (ok)
    {
        Reader r = {.data = data, .len = n - 4, .ok = true};
        get_game(&r, &t);
        ok = r.ok && r.pos == r.len; /* all of it, and nothing more */
    }
    free(data);
    if (!ok)
    {
        free_loaded(&t);
        return false;
    }

    t.map = &t.levels[t.depth - 1].map;
    if (!map_walkable(t.map, t.player.pos.x, t.player.pos.y))
    {
        free_loaded(&t);
        return false;
    }
    path_distances(t.map, t.player.pos, t.player_dist);
    *g = t;
    game_look(g);
    msglog_add(&g->log, g->turn, "Welcome back. You are on depth %d.", g->depth);
    g->fresh_serial = msglog_next_serial(&g->log) - 1;
    return true;
}
