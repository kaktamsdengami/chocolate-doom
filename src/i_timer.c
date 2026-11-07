//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      Timer functions.
//

#include "SDL.h"

#include <stdlib.h>
#include <string.h>

#include "i_timer.h"
#include "doomtype.h"
#include "m_argv.h"

//
// I_GetTime
// returns time in 1/35th second tics
//

static Uint32 basetime = 0;

// PLAYER TWO QA clock. A computer-use agent needs seconds per action, so the
// game clock can run slower (-qa-slowmo K) or only advance a fixed budget per
// key press (-qa-tics N). Menus and intermissions always run in real time.

static int qa_slowmo = 1;
static int qa_step_ms = 0;
static int qa_budget_ms = 0;
static boolean qa_freerun = true;
static Uint32 qa_last_real = 0;
static Uint32 qa_scaled = 0;
static Uint8 qa_keys[SDL_NUM_SCANCODES];

static Uint32 RealMS(void)
{
    Uint32 ticks;

    ticks = SDL_GetTicks();

    if (basetime == 0)
        basetime = ticks;

    return ticks - basetime;
}

static void QA_CheckKeyPresses(void)
{
    const Uint8 *state;
    int numkeys, i;

    SDL_PumpEvents();
    state = SDL_GetKeyboardState(&numkeys);

    if (numkeys > SDL_NUM_SCANCODES)
        numkeys = SDL_NUM_SCANCODES;

    for (i = 0; i < numkeys; ++i)
    {
        if (state[i] && !qa_keys[i])
            qa_budget_ms += qa_step_ms;
        qa_keys[i] = state[i];
    }
}

static Uint32 VirtualMS(void)
{
    Uint32 now, delta;

    now = RealMS();

    if (qa_slowmo == 1 && qa_step_ms == 0)
        return now;

    delta = now - qa_last_real;
    qa_last_real = now;

    if (qa_freerun)
    {
        qa_scaled += delta * qa_slowmo;
    }
    else
    {
        if (qa_step_ms > 0)
        {
            QA_CheckKeyPresses();
            if (delta > (Uint32) qa_budget_ms)
                delta = qa_budget_ms;
            qa_budget_ms -= delta;
        }
        qa_scaled += delta;
    }

    return qa_scaled / qa_slowmo;
}

boolean I_QAClockActive(void)
{
    return qa_slowmo > 1 || qa_step_ms > 0;
}

void I_QASetFreeRun(boolean freerun)
{
    if (freerun != qa_freerun)
    {
        VirtualMS();
        qa_freerun = freerun;
        qa_budget_ms = 0;
    }
}

int  I_GetTime (void)
{
    return (VirtualMS() * TICRATE) / 1000;
}

//
// Same as I_GetTime, but returns time in milliseconds
//

int I_GetTimeMS(void)
{
    return VirtualMS();
}

void I_Sleep(int ms)
{
    SDL_Delay(ms);
}

void I_WaitVBL(int count)
{
    I_Sleep((count * 1000) / 70);
}


void I_InitTimer(void)
{
    int i;

    // initialize timer

    SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");

    SDL_Init(SDL_INIT_TIMER);

    //!
    // @arg <k>
    // @category obscure
    //
    // PLAYER TWO QA: run the game clock k times slower.
    //

    i = M_CheckParmWithArgs("-qa-slowmo", 1);
    if (i > 0)
    {
        qa_slowmo = atoi(myargv[i + 1]);
        if (qa_slowmo < 1)
            qa_slowmo = 1;
    }

    //!
    // @arg <n>
    // @category obscure
    //
    // PLAYER TWO QA frame-step: the game only advances n tics (35 = 1 s)
    // per key press, then waits for the next press.
    //

    i = M_CheckParmWithArgs("-qa-tics", 1);
    if (i > 0)
    {
        qa_step_ms = (atoi(myargv[i + 1]) * 1000) / TICRATE;
        if (qa_step_ms < 0)
            qa_step_ms = 0;
    }
    else if (M_CheckParm("-qa-framestep") > 0)
    {
        qa_step_ms = 1000;
    }

    memset(qa_keys, 0, sizeof(qa_keys));
    qa_last_real = RealMS();
    qa_scaled = qa_last_real * qa_slowmo;
}

