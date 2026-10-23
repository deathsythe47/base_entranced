#include <math.h>

#include "g_tick.h"

#define TICK_ACCUM_EPSILON 1e-4f

int Tick_QuantizeTo(int ms, int grid, qboolean strict) {
	if (grid < 1)
		grid = 1;

	if (strict)
		return ms <= 0 ? grid : grid * (ms / grid + 1);
	return ms <= 0 ? 0 : grid * ((ms + grid - 1) / grid);
}

int Tick_Due(int *deadline, int now, int frameMsec, int period) {
	int n = 0;

	if (period < 1)
		period = 1;

	if (*deadline < now - frameMsec)
		*deadline = now;
	while (*deadline <= now) {
		*deadline += period;
		n++;
	}
	return n;
}

int Tick_DueSince(int *deadline, int *lastEval, int now, int frameMsec, int period, int maxGap) {
	int gap = frameMsec;

	if (maxGap >= period)
		maxGap = period - 1;

	if (*lastEval > 0) {
		gap = now - *lastEval;
		if (gap > maxGap)
			gap = maxGap;
		if (gap < frameMsec)
			gap = frameMsec;
	}
	*lastEval = now;
	return Tick_Due(deadline, now, gap, period);
}

int Tick_Rearm(int deadline, int now, int frameMsec, int period) {
	int d = deadline;

	if (period < 1)
		period = 1;

	if (d <= 0 || d < now - frameMsec)
		d = now;
	return d + period;
}

int Tick_StepsDue(int t0, int now, int step, int *lastStep, int maxSteps) {
	int total, n;

	if (t0 > now)
		return 0;
	if (step < 1)
		step = 1;

	total = (now - t0) / step;
	if (total > maxSteps)
		total = maxSteps;

	n = total - *lastStep;
	if (n <= 0)
		return 0;
	*lastStep = total;
	return n;
}

float Tick_Frac(int dtMsec) {
	if (dtMsec < 0)
		return 0.0f;
	return dtMsec / (float)TICK_BASE_MSEC;
}

float Tick_Decay(float kPerBaseTick, int dtMsec) {
	if (kPerBaseTick <= 0.0f)
		return 0.0f;
	if (dtMsec <= 0)
		return 1.0f;
	return powf(kPerBaseTick, Tick_Frac(dtMsec));
}

qboolean Tick_Roll(float pPerBaseTick, int dtMsec, float rnd01) {
	float p = pPerBaseTick;

	if (p < 0.0f)
		p = 0.0f;
	else if (p > 1.0f)
		p = 1.0f;

	return rnd01 < 1.0f - powf(1.0f - p, Tick_Frac(dtMsec)) ? qtrue : qfalse;
}

int Tick_Accum(float *residual, float amountPerBaseTick, int dtMsec) {
	int whole;

	*residual += amountPerBaseTick * Tick_Frac(dtMsec);

	// toward zero, so negative amounts mirror positive ones. the nudge keeps float rounding from
	// holding back a unit the exact sum has reached
	whole = (int)(*residual + (*residual < 0.0f ? -TICK_ACCUM_EPSILON : TICK_ACCUM_EPSILON));
	*residual -= whole;
	return whole;
}
