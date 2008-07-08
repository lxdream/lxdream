#include "lib.h"
#define TMU_CHANNEL 2
#define BASE_TICKS_PER_US 200
#define CLOCK_DIVIDER 16
#define TOCR 0xFFD80000  /* Output control register */
#define TSTR 0xFFD80004  /* Start register */
#define TCOR(c) (0xFFD80008 + (c*12))  /* Constant register */
#define TCNT(c) (0xFFD8000C + (c*12))  /* Count register */
#define TCR(c)  (0xFFD80010 + (c*12))  /* Control register */

/**
 * Initialize the on-chip timer controller. We snag TMU channel 2 in its
 * highest resolution mode, and start it counting down from max_int. 
 */
void timer_init() {
    unsigned int val = byte_read(TSTR);
    byte_write( TSTR, val & (~(1<<TMU_CHANNEL)) ); /* Stop counter */
    long_write( TCOR(TMU_CHANNEL), 0xFFFFFFFF );
    long_write( TCNT(TMU_CHANNEL), 0xFFFFFFFF );
    word_write( TCR(TMU_CHANNEL), 0x00000000 );
}

void timer_run() {
    byte_write( TSTR, byte_read(TSTR) | (1<<TMU_CHANNEL) );
}

void timer_start() {
    timer_init();
    timer_run();
}

/**
 * Report the current value of TMU2.
 */
unsigned int timer_gettime() {
    return long_read(TCNT(TMU_CHANNEL));
}

/**
 * Stop TMU2 and report the current value.
 */
unsigned int timer_stop() {
    long_write( TSTR, long_read(TSTR) & (~(1<<TMU_CHANNEL)) );
    return long_read( TCNT(TMU_CHANNEL) );
}


/**
 * Convert the supplied timer value to a number of micro seconds since the timer
 * was started.
 */
unsigned int timer_to_microsecs( unsigned int value ) {
    return (0xFFFFFFFF - value) * CLOCK_DIVIDER / BASE_TICKS_PER_US;
}

unsigned int timer_gettime_us() {
    return timer_to_microsecs( timer_gettime() );
}
