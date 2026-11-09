// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 port of DMC100-Doom: start-up trace (diagnostic).
//
// Engine start-up stages are wrapped with GNU ld --wrap (build.py, WRAPS): each
// wrapper shows the stage name and the stack position on screen, runs the
// stage, then marks it done. Counters kept by the platform hooks (NOR reads,
// pump strips, timer and input polls) are shown on one line, refreshed at most
// every 0.5 s, until the first strip reaches the panel. A hang therefore shows
// which stage it is in and whether that stage is still calling the platform.

#include <stdint.h>

#include "doomtype.h"
#include "doomdef.h"
#include "d_player.h"

#include "td.h"

volatile uint32_t td_nor_reads, td_pump_strips, td_time_calls, td_tic_calls;
static uint32_t last_draw;
static unsigned stage_row;

enum { BLACK = 0x0000, WHITE = 0xffff, CYAN = 0x07ff, GREEN = 0x07e0 };

static void hex8(char *s, uint32_t v)
{
    static const char digits[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++)
        s[i] = digits[(v >> (28 - 4 * i)) & 15];
    s[8] = 0;
}

static void dec(char *s, uint32_t v)
{
    char t[11];
    int i = 10;
    t[i] = 0;
    do { t[--i] = (char)('0' + v % 10); v /= 10; } while (v && i);
    for (int k = 0; (s[k] = t[i + k]); k++) {
    }
}

void td_trace_counters(int force)
{
    if (td_pump_strips && !force)
        return;  // frames are on screen
    uint32_t now = td_ticks();
    if (!force && now - last_draw < 50)
        return;
    last_draw = now;
    char line[24];
    line[0] = 'R';
    dec(line + 1, td_nor_reads % 100000);
    td_fill(0, 116, TD_W, 12, BLACK);
    td_text(0, 116, line, CYAN, BLACK);
    line[0] = 'T';
    dec(line + 1, td_time_calls % 100000);
    td_text(56, 116, line, CYAN, BLACK);
    line[0] = 'S';
    dec(line + 1, td_tic_calls % 1000);
    td_text(112, 116, line, CYAN, BLACK);
}

static void stage(const char *name)
{
    char sp[9];
    volatile uint8_t here = 0;  // its address marks the stack depth
    hex8(sp, (uint32_t)(uintptr_t)&here);
    unsigned y = 56 + 10 * (stage_row % 6);
    td_fill(0, y, TD_W, 10, BLACK);
    td_text(0, y, name, WHITE, BLACK);
    td_text(96, y, sp + 3, CYAN, BLACK);  // low 5 hex digits of the stack
    td_trace_counters(1);
}

static void done(void)
{
    unsigned y = 56 + 10 * (stage_row % 6);
    td_text(88, y, ".", GREEN, BLACK);
    stage_row++;
}

#define WRAP0(name)                       \
    void __real_##name(void);             \
    void __wrap_##name(void)              \
    {                                     \
        stage(#name);                     \
        __real_##name();                  \
        done();                           \
    }

WRAP0(V_Init)
WRAP0(PAR_Init)
WRAP0(R_Init)
WRAP0(P_Init)
WRAP0(D_CheckNetGame)
WRAP0(HU_Init)
WRAP0(ST_Init)

void __real_S_Init(int v);
void __wrap_S_Init(int v)
{
    stage("S_Init");
    __real_S_Init(v);
    done();
}

void __real_G_InitNew(skill_t skill, int episode, int map);
void __wrap_G_InitNew(skill_t skill, int episode, int map)
{
    stage("G_InitNew");
    __real_G_InitNew(skill, episode, map);
    done();
}

void __real_P_SetupLevel(int episode, int map, int playermask, skill_t skill);
void __wrap_P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{
    stage("SETUPLEVEL");
    __real_P_SetupLevel(episode, map, playermask, skill);
    done();
}

// Inside the game loop: shown only until the first strip is drawn.
void __real_TryRunTics(void);
void __wrap_TryRunTics(void)
{
    static int shown;
    if (!shown++)
        stage("TRYRUNTICS");
    __real_TryRunTics();
}

void __real_R_RenderPlayerView(player_t *player);
void __wrap_R_RenderPlayerView(player_t *player)
{
    static int shown;
    if (!shown++)
        stage("RENDERVIEW");
    __real_R_RenderPlayerView(player);
}
