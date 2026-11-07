//
// PLAYER TWO QA build: JSONL event log and scripted QA actions.
//
// -qa-log <file>      one JSON line per game event (GameEvent in the
//                     PLAYER TWO contracts). Read by the verifier only.
// -qa-trace <n>       also log the player state every n tics (default 35).
// -qa-script <file>   timed QA actions, one per line: "<tic> <command> ...".
// -qa-wall-err <pct>  log walls whose vanilla distance is off by >= pct %
// -qa-wall-len <n>    ... only for walls at least n units long (default 512); default pct 0.5.
//

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

#include "doomdef.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_game.h"
#include "i_system.h"
#include "info.h"
#include "m_argv.h"
#include "m_misc.h"
#include "p_inter.h"
#include "p_local.h"
#include "r_main.h"
#include "r_state.h"
#include "tables.h"

#include "st_stuff.h"
#include "qa.h"
#include "qa_names.h"

boolean qa_logging = false;

static FILE *qa_log;
static int qa_trace = 35;
static double qa_wall_err = 0.5;
static double qa_wall_min_len = 512.0;
static int qa_last_sky = -1000;
static double qa_wall_max;
static int qa_wall_line = -1;
static double qa_wall_len;
static int qa_last_wall = -1000;

#define MAX_QA_LINES 512

typedef struct
{
    int tic;
    char cmd[16];
    char args[96];
} qa_line_t;

static qa_line_t qa_script[MAX_QA_LINES];
static int qa_num_lines;
static int qa_next_line;

static int qa_shot_tics;
static int qa_move_tics;
static int qa_move_forward, qa_move_side, qa_move_turn;

static const char *power_names[] =
{
    "invulnerability", "strength", "invisibility", "ironfeet", "allmap",
    "infrared",
};

static const char *MapName(void)
{
    static char buf[16];

    if (gamemode == commercial)
        M_snprintf(buf, sizeof(buf), "MAP%02d", gamemap);
    else
        M_snprintf(buf, sizeof(buf), "E%dM%d", gameepisode, gamemap);

    return buf;
}

static double Fixed(fixed_t v)
{
    return v / (double) FRACUNIT;
}

static double Degrees(angle_t a)
{
    return a * (360.0 / 4294967296.0);
}

const char *QA_Str(const char *s)
{
    static char bufs[4][256];
    static int which;
    char *out = bufs[which];
    size_t n = 0;

    which = (which + 1) % 4;

    for (; s != NULL && *s != '\0' && n < sizeof(bufs[0]) - 3; ++s)
    {
        if (*s == '"' || *s == '\\')
            out[n++] = '\\';
        out[n++] = (*s == '\n' || *s == '\r') ? ' ' : *s;
    }
    out[n] = '\0';

    return out;
}

static const char *MobjName(mobj_t *mo)
{
    if (mo->type >= 0 && mo->type < arrlen(qa_mobj_names))
        return qa_mobj_names[mo->type] + 3;

    return "UNKNOWN";
}

void QA_Event(const char *type, mobj_t *actor, const char *datafmt, ...)
{
    player_t *p = &players[consoleplayer];
    va_list args;

    if (!qa_logging)
        return;

    fprintf(qa_log, "{\"tic\":%d,\"t\":%.3f,\"map\":\"%s\",\"type\":\"%s\"",
            leveltime, SDL_GetTicks() / 1000.0, MapName(), type);

    if (p->mo != NULL)
    {
        fprintf(qa_log, ",\"player\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,"
                "\"angle\":%.2f,\"health\":%d,\"armor\":%d,\"sector\":%d}",
                Fixed(p->mo->x), Fixed(p->mo->y), Fixed(p->mo->z),
                Degrees(p->mo->angle), p->health, p->armorpoints,
                (int) (p->mo->subsector->sector - sectors));
    }

    if (actor != NULL)
    {
        fprintf(qa_log, ",\"actor\":{\"id\":%d,\"type\":\"%s\",\"x\":%.2f,"
                "\"y\":%.2f,\"z\":%.2f,\"health\":%d}",
                (int) (((size_t) actor / sizeof(void *)) & 0x7fffffff),
                MobjName(actor), Fixed(actor->x), Fixed(actor->y),
                Fixed(actor->z), actor->health);
    }

    fprintf(qa_log, ",\"data\":{");
    if (datafmt != NULL)
    {
        va_start(args, datafmt);
        vfprintf(qa_log, datafmt, args);
        va_end(args);
    }
    fprintf(qa_log, "}}\n");
    fflush(qa_log);
}

static void LoadScript(const char *filename)
{
    FILE *f;
    char line[160];

    f = M_fopen(filename, "r");
    if (f == NULL)
        I_Error("QA: cannot open script %s", filename);

    while (fgets(line, sizeof(line), f) != NULL && qa_num_lines < MAX_QA_LINES)
    {
        qa_line_t *l = &qa_script[qa_num_lines];

        l->args[0] = '\0';
        if (line[0] == '#'
         || sscanf(line, "%d %15s %95[^\n]", &l->tic, l->cmd, l->args) < 2)
            continue;
        ++qa_num_lines;
    }

    fclose(f);
}

void QA_Init(void)
{
    int i;

    i = M_CheckParmWithArgs("-qa-log", 1);
    if (i > 0)
    {
        qa_log = M_fopen(myargv[i + 1], "w");
        if (qa_log == NULL)
            I_Error("QA: cannot open log %s", myargv[i + 1]);
        qa_logging = true;
    }

    i = M_CheckParmWithArgs("-qa-trace", 1);
    if (i > 0)
        qa_trace = atoi(myargv[i + 1]);

    i = M_CheckParmWithArgs("-qa-wall-err", 1);
    if (i > 0)
        qa_wall_err = atof(myargv[i + 1]);

    i = M_CheckParmWithArgs("-qa-wall-len", 1);
    if (i > 0)
        qa_wall_min_len = atof(myargv[i + 1]);

    i = M_CheckParmWithArgs("-qa-script", 1);
    if (i > 0)
        LoadScript(myargv[i + 1]);
}

static void RunCommand(qa_line_t *l)
{
    player_t *p = &players[consoleplayer];
    int a = 0, b = 0, c = 0, d = 0;

    if (p->mo == NULL)
        return;

    QA_Event("qa", NULL, "\"cmd\":\"%s\",\"args\":\"%s\"",
             QA_Str(l->cmd), QA_Str(l->args));

    if (!strcmp(l->cmd, "health"))
    {
        p->health = p->mo->health = atoi(l->args);
    }
    else if (!strcmp(l->cmd, "hurt"))
    {
        P_DamageMobj(p->mo, NULL, NULL, atoi(l->args));
    }
    else if (!strcmp(l->cmd, "give"))
    {
        for (a = 0; a < arrlen(power_names); ++a)
            if (!strcmp(l->args, power_names[a]))
                P_GivePower(p, a);
    }
    else if (!strcmp(l->cmd, "key"))
    {
        // key <char code>: type a letter again (cheat codes, from "key" events)
        event_t ev = {ev_keydown, 0, 0, 0, 0};
        ev.data1 = ev.data2 = atoi(l->args);
        ST_Responder(&ev);
    }
    else if (!strcmp(l->cmd, "god"))
    {
        p->cheats ^= CF_GODMODE;
    }
    else if (!strcmp(l->cmd, "warp"))
    {
        // warp <x> <y> <angle in degrees>
        sscanf(l->args, "%d %d %d", &a, &b, &c);
        P_TeleportMove(p->mo, a * FRACUNIT, b * FRACUNIT);
        p->mo->angle = (angle_t) (((c % 360 + 360) % 360) * (ANG90 / 90));
        p->mo->momx = p->mo->momy = 0;
    }
    else if (!strcmp(l->cmd, "move"))
    {
        // move <forward> <side> <turn> <tics>, like a held key
        sscanf(l->args, "%d %d %d %d", &a, &b, &c, &d);
        qa_move_forward = a;
        qa_move_side = b;
        qa_move_turn = c;
        qa_move_tics = d;
    }
    else if (!strcmp(l->cmd, "shot"))
    {
        // shot [n]: one screenshot now, or one per tic for n tics
        qa_shot_tics = l->args[0] != '\0' ? atoi(l->args) : 1;
    }
    else if (!strcmp(l->cmd, "quit"))
    {
        // Leave without ENDOOM or config writes so scripted runs never block.
        if (qa_log != NULL)
            fclose(qa_log);
        exit(0);
    }
}

void QA_PreTick(void)
{
    ticcmd_t *cmd = &players[consoleplayer].cmd;

    while (qa_next_line < qa_num_lines
        && qa_script[qa_next_line].tic <= leveltime)
    {
        RunCommand(&qa_script[qa_next_line++]);
    }

    if (qa_shot_tics > 0)
    {
        G_ScreenShot();
        --qa_shot_tics;
    }

    if (qa_move_tics > 0)
    {
        cmd->forwardmove = qa_move_forward;
        cmd->sidemove = qa_move_side;
        cmd->angleturn = qa_move_turn;
        --qa_move_tics;
    }
}

void QA_PostTick(void)
{
    if (qa_trace > 0 && leveltime % qa_trace == 0)
        QA_Event("player", NULL, NULL);

    if (qa_wall_line >= 0 && leveltime - qa_last_wall >= 35)
    {
        QA_Event("render", NULL, "\"flag\":\"wall_distance_error\","
                 "\"line\":%d,\"length\":%.1f,\"error_pct\":%.1f",
                 qa_wall_line, qa_wall_len, qa_wall_max);
        qa_last_wall = leveltime;
        qa_wall_line = -1;
        qa_wall_max = 0;
    }
}

void QA_SkyDrawn(int fixed)
{
    if (!qa_logging || fixed == 0 || leveltime - qa_last_sky < 35)
        return;

    qa_last_sky = leveltime;
    QA_Event("render", NULL, "\"flag\":\"sky_ignores_colormap\","
             "\"fixedcolormap\":%d", fixed);
}

void QA_WallDistance(line_t *line, fixed_t vanilla, double precise)
{
    double err, len;

    if (!qa_logging || precise < 16.0)
        return;

    len = hypot(Fixed(line->dx), Fixed(line->dy));
    if (len < qa_wall_min_len)
        return;

    err = fabs(Fixed(vanilla) - precise) * 100.0 / precise;

    if (err >= qa_wall_err && err > qa_wall_max)
    {
        qa_wall_max = err;
        qa_wall_line = (int) (line - lines);
        qa_wall_len = len;
    }
}
