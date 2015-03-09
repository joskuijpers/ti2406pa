/* Protocol 6 (nonsequential receive) accepts frames out of order, but
 * passes packets to the network layer in order. Associated with each
 * outstanding frame is a timer. When the timer goes off, only that frame is
 * retransmitted, not all the outstanding frames, as in protocol 5.
 *
 * To compile: cc -o protocol6 p6.c simulator.o
 * To run: protocol6 events timeout  pct_loss  pct_cksum  debug_flags
 *
 * Written by Andrew S. Tanenbaum
 * Revised by Shivakant Mishra
 */

#define MAX_SEQ 7	/* should be 2^n - 1 */
#define NR_BUFS ((MAX_SEQ + 1) / 2)
/* changed from MAX_SEQ+1 / 2 ,and back */ /*JH*/
typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready, ack_timeout} event_type;
#include <unistd.h>
#include "protocol.h"
boolean no_nak = true;	/* no nak has been sent yet */

static boolean between(seq_nr a, seq_nr b, seq_nr c)
{
    /* Same as between in protocol5, but shorter and more obscure. */
    return ((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a));
}

static void send_frame(frame_kind fk, seq_nr frame_nr, seq_nr frame_expected, packet buffer[])
{
    /* Construct and send a data, ack, or nak frame. */
    frame s;	/* scratch variable */

    s.kind = fk;	/* kind == data, ack, or nak */
    if (fk == data) s.info = buffer[frame_nr % NR_BUFS];
    s.seq = frame_nr;	/* only meaningful for data frames */
    s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);
    if (fk == nak) no_nak = false;	/* one nak per frame, please */
    to_physical_layer(&s);	/* transmit the frame */
    /*  if (fk == data) start_timer(frame_nr % NR_BUFS); */
    if (fk == data) start_timer(frame_nr); /*JH*/

    stop_ack_timer();	/* no need for separate ack frame */
}

void protocol6(void)
{
    seq_nr ack_expected;	/* lower edge of sender's window */
    seq_nr next_frame_to_send;	/* upper edge of sender's window + 1 */
    seq_nr frame_expected;	/* lower edge of receiver's window */
    seq_nr too_far;	/* upper edge of receiver's window + 1 */
    int i;	/* index into buffer pool */
    frame r;	/* scratch variable */
    packet out_buf[NR_BUFS];	/* buffers for the outbound stream */
    packet in_buf[NR_BUFS];	/* buffers for the inbound stream */
    boolean arrived[NR_BUFS];	/* inbound bit map */
    seq_nr nbuffered;	/* how many output buffers currently used */
    event_type event;

    /* put protocolnumber and process id in logfile */      /*JH*/
    sprintf(logbuf,"XXX6 protocol6, pid=%d\n", getpid()); /*JH*/
    flog_string(logbuf);                                  /*JH*/

    enable_network_layer();	/* initialize */
    ack_expected = 0;	/* next ack expected on the inbound stream */
    next_frame_to_send = 0;	/* number of next outgoing frame */
    frame_expected = 0;	/* frame number expected */
    too_far = NR_BUFS;	/* receiver's upper window + 1 */
    nbuffered = 0;	/* initially no packets are buffered */

    for (i = 0; i < NR_BUFS; i++) arrived[i] = false;
    while (true) {
        wait_for_event(&event);	/* five possibilities: see event_type above */
        switch(event) {
            case network_layer_ready:	/* accept, save, and transmit a new frame */
                nbuffered = nbuffered + 1;	/* expand the window */
                from_network_layer(&out_buf[next_frame_to_send % NR_BUFS]); /* fetch new packet */
                send_frame(data, next_frame_to_send, frame_expected, out_buf);	/* transmit the frame */
                inc(next_frame_to_send);	/* advance upper window edge */
                break;

            case frame_arrival:	/* a data or control frame has arrived */
                from_physical_layer(&r);	/* fetch incoming frame from physical layer */
                if (r.kind == data) {
                    /* An undamaged frame has arrived. */
                    if ((r.seq != frame_expected) && no_nak)
                        send_frame(nak, 0, frame_expected, out_buf); else start_ack_timer();

                    if (between(frame_expected, r.seq, too_far) && (arrived[r.seq%NR_BUFS] == false)) {
                        /* Frames may be accepted in any order. */
                        arrived[r.seq % NR_BUFS] = true;	/* mark buffer as full */
                        in_buf[r.seq % NR_BUFS] = r.info;	/* insert data into buffer */
                        while (arrived[frame_expected % NR_BUFS]) {
                            /* Pass frames and advance window. */
                            to_network_layer(&in_buf[frame_expected % NR_BUFS]);


                            no_nak = true;
                            arrived[frame_expected % NR_BUFS] = false;
                            inc(frame_expected);	/* advance lower edge of receiver's window */
                            inc(too_far);	/* advance upper edge of receiver's window */
                            start_ack_timer();	/* to see if (a separate ack is needed */
                        }
                    }
                }
                if((r.kind==nak) && between(ack_expected,(r.ack+1)%(MAX_SEQ+1),next_frame_to_send))
                    send_frame(data, (r.ack+1) % (MAX_SEQ + 1), frame_expected, out_buf);

                while (between(ack_expected, r.ack, next_frame_to_send)) {
                    nbuffered = nbuffered - 1;	/* handle piggybacked ack */
                    stop_timer(ack_expected);/*JH*/	/* frame arrived intact */
                    inc(ack_expected);	/* advance lower edge of sender's window */
                }
                break;

            case cksum_err: if (no_nak) send_frame(nak, 0, frame_expected, out_buf); break;	/* damaged frame */
            case timeout: send_frame(data, get_timedout_seqnr(), frame_expected, out_buf); break;	/* we timed out */
            case ack_timeout: send_frame(ack,0,frame_expected, out_buf);	/* ack timer expired; send ack */
        }

        if (nbuffered < NR_BUFS) enable_network_layer(); else disable_network_layer();
    }
}

int main (int argc, char *argv[])
{
    int timeout_interval, pkt_loss, garbled, debug_flags;
    long event;

    if (!parse_first_five_parameters(argc, argv, &event, &timeout_interval,
                                     &pkt_loss, &garbled, &debug_flags)) {
        printf ("Usage: p6 events timeout loss cksum debug\n");
        exit(1);
    }

    init_max_seqnr(MAX_SEQ + 1);
    printf("\n\n Simulating Protocol 6\n");
    start_simulator(protocol6, protocol6, event, timeout_interval, pkt_loss, garbled, debug_flags);

    return 0;
}
