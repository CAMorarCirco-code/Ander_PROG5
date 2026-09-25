/*
 * Timing check of the real stepper firmware (stepper.elf) in the simavr
 * ATmega328P simulator. No hardware involved: this records every edge on
 * PD7 (STEP), PD6 (DIR) and PD5 (EN) with its CPU cycle number and checks
 * the TB6600 timing requirements and the ping-pong positions.
 *
 *   make -C ../stepper_library && cc sim_timing.c -lsimavr -lelf -o sim_timing
 *   ./sim_timing ../stepper_library/stepper.elf [seconds]
 */
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/avr_ioport.h>

#include <stdio.h>
#include <stdlib.h>

#define F_CPU 16000000.0
#define MAX_EDGES 400000   /* enough for ~10 s at 120 RPM */

typedef struct { avr_cycle_count_t t; int pin; int level; } edge_t;
static edge_t edges[MAX_EDGES];
static int n_edges;
static avr_t* avr;

static void on_pin(struct avr_irq_t* irq, uint32_t value, void* param)
{
    (void)irq;
    if (n_edges < MAX_EDGES) {
        edges[n_edges++] = (edge_t){avr->cycle, (int)(intptr_t)param, (int)(value & 1)};
    }
}

static double us(avr_cycle_count_t c) { return c * 1e6 / F_CPU; }

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "../stepper_library/stepper.elf";
    double seconds = argc > 2 ? atof(argv[2]) : 3.5;

    elf_firmware_t fw = {0};
    if (elf_read_firmware(path, &fw) != 0) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    avr = avr_make_mcu_by_name("atmega328p");
    if (!avr) { fprintf(stderr, "no atmega328p core\n"); return 1; }
    avr_init(avr);
    avr->frequency = 16000000;
    avr_load_firmware(avr, &fw);
    avr->log = 0;

    for (int pin = 5; pin <= 7; pin++) {
        avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), pin), on_pin, (void*)(intptr_t)pin);
    }

    const avr_cycle_count_t end = (avr_cycle_count_t)(seconds * F_CPU);
    while (avr->cycle < end) {
        int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) { fprintf(stderr, "cpu stopped: %d\n", state); return 1; }
    }

    // --- analyse ----------------------------------------------------------
    // A "move" is the run of steps between two DIR changes (ping-pong).
    enum { MAX_MOVES = 16 };
    long   moveSteps[MAX_MOVES]     = {0};
    double moveFirst[MAX_MOVES]     = {0};   // interval between step 1 and 2
    double moveFastest[MAX_MOVES]   = {0};
    double moveLast[MAX_MOVES]      = {0};   // interval before the last step
    int    move = 0;

    int dir = 0, en = 0, step = 0, awaitingFirstStep = 0, sawDirChange = 0;
    avr_cycle_count_t lastRise = 0, lastDirChange = 0;
    double hiMin = 1e9, hiMax = 0, dirSetupMin = 1e9;
    long position = 0, maxPos = 0, minPos = 0, stepsTotal = 0, stepsWhileDisabled = 0;

    for (int i = 0; i < n_edges; i++) {
        edge_t e = edges[i];
        if (e.pin == 5) { en = e.level; continue; }
        if (e.pin == 6) {
            if (e.level != dir) {
                if (moveSteps[move] > 0 && move < MAX_MOVES - 1) move++;
                lastDirChange = e.t; sawDirChange = 1; awaitingFirstStep = 1;
            }
            dir = e.level;
            continue;
        }
        if (e.level && !step) {                      // STEP rising edge = one step
            if (awaitingFirstStep) {
                double s = us(e.t - lastDirChange);
                if (s < dirSetupMin) dirSetupMin = s;
                awaitingFirstStep = 0;
            }
            if (moveSteps[move] > 0) {
                double iv = us(e.t - lastRise);
                if (moveSteps[move] == 1) moveFirst[move] = iv;
                if (moveFastest[move] == 0 || iv < moveFastest[move]) moveFastest[move] = iv;
                moveLast[move] = iv;
            }
            if (!en) stepsWhileDisabled++;
            position += dir ? -1 : 1;
            if (position > maxPos) maxPos = position;
            if (position < minPos) minPos = position;
            stepsTotal++;
            moveSteps[move]++;
            lastRise = e.t;
        } else if (!e.level && step) {               // falling edge
            double hi = us(e.t - lastRise);
            if (hi < hiMin) hiMin = hi;
            if (hi > hiMax) hiMax = hi;
        }
        step = e.level;
    }

    printf("simulated %.2f s on ATmega328P @ 16 MHz, %d pin edges recorded\n", seconds, n_edges);
    printf("steps total             : %ld (while EN low: %ld)\n", stepsTotal, stepsWhileDisabled);
    printf("position range          : %ld .. %ld steps (final %ld)\n", minPos, maxPos, position);
    printf("STEP high time          : %.3f .. %.3f us\n", hiMin, hiMax);
    printf("DIR change -> next STEP : %.1f us (min)\n", dirSetupMin);
    for (int m = 0; m <= move; m++) {
        printf("move %d: %5ld steps %s, intervals: first %.1f us, fastest %.1f us, last %.1f us\n",
               m + 1, moveSteps[m], (m % 2) ? "CCW" : "CW ", moveFirst[m], moveFastest[m], moveLast[m]);
    }

    int ok = 1;
    ok &= (hiMin >= 4.0 && hiMax <= 6.0);        // 5 us +-1 us
    ok &= (!sawDirChange || dirSetupMin >= 20.0);
    ok &= (stepsWhileDisabled == 0);
    printf("%s\n", ok ? "RESULT: timing requirements met" : "RESULT: FAILED");
    return ok ? 0 : 2;
}
