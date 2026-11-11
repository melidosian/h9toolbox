/* MAZE CHASE: an original maze game for the TD-H9. Eat every dot while four
 * prowlers hunt you; a power pellet turns them blue for a few seconds and
 * lets you eat them. Four lives, faster prowlers every level.
 *
 * Keys: 2 / 4 / 6 / 8 (or Up / Down) steer, Menu pauses, Back leaves.
 * The 25 x 19 maze is built from 6-pixel tiles (the maze fills the screen
 * below a 14-pixel score line); everything lives on the stack, and time is
 * the loader's 100 Hz tick. */
#include <stdint.h>

#include "gfx.h"
#include "h9mod.h"
#include "snd.h"

#ifndef TICK
#define TICK (*(volatile uint32_t *)0x1d8b0)
#endif
enum { W = 25, H = 19, T = 6, OX = 5, OY = 14, GHOSTS = 4 };
enum { C_WALL = 0x5c1f, C_DOT = 0xffb6, C_DOOR = 0xfb56, C_FRIGHT = 0x001f, C_PLAYER = 0x07e0, C_ORANGE = 0xfd20,
       C_PINK = 0xf81f, C_CYAN = 0x07ff };

static const char LEVEL[H][W + 1] = {
    "#########################",
    "#.......................#",
    "#####.#.###.#.###.#.#####",
    "#.......#...#...#.......#",
    "#.#.#.#.#.#.#.#.#.#.#.#.#",
    "#.#.#...#.#...#.#...#.#.#",
    "#.#.#.###.#.#.#.###.#.#.#",
    "#.....#...........#.....#",
    "#.#.#.#.####D####.#.#.#.#",
    "         .#HHH#.         ",
    "#.#####.#.#####.#.#####.#",
    "#.......................#",
    "#.#.#.###.#####.###.#.#.#",
    "#...#...#.......#...#...#",
    "###.###.###.#.###.###.###",
    "#.......#...#...#.......#",
    "#.###.###.#.#.#.###.###.#",
    "#.........#...#.........#",
    "#########################",
};
static const int8_t DX[4] = { 0, -1, 0, 1 }, DY[4] = { -1, 0, 1, 0 };   /* up, left, down, right */
static const uint8_t PLAYER[6] = { 0x1e, 0x3f, 0x3f, 0x3f, 0x3f, 0x1e };
static const uint8_t GHOST[6] = { 0x1e, 0x3f, 0x2d, 0x3f, 0x3f, 0x2d };
static const uint16_t GHOST_COLOR[GHOSTS] = { GFX_RED, C_PINK, C_CYAN, C_ORANGE };
static const uint8_t CORNER[GHOSTS][2] = { { 23, 1 }, { 1, 1 }, { 1, 17 }, { 23, 17 } };

struct ghost {
    int8_t x, y, dir;
    uint8_t out, fright;      /* out of the house; frightened (blue) */
    uint32_t release, next;   /* tick to leave the house, tick of the next step */
};

struct game {
    char map[H][W + 1];
    int8_t px, py, dir, want;
    uint8_t mouth, lives, level, combo;
    unsigned dots;
    uint32_t score, hi, next_p, fright_end, start;
    uint32_t rng;
    uint8_t sound, waka;
    struct snd snd;
    struct ghost g[GHOSTS];
};

static unsigned rnd(struct game *m)
{
    m->rng ^= m->rng << 13; m->rng ^= m->rng >> 17; m->rng ^= m->rng << 5;
    return m->rng;
}
/* ---- sound effects (square waves; see snd.c). Frequencies are Hz, lengths in 10 ms. ---- */
enum { FX_WAKA, FX_PELLET, FX_GHOST, FX_START, FX_CLEAR, FX_DEATH, FX_OVER };
static void fx(struct game *m, unsigned id)
{
    if (!m->sound) return;
    struct snd *s = &m->snd;
    switch (id) {
    case FX_WAKA:                                    /* a dot: two pitches, alternating */
        snd_begin(s, 1);
        if (m->waka++ & 1) snd_sweep(s, 280, 520, 6); else snd_sweep(s, 520, 280, 6);
        break;
    case FX_PELLET: snd_begin(s, 4); snd_sweep(s, 150, 900, 14); snd_note(s, 900, 4); break;
    case FX_GHOST: snd_begin(s, 5); snd_sweep(s, 400, 1800, 22); break;
    case FX_START:
        snd_begin(s, 9);
        snd_note(s, 392, 8); snd_note(s, 523, 8); snd_note(s, 659, 8); snd_note(s, 784, 8);
        snd_note(s, 659, 8); snd_note(s, 784, 16);
        break;
    case FX_CLEAR:
        snd_begin(s, 7);
        snd_note(s, 523, 8); snd_note(s, 659, 8); snd_note(s, 784, 8); snd_note(s, 1047, 8);
        snd_note(s, 784, 8); snd_note(s, 1047, 20);
        break;
    case FX_DEATH: snd_begin(s, 8); snd_sweep(s, 800, 120, 60); snd_note(s, 100, 6); break;
    default: snd_begin(s, 8); snd_note(s, 392, 20); snd_note(s, 330, 20); snd_note(s, 262, 40); break;
    }
}
static void pump(struct game *m) { if (m->sound) snd_pump(&m->snd); }

static int wrap(int x) { return x < 0 ? x + W : x >= W ? x - W : x; }
static int is_wall(const struct game *m, int x, int y)
{
    return x < 0 || x >= W || y < 0 || y >= H || m->map[y][x] == '#';
}
static int player_ok(const struct game *m, int x, int y)
{
    char c = m->map[y][x];
    return c == '.' || c == 'o' || c == ' ';
}
static int ghost_ok(const struct game *m, const struct ghost *g, int x, int y)
{
    char c = m->map[y][x];
    return c == '.' || c == 'o' || c == ' ' || (!g->out && (c == 'H' || c == 'D'));
}

/* ---- drawing ---- */
static uint16_t base_pixel(const struct game *m, int x, int y, int px, int py)
{
    char c = m->map[y][x];
    if (c == '#')
        return ((py == 0 && !is_wall(m, x, y - 1)) || (py == 5 && !is_wall(m, x, y + 1)) ||
                (px == 0 && !is_wall(m, x - 1, y)) || (px == 5 && !is_wall(m, x + 1, y))) ? C_WALL : 0;
    if (c == '.') return px >= 2 && px <= 3 && py >= 2 && py <= 3 ? C_DOT : 0;
    if (c == 'o') return px >= 1 && px <= 4 && py >= 1 && py <= 4 && !((px == 1 || px == 4) && (py == 1 || py == 4)) ? GFX_WHITE : 0;
    if (c == 'D') return py == 2 || py == 3 ? C_DOOR : 0;
    return 0;
}

static uint16_t ghost_color(const struct game *m, const struct ghost *g, uint32_t t)
{
    if (!g->fright) return GHOST_COLOR[g - m->g];
    return m->fright_end - t < 150 && ((t / 20) & 1) ? GFX_WHITE : C_FRIGHT;
}

static void draw_tile(const struct game *m, int x, int y, uint32_t t)
{
    uint8_t buffer[T * T * 2];
    for (int py = 0; py < T; py++)
        for (int px = 0; px < T; px++) {
            uint16_t c = base_pixel(m, x, y, px, py);
            for (unsigned i = 0; i < GHOSTS; i++) {
                const struct ghost *g = &m->g[i];
                if (g->x == x && g->y == y && (GHOST[py] >> (5 - px)) & 1) c = ghost_color(m, g, t);
            }
            if (m->px == x && m->py == y && (PLAYER[py] >> (5 - px)) & 1) {
                int open = m->mouth & 1, hole = 0;
                if (open) {
                    int mid = py == 2 || py == 3, vmid = px == 2 || px == 3;
                    hole = m->dir == 3 ? (px >= 3 && mid) : m->dir == 1 ? (px <= 2 && mid) :
                           m->dir == 0 ? (py <= 2 && vmid) : (py >= 3 && vmid);
                }
                if (!hole) c = C_PLAYER;
            }
            buffer[(py * T + px) * 2] = (uint8_t)(c >> 8);
            buffer[(py * T + px) * 2 + 1] = (uint8_t)c;
        }
    unsigned sx = OX + x * T, sy = OY + y * T;
    h9_lcd_window(sx, sy, sx + T - 1, sy + T - 1);
    h9_lcd_write(buffer, sizeof buffer);
}

static void draw_maze(const struct game *m)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) draw_tile(m, x, y, TICK);
}

static char *put_num(char *d, uint32_t v, unsigned width)
{
    char t[10];
    unsigned n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10);
    while (n < width) t[n++] = '0';
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}

static void hud(const struct game *m)
{
    char b[24], *d = b;
    const char *s = "SCORE ";
    while (*s) *d++ = *s++;
    d = put_num(d, m->score, 6);
    gfx_text(2, 3, b, 12, 1, GFX_WHITE, GFX_BLACK);
    d = b; *d++ = 'L'; d = put_num(d, m->level, 1);
    gfx_text(88, 3, b, 4, 1, GFX_GREY, GFX_BLACK);
    gfx_fill(116, 2, 42, 10, GFX_BLACK);
    for (unsigned i = 0; i < m->lives; i++) gfx_fill(120 + i * 9, 4, 6, 6, C_PLAYER);
}

static void banner(const char *text, uint16_t color)
{
    unsigned n = 0;
    while (text[n]) n++;
    unsigned w = n * 6 + 8, x = (H9_WIDTH - w) / 2;
    gfx_fill(x, OY + 9 * T + 14, w, 12, GFX_BLACK);
    gfx_text(x + 4, OY + 9 * T + 16, text, 20, 1, color, GFX_BLACK);
}

/* ---- game ---- */
static void place(struct game *m, uint32_t t)
{
    m->px = 12; m->py = 13; m->dir = 3; m->want = 3; m->mouth = 0;
    for (unsigned i = 0; i < GHOSTS; i++) {
        struct ghost *g = &m->g[i];
        g->x = (int8_t)(i == 0 ? 12 : 10 + i); g->y = (int8_t)(i == 0 ? 7 : 9);
        g->dir = (int8_t)(i == 0 ? 1 : i == 1 ? 3 : 1);
        g->out = i == 0; g->fright = 0;
        g->release = t + i * 220u; g->next = t + 60;
    }
    m->fright_end = 0; m->combo = 0; m->next_p = t + 60; m->start = t;
}

static void new_level(struct game *m)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) m->map[y][x] = LEVEL[y][x];
    m->dots = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) if (m->map[y][x] == '.') m->dots++;
    for (unsigned i = 0; i < GHOSTS; i++) {
        m->map[CORNER[i][1]][CORNER[i][0]] = 'o';   /* a power pellet in each corner */
    }
}

static unsigned ghost_period(const struct game *m, const struct ghost *g)
{
    unsigned p = m->level > 6 ? 11 : 17 - m->level;
    return g->fright ? 26 : p;
}

static void ghost_step(struct game *m, struct ghost *g, uint32_t t)
{
    int idx = (int)(g - m->g), tx, ty;
    if (!g->out) {
        if (t < g->release) return;
        tx = 12; ty = 7;
    } else if (g->fright) {
        tx = ty = 0;
    } else if (((t - m->start) % 2700u) < 700u) {
        tx = CORNER[idx][0]; ty = CORNER[idx][1];            /* scatter to the corner */
    } else if (idx == 1) {
        tx = m->px + 4 * DX[m->dir]; ty = m->py + 4 * DY[m->dir];   /* ambush ahead */
    } else if (idx == 2) {
        int dx = m->px - g->x, dy = m->py - g->y;
        tx = dx * dx + dy * dy > 64 ? m->px : CORNER[idx][0];       /* hunt from afar, retreat when close */
        ty = dx * dx + dy * dy > 64 ? m->py : CORNER[idx][1];
    } else if (idx == 3 && (rnd(m) & 3) == 0) {
        tx = (int)(rnd(m) % W); ty = (int)(rnd(m) % H);     /* now and then a random wander */
    } else {
        tx = m->px; ty = m->py;
    }
    int best = -1, best_d = 0, count = 0, choices[4];
    for (int d = 0; d < 4; d++) {
        if (d == ((g->dir + 2) & 3)) continue;               /* no turning back */
        int nx = wrap(g->x + DX[d]), ny = g->y + DY[d];
        if (ny < 0 || ny >= H || !ghost_ok(m, g, nx, ny)) continue;
        int ex = nx - tx, ey = ny - ty, dist = ex * ex + ey * ey;
        choices[count++] = d;
        if (best < 0 || dist < best_d) { best = d; best_d = dist; }
    }
    if (!count) best = (g->dir + 2) & 3;                    /* a dead end: turn round */
    else if (g->fright) best = choices[rnd(m) % (unsigned)count];
    int ox = g->x, oy = g->y;
    g->dir = (int8_t)best;
    g->x = (int8_t)wrap(g->x + DX[best]); g->y = (int8_t)(g->y + DY[best]);
    if (!g->out && g->y <= 7) g->out = 1;
    draw_tile(m, ox, oy, t);
    draw_tile(m, g->x, g->y, t);
}

/* Returns 1 if the player lost a life. */
static int collide(struct game *m, uint32_t t)
{
    for (unsigned i = 0; i < GHOSTS; i++) {
        struct ghost *g = &m->g[i];
        if (g->x != m->px || g->y != m->py) continue;
        if (!g->fright) return 1;
        m->score += 200u << m->combo;
        if (m->combo < 3) m->combo++;
        g->fright = 0; g->out = 0; g->x = 12; g->y = 9; g->dir = 0;
        fx(m, FX_GHOST);
        g->release = t + 120; g->next = t + 40;
        draw_tile(m, m->px, m->py, t);
        hud(m);
    }
    return 0;
}

static void player_step(struct game *m, uint32_t t)
{
    int d = m->want, nx = wrap(m->px + DX[d]), ny = m->py + DY[d];
    if (ny < 0 || ny >= H || !player_ok(m, nx, ny)) {
        d = m->dir; nx = wrap(m->px + DX[d]); ny = m->py + DY[d];
        if (ny < 0 || ny >= H || !player_ok(m, nx, ny)) return;
    }
    int ox = m->px, oy = m->py;
    m->dir = (int8_t)d; m->px = (int8_t)nx; m->py = (int8_t)ny; m->mouth++;
    char c = m->map[ny][nx];
    if (c == '.' || c == 'o') {
        m->map[ny][nx] = ' ';
        m->dots--;
        m->score += c == 'o' ? 50 : 10;
        fx(m, c == 'o' ? FX_PELLET : FX_WAKA);
        if (m->score > m->hi) m->hi = m->score;
        if (c == 'o') {
            unsigned len = m->level > 6 ? 250 : 700 - m->level * 70u;
            m->fright_end = t + len; m->combo = 0;
            for (unsigned i = 0; i < GHOSTS; i++) if (m->g[i].out) { m->g[i].fright = 1; m->g[i].dir = (int8_t)((m->g[i].dir + 2) & 3); }
        }
        hud(m);
    }
    draw_tile(m, ox, oy, t);
    draw_tile(m, nx, ny, t);
}

static unsigned read_dir(unsigned key)
{
    switch (key) {
    case H9_KEY_2: case H9_KEY_UP: return 0;
    case H9_KEY_4: return 1;
    case H9_KEY_5 + 3: case H9_KEY_DOWN: return 2;    /* the 8 key */
    case H9_KEY_5 + 1: return 3;                      /* the 6 key */
    default: return 9;
    }
}

static void pause_ticks(struct game *m, unsigned n)
{
    uint32_t end = TICK + n;
    while ((int32_t)(TICK - end) < 0) { pump(m); h9_delay(1); }
}

static void release(struct game *m)
{
    while (h9_key() != H9_KEY_NONE) { pump(m); h9_delay(2); }
}

static unsigned wait_key(struct game *m)
{
    unsigned last = h9_key();
    for (;;) {
        pump(m);
        h9_delay(2);
        unsigned key = h9_key();
        if (key != last && key != H9_KEY_NONE) return key;
        last = key;
    }
}

/* Draw the board with a banner, hold it a moment, then put everyone at their start. */
static void begin_round(struct game *m, const char *text, uint16_t color)
{
    gfx_fill(0, OY, H9_WIDTH, H9_HEIGHT - OY, GFX_BLACK);
    place(m, TICK);
    draw_maze(m); hud(m);
    banner(text, color);
    fx(m, FX_START);
    pause_ticks(m, 150);
    place(m, TICK);
    draw_maze(m);
}

static void title(const struct game *m)
{
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, GFX_BLACK);
    gfx_text(32, 36, "MAZE CHASE", 12, 2, C_PLAYER, GFX_BLACK);
    gfx_text(12, 64, "2 4 6 8 OR UP/DOWN STEER", 26, 1, GFX_WHITE, GFX_BLACK);
    gfx_text(12, 76, "MENU PAUSE  BACK EXIT", 26, 1, GFX_WHITE, GFX_BLACK);
    gfx_text(12, 88, m->sound ? "5 SOUND: ON " : "5 SOUND: OFF", 26, 1, GFX_WHITE, GFX_BLACK);
    gfx_text(12, 104, "ANY OTHER KEY STARTS", 26, 1, GFX_GREY, GFX_BLACK);
}

/* Plays games until Back. */
static void run(struct game *m)
{
    for (;;) {                                   /* one game */
        m->score = 0; m->lives = 3; m->level = 1;
        gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, GFX_BLACK);
        new_level(m);
        begin_round(m, "READY", GFX_YELLOW);
        unsigned last = H9_KEY_NONE;
        int over = 0;
        while (!over) {
            uint32_t t = TICK;
            unsigned key = h9_key();
            if (key == H9_KEY_BACK && last != key) return;
            if (key == H9_KEY_MENU && last != key) {                 /* pause */
                banner("PAUSED", GFX_WHITE);
                release(m);
                for (;;) { unsigned k = wait_key(m); if (k == H9_KEY_MENU) break; if (k == H9_KEY_BACK) return; }
                release(m);
                draw_maze(m);
                uint32_t gap = TICK - t;                  /* time stood still */
                m->next_p += gap; m->fright_end += m->fright_end ? gap : 0; m->start += gap;
                for (unsigned i = 0; i < GHOSTS; i++) { m->g[i].next += gap; m->g[i].release += gap; }
                t = TICK; key = H9_KEY_NONE;
            }
            last = key;
            unsigned d = read_dir(key);
            if (d < 4) m->want = (int8_t)d;

            if (t >= m->next_p) { player_step(m, t); m->next_p = t + 14; }
            if (m->fright_end && t >= m->fright_end) {
                m->fright_end = 0;
                for (unsigned i = 0; i < GHOSTS; i++) m->g[i].fright = 0;
            }
            for (unsigned i = 0; i < GHOSTS; i++)
                if (t >= m->g[i].next) { ghost_step(m, &m->g[i], t); m->g[i].next = t + ghost_period(m, &m->g[i]); }
            if (collide(m, t)) {
                fx(m, FX_DEATH);
                pause_ticks(m, 80);
                if (--m->lives == 0) { over = 1; break; }
                begin_round(m, "READY", GFX_YELLOW);
                continue;
            }
            if (!m->dots) {
                m->level++;
                banner("LEVEL CLEAR", C_PLAYER);
                fx(m, FX_CLEAR);
                pause_ticks(m, 150);
                new_level(m);
                begin_round(m, "READY", GFX_YELLOW);
                continue;
            }
            pump(m);
            h9_delay(1);
        }
        char b[24], *d = b;
        const char *s = "BEST ";
        while (*s) *d++ = *s++;
        put_num(d, m->hi, 6);
        banner("GAME OVER", GFX_RED);
        gfx_text(40, OY + 9 * T + 32, b, 14, 1, GFX_WHITE, GFX_BLACK);
        gfx_text(24, OY + 9 * T + 44, "MENU AGAIN  BACK EXIT", 22, 1, GFX_GREY, GFX_BLACK);
        fx(m, FX_OVER);
        release(m);
        for (;;) {
            unsigned k = wait_key(m);
            if (k == H9_KEY_BACK) return;
            if (k == H9_KEY_MENU) break;
        }
    }
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)api; (void)self;
    struct game m;
    m.hi = 0; m.rng = TICK | 1; m.sound = 1; m.waka = 0;
    m.snd.ready = 0;
    title(&m);
    for (;;) {
        gfx_wait_release();
        unsigned k = gfx_wait_key();
        if (k == H9_KEY_BACK) return 0;
        if (k != H9_KEY_5) break;
        m.sound ^= 1;
        title(&m);
    }
    if (m.sound) snd_init(&m.snd);
    run(&m);
    snd_close(&m.snd);
    return 0;
}
