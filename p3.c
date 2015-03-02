/* Protocol 3 (par) allows unidirectional data flow over an unreliable
 * channel.
 *
 * To compile: cc -o protocol3 p3.c simulator.o
 * To run: protocol3 events timeout  pct_loss  pct_cksum  debug_flags
 *
 * Written by Andrew S. Tanenbaum
 * Revised by Shivakant Mishra
 */



#define MAX_SEQ 1	/* must be 1 for protocol 3 */
#define MAX_PROTOCOL 6          /* highest protocol being simulated */
                                /* THIS SHOULD BE REMOVED */
typedef enum  {frame_arrival, cksum_err, timeout} event_type;
#include <stdio.h>
#include <stdlib.h>
#include "protocol.h"

void sender3(void);
void receiver3(void);

int main(int argc, char *argv[])
{
  int timeout_interval, pkt_loss, garbled, debug_flags;
  long event;

  if (!parse_first_five_parameters(argc, argv, &event, &timeout_interval,
                                   &pkt_loss, &garbled, &debug_flags))  {
    printf ("Usage: p3 events timeout loss cksum debug\n");
    exit(1);
  }

  printf("\n\n Simulating Protocol 3\n");
  start_simulator(sender3, receiver3, event, timeout_interval, pkt_loss, garbled, debug_flags);
}

void sender3(void)
{
  seq_nr next_frame_to_send;	/* seq number of next outgoing frame */
  frame s;		/* scratch variable */
  packet buffer;	/* buffer for an outbound packet */
  event_type event;

 /* put protocolnumber and process id in log */                /*JH*/
  sprintf(logbuf,"XXX3 protocol3 Sender, pid=%d\n", getpid()); /*JH*/
  flog_string(logbuf);                                         /*JH*/

  next_frame_to_send = 0;	/* initialize outbound sequence numbers */
  from_network_layer(&buffer);	/* fetch first packet */
  while (true) {
        init_frame(&s);
        s.info = buffer;	/* construct a frame for transmission */
        s.seq = next_frame_to_send;	/* insert sequence number in frame */
        to_physical_layer(&s);	/* send it on its way */
        start_timer(s.seq);	/* if answer takes too long, time out */
        wait_for_event(&event);	/* frame_arrival, cksum_err, timeout */
        if (event == frame_arrival) {
                from_physical_layer(&s);	/* get the acknowledgement */
                if (s.ack == next_frame_to_send) {
                        from_network_layer(&buffer);	/* get the next one to send */
                        inc(next_frame_to_send);	/* invert next_frame_to_send */
                }
        }
  }
}

void receiver3(void)
{
  seq_nr frame_expected;
  frame r, s;
  event_type event;

 /* put protocolnumber and process id in log */                /*JH*/
  sprintf(logbuf,"XXX3 protocol3 Receiver, pid=%d\n", getpid()); /*JH*/
  flog_string(logbuf);                                         /*JH*/

 frame_expected = 0;
  while (true) {
        wait_for_event(&event);	/* possibilities: frame_arrival, cksum_err */
        if (event == frame_arrival) {
                /* A valid frame has arrived. */
                from_physical_layer(&r);	/* go get the newly arrived frame */
                if (r.seq == frame_expected) {
                        /* This is what we have been waiting for. */
                        to_network_layer(&r.info);	/* pass the data to the network layer */
                        inc(frame_expected);	/* next time expect the other sequence nr */
                }
                init_frame(&s);
                s.ack = 1 - frame_expected;	/* tell which frame is being acked */
                to_physical_layer(&s);	/* only the ack field is use */
        }
  }
}
