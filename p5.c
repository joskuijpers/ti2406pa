/* Protocol 5 (pipelining) allows multiple outstanding frames. The sender
 * may transmit up to MAX_SEQ frames without waiting for an ack. In addition,
 * unlike the previous protocols, the network layer is not assumed to have
 * a new packet all the time. Instead, the network layer causes a
 * network_layer_ready event when there is a packet to send.
 *
 * To compile: cc -o protocol5 p5.c simulator.o
 * To run: protocol5 events timeout  pct_loss  pct_cksum  debug_flags
 *
 * Written by Andrew S. Tanenbaum
 * Revised by Shivakant Mishra
 */

#define MAX_SEQ 7	/* should be 2^n - 1 */
typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready} event_type;
#include "protocol.h"

static boolean between(seq_nr a, seq_nr b, seq_nr c)
{
/* Return true if (a <=b < c circularly; false otherwise. */
  if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
        return(true);
    else
        return(false);
}

static void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer[])
{
/* Construct and send a data frame. */
  frame s;	/* scratch variable */

  init_frame(&s);
  s.kind = data;
  s.info = buffer[frame_nr];	/* insert packet into frame */
  s.seq = frame_nr;	/* insert sequence number into frame */
  s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);	/* piggyback ack */
  to_physical_layer(&s);	/* transmit the frame */
  start_timer(frame_nr);	/* start the timer running */
}

void protocol5(void)
{
  seq_nr next_frame_to_send;	/* MAX_SEQ > 1; used for outbound stream */
  seq_nr ack_expected;	/* oldest frame as yet unacknowledged */
  seq_nr frame_expected;	/* next frame expected on inbound stream */
  frame r;	/* scratch variable */
  packet buffer[MAX_SEQ+1];	/* buffers for the outbound stream */
  seq_nr nbuffered;	/* # output buffers currently in use */
  seq_nr i;	/* used to index into the buffer array */
  event_type event;

 /* put protocol number and process id to log file */   /*JH*/
  sprintf(logbuf,"XXX5 protocol5, pid=%d\n", getpid()); /*JH*/
  flog_string(logbuf);                                  /*JH*/


  enable_network_layer();	/* allow network_layer_ready events */
  ack_expected = 0;	/* next ack expected inbound */
  next_frame_to_send = 0;	/* next frame going out */
  frame_expected = 0;	/* number of frame expected inbound */
  nbuffered = 0;	/* initially no packets are buffered */

  while (true) {
     wait_for_event(&event);	/* four possibilities: see event_type above */

     switch(event) {
        case network_layer_ready:	/* the network layer has a packet to send */
                /* Accept, save, and transmit a new frame. */
                from_network_layer(&buffer[next_frame_to_send]); /* fetch new packet */
                nbuffered = nbuffered + 1;	/* expand the sender's window */
                send_data(next_frame_to_send, frame_expected, buffer);	/* transmit the frame */
                inc(next_frame_to_send);	/* advance sender's upper window edge */
                break;

        case frame_arrival:	/* a data or control frame has arrived */
                from_physical_layer(&r);	/* get incoming frame from physical layer */

                if (r.seq == frame_expected) {
                        /* Frames are accepted only in order. */
                        to_network_layer(&r.info);	/* pass packet to network layer */
                        inc(frame_expected);	/* advance lower edge of receiver's window */
                 }

                 /* Ack n implies n - 1, n - 2, etc.  Check for this. */
                while (between(ack_expected, r.ack, next_frame_to_send)) {
                        /* Handle piggybacked ack. */
                        nbuffered = nbuffered - 1;	/* one frame fewer buffered */
                        stop_timer(ack_expected);	/* frame arrived intact; stop timer */
                        inc(ack_expected);	/* contract sender's window */
                }
                break;

        case cksum_err: ;	/* just ignore bad frames */
                break;

        case timeout:	/* trouble; retransmit all outstanding frames */
                next_frame_to_send = ack_expected;	/* start retransmitting here */
                for (i = 1; i <= nbuffered; i++) {
                        send_data(next_frame_to_send, frame_expected, buffer);	/* resend 1 frame */
                        inc(next_frame_to_send);	/* prepare to send the next one */
                }
     }

     if (nbuffered < MAX_SEQ)
        enable_network_layer();
     else
        disable_network_layer();
  }
}

int main(int argc, char *argv[])
{
  int timeout_interval, pkt_loss, garbled, debug_flags;
  long event;

  if (!parse_first_five_parameters(argc, argv, &event, &timeout_interval,
                                   &pkt_loss, &garbled, &debug_flags))  {
    printf ("Usage: p5 events timeout loss cksum debug\n");
    exit(1);
  }

  printf("\n\n Simulating Protocol 5\n");
  start_simulator(protocol5, protocol5, event, timeout_interval, pkt_loss, garbled, debug_flags);
}

