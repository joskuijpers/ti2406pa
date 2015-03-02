/* Protocol 4 (sliding window) is bidirectional and is more robust than
 * protocol 3.
 *
 * To compile: cc -o protocol4 p4.c simulator.o
 * To run: protocol4 events timeout  pct_loss  pct_cksum  debug_flags
 *
 * Written by Andrew S. Tanenbaum
 * Revised by Shivakant Mishra
 */


#define MAX_SEQ 1
typedef enum {frame_arrival, cksum_err, timeout} event_type;
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "protocol.h"

void protocol4 (void)
{
    seq_nr next_frame_to_send;	/* 0 or 1 only */
    seq_nr frame_expected;	/* 0 or 1 only */
    frame r, s;	/* scratch variables */
    packet buffer;	/* current packet being sent */
    event_type event;

    /* put protocol number and process id in log */
    sprintf(logbuf,"XXX4 protocol4, pid=%d\n", getpid());
    flog_string(logbuf);

    next_frame_to_send = 0;	/* next frame on the outbound stream */
    frame_expected = 0;	/* number of frame arriving frame expected */
    from_network_layer(&buffer);	/* fetch a packet from the network layer */
    init_frame(&s);
    s.kind = data;
    s.info = buffer;	/* prepare to send the initial frame */
    s.seq = next_frame_to_send;	/* insert sequence number into frame */
    s.ack = 1 - frame_expected;	/* piggybacked ack */
    to_physical_layer(&s);	/* transmit the frame */
    start_timer(s.seq);	/* start the timer running */

    while (true) {
        wait_for_event(&event);	/* could be: frame_arrival, cksum_err, timeout */

        if ((event != frame_arrival) && (event != cksum_err) && (event != timeout))
            printf("\n SOMETHING WEIRD\n");

        if (event == frame_arrival) { /* a frame has arrived undamaged. */
            from_physical_layer(&r);	/* go get it */

            if (r.seq == frame_expected) {
                /* Handle inbound frame stream. */
                to_network_layer(&r.info);	/* pass packet to network layer */
                inc(frame_expected);	/* invert sequence number expected next */
            }

            if (r.ack == next_frame_to_send) { /* handle outbound frame stream. */
                stop_timer(r.ack); /*JH*/ /* necessary to prevent superfluous timeouts */
                from_network_layer(&buffer);	/* fetch new packet from network layer */
                inc(next_frame_to_send);	/* invert sender's sequence number */
            }
        }
        
        init_frame(&s);
        s.kind = data;
        s.info = buffer;	/* construct outbound frame */
        s.seq = next_frame_to_send;	/* insert sequence number into it */
        s.ack = 1 - frame_expected;	/* seq number of last received frame */
        to_physical_layer(&s);	/* transmit a frame */
        start_timer(s.seq);	/* start the timer running */
    }
}

int main (int argc, char *argv[])
{
    int timeout_interval, pkt_loss, garbled, debug_flags;
    long event;

    if (!parse_first_five_parameters(argc, argv, &event, &timeout_interval,
                                     &pkt_loss, &garbled, &debug_flags))  {
        printf ("Usage: p4 events timeout loss cksum debug\n");
        exit(1);
    }

    printf("\n\n Simulating Protocol 4\n");
    start_simulator(protocol4, protocol4, event, timeout_interval, pkt_loss, garbled, debug_flags);

    return 0;
}

