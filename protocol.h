#define MAX_PKT 4	/* determines packet size in bytes */

#include <stdio.h>
#include <stdlib.h>

typedef enum {false, true} boolean;	/* boolean type */
typedef unsigned int seq_nr;	/* sequence or ack numbers */
typedef struct {unsigned char data[MAX_PKT];} packet;	/* packet definition */
typedef enum {data, ack, nak} frame_kind;	/* frame_kind definition */

typedef struct {	/* frames are transported in this layer */
  frame_kind kind;	/* what kind of a frame is it? */
  seq_nr seq;   	/* sequence number */
  seq_nr ack;   	/* acknowledgement number */
  packet info;  	/* the network layer packet */
} frame;

/* start_simulator initializes various simulator parameters and starts the
 * simulation. Control will return from this function only after the simulation
 * has been completed.
 *
 * Parameter description:
 *   proc1: function name of the protocol function that is executed by one
 *          end of the link.
 *   proc2: function name of the protocol function that is executed by the
 *          other end of the link. For several datalink protocols, proc1 and
 *          proc2 may be same, e.g. protocols 4, 5, and 6.
 *   event: how long (in terms of number of events) the simulation should run.
 *   tm_out: timeout interval in ticks.
 *   pk_loss: percentage of frames that are lost (0-99).
 *   grb: percentage of arriving frames that are bad (due to checksum errors).
 *   d_flags: enables various tracing flags.
 *              1        frames sent
 *              2        frames received
 *              4        timeouts
 *              8        periodic printout for use with long runs
 */
void start_simulator(void (*proc1)(), void (*proc2)(), long event,
                     int tm_out, int pk_loss, int grb, int d_flags);

/* init_frame fills in default initial values in a frame. Protocols should
 * call this function before creating a new frame. Protocols may later update
 * some of these fields. This initialization is not strictly needed, but
 * makes the simulation trace look better, showing unused fields as zeros.
 */
void init_frame(frame *s);

/* Wait for an event to happen; return its type in event. */
void wait_for_event(event_type *event);

/* Fetch a packet from the network layer for transmission on the channel. */
void from_network_layer(packet *p);

/* Deliver information from an inbound frame to the network layer. */
void to_network_layer(packet *p);

/* Go get an inbound frame from the physical layer and copy it to r. */
void from_physical_layer(frame *r);

/* Pass the frame to the physical layer for transmission. */
void to_physical_layer(frame *s);

/* Start the clock running and enable the timeout event. */
void start_timer(seq_nr k);

/* Stop the clock and disable the timeout event. */
void stop_timer(seq_nr k);

/* Start an auxiliary timer and enable the ack_timeout event. */
void start_ack_timer(void);

/* Stop the auxiliary timer and disable the ack_timeout event. */
void stop_ack_timer(void);

/* Allow the network layer to cause a network_layer_ready event. */
void enable_network_layer(void);

/* Forbid the network layer from causing a network_layer_ready event. */
void disable_network_layer(void);

/* In case of a timeout event, it is possible to find out the sequence
 * number of the frame that timed out (this is the sequence number parameter
 * in the start_timer function). For this, the simulator must know the maximum
 * possible value that a sequence number may have. Function init_max_seqnr()
 * tells the simulator this value. This function must be called before
 * start_simulator() function. When a timeout event occurs, function
 * get_timedout_seqnr() returns the sequence number of the frame that timed out.
 */
void init_max_seqnr(unsigned int o);
unsigned int get_timedout_seqnr(void);

/* Help function for protocol designers to parse first five command-line
 * parameters that the simulator needs. This assumes that the first five
 * command-line parameters are the simulator parameters. If a protocol needs
 * any additional parameters, it needs to parse them separately.
 */
int parse_first_five_parameters(int argc, char *argv[], long *event,
                                int *timeout_interval, int *pkt_loss,
                                int *garbled, int *debug_flags);

/* copy a buffer to the log file of the process */
void flog_string(char *logbuf);

/* Macro inc is expanded in-line: Increment k circularly. */
#define inc(k) if (k < MAX_SEQ) k = k + 1; else k = 0

char logbuf[255];      /* a buffer to present strings to flog_string */
