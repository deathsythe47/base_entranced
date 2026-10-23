// host unit test for g_tick: make -C game tick-test
#include <math.h>
#include <stdio.h>

#include "g_tick.h"

static int checks, failures;

#define CHECK(cond, ...) do { \
	checks++; \
	if (!(cond)) { \
		failures++; \
		printf("FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); \
		printf(__VA_ARGS__); \
		printf("\n"); \
	} \
} while (0)

static const int frameTimes[] = { 50, 33, 25, 16, 8, 4 };
#define NUM_FRAMETIMES ((int)(sizeof(frameTimes) / sizeof(frameTimes[0])))

#define SIM_MSEC 60000

static int Near(int got, int want, int slack) {
	return got >= want - slack && got <= want + slack;
}

static void TestQuantize(void) {
	static const int strictIn[] = { 100, 200, 1000, 14, 40, 50, 0 };
	static const int strictOut[] = { 150, 250, 1050, 50, 50, 100, 50 };
	static const int inclIn[] = { 100, 200, 1000, 14, 40, 50, 51 };
	static const int inclOut[] = { 100, 200, 1000, 50, 50, 50, 100 };
	int i, ms;

	for (i = 0; i < (int)(sizeof(strictIn) / sizeof(strictIn[0])); i++)
		CHECK(Tick_Quantize(strictIn[i], qtrue) == strictOut[i], "strict %d -> %d", strictIn[i], Tick_Quantize(strictIn[i], qtrue));
	for (i = 0; i < (int)(sizeof(inclIn) / sizeof(inclIn[0])); i++)
		CHECK(Tick_Quantize(inclIn[i], qfalse) == inclOut[i], "inclusive %d -> %d", inclIn[i], Tick_Quantize(inclIn[i], qfalse));

	CHECK(Tick_QuantizeTo(200, 100, qtrue) == 300, "%d", Tick_QuantizeTo(200, 100, qtrue));
	CHECK(Tick_QuantizeTo(200, 100, qfalse) == 200, "%d", Tick_QuantizeTo(200, 100, qfalse));
	CHECK(Tick_QuantizeTo(150, 100, qfalse) == 200, "%d", Tick_QuantizeTo(150, 100, qfalse));

	// the ReArmForceRegen formula
	for (ms = 1; ms <= 3000; ms++)
		CHECK(Tick_Quantize(ms, qtrue) == 50 * ((ms + 50) / 50), "ms %d -> %d", ms, Tick_Quantize(ms, qtrue));
}

static void TestDue(void) {
	static const int periods[] = { 50, 100, 150, 250, 300, 1000 };
	int f, p, i, now, deadline, base;

	for (f = 0; f < NUM_FRAMETIMES; f++) {
		int frameMsec = frameTimes[f];
		for (p = 0; p < (int)(sizeof(periods) / sizeof(periods[0])); p++) {
			int period = periods[p];
			int total = 0, maxDue = 0, worstLate = 0, early = 0;

			CHECK(period >= frameMsec, "period %d frame %d", period, frameMsec);
			deadline = 0;
			for (now = 0; now < SIM_MSEC; now += frameMsec) {
				int ideal = deadline;
				int n = Tick_Due(&deadline, now, frameMsec, period);
				if (n > maxDue)
					maxDue = n;
				if (n) {
					if (now < ideal)
						early++;
					if (now - ideal > worstLate)
						worstLate = now - ideal;
				}
				total += n;
			}
			CHECK(Near(total, SIM_MSEC / period, 1), "frame %d period %d: %d ticks", frameMsec, period, total);
			CHECK(maxDue <= 1, "frame %d period %d: %d ticks in one call", frameMsec, period, maxDue);
			CHECK(!early, "frame %d period %d: %d early ticks", frameMsec, period, early);
			CHECK(worstLate < frameMsec, "frame %d period %d: tick %d ms late", frameMsec, period, worstLate);
		}
	}

	// a deadline left well behind re-anchors and pays one tick, then waits a full period
	base = 10000;
	deadline = base - 3000;
	CHECK(Tick_Due(&deadline, base, 25, 100) == 1, "stale deadline");
	CHECK(deadline == base + 100, "re-anchored to %d", deadline);
	for (i = 1; i < 100; i++)
		CHECK(Tick_Due(&deadline, base + i, 1, 100) == 0, "early tick at +%d", i);
	CHECK(Tick_Due(&deadline, base + 100, 1, 100) == 1, "tick at +100");
}

/*
a gate evaluated only when a client's packets arrive (every gap ms on an 8 ms server) keeps its carry when the
staleness slack is the time since the last evaluation, capped at the period (G_TickDueSince)
*/
static void TestDueSince(void) {
	static const int gaps[] = { 16, 24, 32, 40 };
	const int frameMsec = 8, period = 150;
	int g;

	for (g = 0; g < (int)(sizeof(gaps) / sizeof(gaps[0])); g++) {
		int gap = gaps[g];
		int now, nextEval = 0, lastEval = 0, deadline = 0, fires = 0, first = -1, last = 0, worstLate = 0;
		float mean;

		for (now = 0; now < SIM_MSEC; now += frameMsec) {
			int ideal = deadline, n;

			if (now < nextEval)
				continue;
			nextEval += gap;
			n = Tick_DueSince(&deadline, &lastEval, now, frameMsec, period, period);
			if (n) {
				if (first < 0)
					first = now;
				else if (now - ideal > worstLate)
					worstLate = now - ideal;
				last = now;
				fires += n;
			}
		}
		mean = fires > 1 ? (last - first) / (float)(fires - 1) : 0.0f;
		CHECK(fabsf(mean - period) <= period * 0.005f, "gap %d: mean fire interval %.2f", gap, mean);
		CHECK(worstLate <= gap, "gap %d: a fire %d ms late", gap, worstLate);
	}

	// an idle gap longer than the period pays one tick and re-anchors, whatever cap at or above the period is asked for
	{
		const int base = 10000, idle = 400;
		int deadline = base, lastEval = 0, n;

		CHECK(Tick_DueSince(&deadline, &lastEval, base, frameMsec, 50, 50) == 1, "first evaluation");
		n = Tick_DueSince(&deadline, &lastEval, base + idle, frameMsec, 50, 50);
		CHECK(n == 1, "period cap: %d ticks after %d ms idle", n, idle);
		CHECK(deadline == base + idle + 50, "period cap: re-anchored to %d", deadline - base);

		deadline = base;
		lastEval = 0;
		CHECK(Tick_DueSince(&deadline, &lastEval, base, frameMsec, 50, 1000) == 1, "first evaluation");
		n = Tick_DueSince(&deadline, &lastEval, base + idle, frameMsec, 50, 1000);
		CHECK(n == 1, "1000 cap: %d ticks after %d ms idle", n, idle);
		CHECK(deadline == base + idle + 50, "1000 cap: re-anchored to %d", deadline - base);
	}

	// a deadline a full period old (one skipped tick) is stale: one tick, re-anchored; a frame newer still carries
	{
		const int now = 20000;
		int deadline = now - period, lastEval = now - period, n;

		n = Tick_DueSince(&deadline, &lastEval, now, frameMsec, period, period);
		CHECK(n == 1, "full period old: %d ticks", n);
		CHECK(deadline == now + period, "full period old: deadline %d", deadline - now);

		deadline = now - period + frameMsec;
		lastEval = now - period;
		n = Tick_DueSince(&deadline, &lastEval, now, frameMsec, period, period);
		CHECK(n == 1, "under a period old: %d ticks", n);
		CHECK(deadline == now + frameMsec, "under a period old: deadline %d (carried)", deadline - now);
	}
}

/*
a vehicle muzzle wait: ready at wait <= now, checked every 32 ms with each check stamped (trigger held or not), the next
wait armed from the deadline that made the shot due with the gap since the previous check
*/
static int MuzzleCheck(int *wait, int *lastEval, int now, qboolean held, int frameMsec, int period) {
	int prev = *lastEval;

	*lastEval = now;
	if (!held || *wait > now)
		return 0;
	Tick_DueSince(wait, &prev, now, frameMsec, period, period);
	return 1;
}

static void TestMuzzleWait(void) {
	const int frameMsec = 8, step = 32, period = 250;
	int now = 1000, wait = 0, lastEval = 0, shots = 0, first = -1, last = 0, reanchors = 0, end;
	float mean;

	// held 20 s: one shot's lateness (up to a check gap) is under 0.5% of the mean only over this many shots
	for (end = now + 20000; now <= end; now += step) {
		int prevWait = wait;

		if (MuzzleCheck(&wait, &lastEval, now, qtrue, frameMsec, period)) {
			if (first < 0)
				first = now;
			else if (wait != prevWait + period)
				reanchors++;
			last = now;
			shots++;
		}
	}
	mean = shots > 1 ? (last - first) / (float)(shots - 1) : 0.0f;
	CHECK(fabsf(mean - period) <= period * 0.005f, "held: mean shot interval %.2f over %d shots", mean, shots);
	CHECK(!reanchors, "held: %d re-anchors", reanchors);

	// 400 ms released, then pressed: the first shot is immediate and the next one a full period later
	for (end = now + 400; now < end; now += step)
		MuzzleCheck(&wait, &lastEval, now, qfalse, frameMsec, period);
	CHECK(MuzzleCheck(&wait, &lastEval, now, qtrue, frameMsec, period), "press: no immediate shot");
	CHECK(wait == now + period, "press: next wait %d after the shot", wait - now);
}

static void TestRearm(void) {
	static const int periods[] = { 50, 100 };
	int f, p;

	for (f = 0; f < NUM_FRAMETIMES; f++) {
		int frameMsec = frameTimes[f];
		for (p = 0; p < (int)(sizeof(periods) / sizeof(periods[0])); p++) {
			int period = periods[p];
			int nextthink = Tick_Rearm(0, 0, frameMsec, period);
			int thinks = 0, stuck = 0, now;

			for (now = 0; now < SIM_MSEC; now += frameMsec) {
				if (nextthink > 0 && nextthink <= now) {
					thinks++;
					nextthink = Tick_Rearm(nextthink, now, frameMsec, period);
					if (nextthink <= now)
						stuck++;
				}
			}
			CHECK(Near(thinks, SIM_MSEC / period, 1), "frame %d period %d: %d thinks", frameMsec, period, thinks);
			CHECK(!stuck, "frame %d period %d: re-armed at or before now %d times", frameMsec, period, stuck);
		}
	}

	CHECK(Tick_Rearm(0, 5000, 25, 100) == 5100, "unset deadline -> %d", Tick_Rearm(0, 5000, 25, 100));
	CHECK(Tick_Rearm(4900, 5000, 25, 100) == 5100, "stale deadline -> %d", Tick_Rearm(4900, 5000, 25, 100));
	CHECK(Tick_Rearm(4990, 5000, 25, 100) == 5090, "carried deadline -> %d", Tick_Rearm(4990, 5000, 25, 100));
}

static void TestStepsDue(void) {
	int lastStep = 0, n, now, total = 0;

	n = Tick_StepsDue(0, 0, 50, &lastStep, 16);
	CHECK(n == 0 && lastStep == 0, "now 0: %d new, lastStep %d", n, lastStep);
	n = Tick_StepsDue(0, 120, 50, &lastStep, 16);
	CHECK(n == 2 && lastStep == 2, "now 120: %d new, lastStep %d", n, lastStep);
	n = Tick_StepsDue(0, 130, 50, &lastStep, 16);
	CHECK(n == 0 && lastStep == 2, "now 130: %d new, lastStep %d", n, lastStep);
	n = Tick_StepsDue(0, 5000, 50, &lastStep, 16);
	CHECK(n == 14 && lastStep == 16, "now 5000: %d new, lastStep %d", n, lastStep);
	n = Tick_StepsDue(0, 6000, 50, &lastStep, 16);
	CHECK(n == 0 && lastStep == 16, "now 6000: %d new, lastStep %d", n, lastStep);

	CHECK(Tick_StepsDue(100, 50, 50, &lastStep, 16) == 0, "t0 in the future");

	// never more than maxSteps overall, whatever the frame pattern
	lastStep = 0;
	for (now = 1000; now < 3000; now += 33)
		total += Tick_StepsDue(1000, now, 50, &lastStep, 16);
	CHECK(total == 16, "%d steps overall", total);
}

static unsigned int lcgState = 12345;

static float Rand01(void) {
	lcgState = lcgState * 1664525u + 1013904223u;
	return (lcgState >> 8) / 16777216.0f;
}

static void TestScalars(void) {
	float residual, want, frac;
	int i, total, hits;

	CHECK(Tick_Frac(50) == 1.0f, "%f", Tick_Frac(50));
	CHECK(Tick_Frac(25) == 0.5f, "%f", Tick_Frac(25));
	CHECK(Tick_Frac(-10) == 0.0f, "%f", Tick_Frac(-10));

	CHECK(fabsf(Tick_Decay(0.9f, 50) - 0.9f) < 1e-5f, "%f", Tick_Decay(0.9f, 50));
	CHECK(fabsf(Tick_Decay(0.9f, 100) - 0.81f) < 1e-5f, "%f", Tick_Decay(0.9f, 100));
	CHECK(fabsf(Tick_Decay(0.85f, 8) - 0.9743f) < 1e-4f, "%f", Tick_Decay(0.85f, 8));
	CHECK(Tick_Decay(0.0f, 50) == 0.0f, "%f", Tick_Decay(0.0f, 50));
	CHECK(Tick_Decay(0.9f, 0) == 1.0f, "%f", Tick_Decay(0.9f, 0));

	want = 1.0f - powf(0.9f, 8 / 50.0f);
	hits = 0;
	for (i = 0; i < 10000; i++)
		hits += Tick_Roll(0.1f, 8, Rand01()) ? 1 : 0;
	frac = hits / 10000.0f;
	CHECK(fabsf(frac - want) < 0.01f, "hit fraction %f, want %f", frac, want);
	CHECK(!Tick_Roll(0.0f, 50, 0.0f), "p 0 hit");
	CHECK(Tick_Roll(1.0f, 50, 0.999f), "p 1 missed");
	CHECK(Tick_Roll(2.0f, 50, 0.999f), "p clamped to 1 missed");

	residual = 0.0f;
	total = 0;
	for (i = 0; i < 50; i++)
		total += Tick_Accum(&residual, 1.0f, 8);
	CHECK(total == 8 && fabsf(residual) < 1.0f, "positive: %d units, residual %f", total, residual);

	residual = 0.0f;
	total = 0;
	for (i = 0; i < 50; i++)
		total += Tick_Accum(&residual, -1.0f, 8);
	CHECK(total == -8 && fabsf(residual) < 1.0f, "negative: %d units, residual %f", total, residual);
}

int main(void) {
	TestQuantize();
	TestDue();
	TestDueSince();
	TestMuzzleWait();
	TestRearm();
	TestStepsDue();
	TestScalars();

	printf("test_tick: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
