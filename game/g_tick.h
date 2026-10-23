#ifndef G_TICK_H
#define G_TICK_H

#include "q_shared.h"

/*
tickrate-independent timing: the game was tuned on a 50 ms server frame, and these helpers make timers,
per-frame decays and per-frame chances come out the same at any sv_fps. pure functions of their arguments
*/

#define TICK_BASE_MSEC 50	// the server frame the game was tuned on (sv_fps 20)

/*
what a server stepping `grid` ms made of an ms timer: strict -> smallest multiple of grid > ms,
inclusive -> smallest multiple of grid >= ms. ms <= 0 gives grid (strict) or 0 (inclusive); grid < 1 is 1
*/
int Tick_QuantizeTo(int ms, int grid, qboolean strict);
#define Tick_Quantize(ms, strict) Tick_QuantizeTo((ms), TICK_BASE_MSEC, (strict))

/*
ticks owed at `now` on a deadline advanced `period` ms per tick, so a frame's leftover carries into the
next tick. a deadline more than one frame in the past is re-anchored to now instead of paid out. period < 1 is 1
*/
int Tick_Due(int *deadline, int now, int frameMsec, int period);

/*
Tick_Due for a gate evaluated only now and then (when a client's packets arrive): the staleness slack is the time
since *lastEval, clamped to [frameMsec, maxGap], so a deadline up to one evaluation gap old still carries. a cap of
the period or more is taken as period - 1: when the period is longer than a frame, a deadline a full period old is
stale and pays exactly one tick. stamps *lastEval with now; *lastEval <= 0 means never evaluated (slack frameMsec)
*/
int Tick_DueSince(int *deadline, int *lastEval, int now, int frameMsec, int period, int maxGap);

// next deadline for a nextthink-style think, carried from the old one unless it is unset or more than a frame stale
int Tick_Rearm(int deadline, int now, int frameMsec, int period);

/*
fixed-step replay: steps of `step` ms elapsed since t0, capped at maxSteps. returns how many are due beyond
*lastStep and advances *lastStep to the new count. t0 > now gives 0
*/
int Tick_StepsDue(int t0, int now, int step, int *lastStep, int maxSteps);

// dtMsec in base ticks (negative dt is 0)
float Tick_Frac(int dtMsec);

// what a per-base-tick multiplier k comes to over dtMsec. k <= 0 gives 0, dt <= 0 gives 1
float Tick_Decay(float kPerBaseTick, int dtMsec);

// a per-base-tick chance p (clamped 0..1) over dtMsec, decided by rnd01 in [0, 1)
qboolean Tick_Roll(float pPerBaseTick, int dtMsec, float rnd01);

/*
an amount per base tick scaled by dtMsec, with the fraction carried in *residual. returns the whole units
to apply now (negative for negative amounts)
*/
int Tick_Accum(float *residual, float amountPerBaseTick, int dtMsec);

#endif
