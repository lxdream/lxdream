#include "../lib.h"
#define TMU_CHANNEL 2

#define TOCR 0xFFD80000  /* Output control register */
#define TSTR 0xFFD80004  /* Start register */
#define TCOR(c) (0xFFD80008 + (c*12))  /* Constant register */
#define TCNT(c) (0xFFD8000C + (c*12))  /* Count register */
#define TCR(c)  (0xFFD80010 + (c*12))  /* Control register */

/**
 * Initialize the on-chip timer controller. We snag TMU channel 2 in its
 * highest resolution mode, and start it counting down from max_int. 
 */
void timer_start() {
    unsigned int val = long_read(TSTR);
    long_write( TSTR, val & (~(1<<TMU_CHANNEL)) ); /* Stop counter */
    long_write( TCOR(TMU_CHANNEL), 0xFFFFFFFF );
    long_write( TCNT(TMU_CHANNEL), 0xFFFFFFFF );
    long_write( TCR(TMU_CHANNEL), 0x00000000 );
    long_write( TSTR, val | (1<<TMU_CHANNEL) );
}

/**
 * Report the current value of TMU2.
 */
long timer_gettime() {
    return long_read(TCNT(TMU_CHANNEL));
}

/**
 * Stop TMU2 and report the current value.
 */
long timer_stop() {
    long_write( TSTR, long_read(TSTR) & (~(1<<TMU_CHANNEL)) );
    return long_read( TCNT(TMU_CHANNEL) );
}


/**
 * Convert the supplied timer value to a number of micro seconds since the timer
 * was started.
 */
long timer_to_microsecs( long value ) {
    return value;
}
