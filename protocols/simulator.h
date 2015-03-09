/* Data structures. */

typedef enum {
    no_event = -1,
    frame_arrival,
    cksum_err,
    timeout,
    network_layer_ready,
    ack_timeout
} event_type;

#include "protocol.h"
typedef unsigned long bigint;	/* bigint integer type available */

/* General constants */
#define TICK_SIZE (sizeof(tick))
#define DELTA 10		/* must be greater than NR_TIMERS so each
                         * timer can go off at a separate tick.
                        */

/* Reply codes sent by workers back to main. */
#define OK      1		/* normal response */
#define NOTHING 2		/* worker did nothing */

/* Simulation parameters. */
bigint timeout_interval;	/* timeout interval in ticks */
int pkt_loss;			/* controls packet loss rate: 0 to 990 */
int garbled;			/* control cksum error rate: 0 to 990 */
int debug_flags;		/* debug flags */
void (*proc1)(void);
void (*proc2)(void);

/* File descriptors for pipes. */
int r1, w1, r2, w2, r3, w3, r4, w4, r5, w5, r6, w6;

/* Filled in by main to tell each worker its id. */
int id;				/* 0 or 1 */

bigint zero;

int mrfd, mwfd, prfd;
