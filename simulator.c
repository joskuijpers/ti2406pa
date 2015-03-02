/* Simulator for datalink protocols. 
 *    Written by Andrew S. Tanenbaum.
 *    Revised by Shivakant Mishra.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>  /*JH*/
#include <errno.h>  /*JH*/
#include <time.h>   /*JH*/
#include "simulator.h"

#define NR_TIMERS 8             /* number of timers; this should be greater
                                   than half the number of sequence numbers. */
#define MAX_QUEUE 1000            /* max number of buffered frames */
#define NO_EVENT -1             /* no event possible */
#define FRAME_SIZE (sizeof(frame))
#define BYTE 0377               /* byte mask */
#define UINT_MAX  0xFFFFFFFF    /* maximum value of an unsigned 32-bit int */
#define INTERVAL 100000         /* interval for periodic printing */
#define AUX 2                   /* aux timeout is main timeout/AUX */

/* DEBUG MASKS */
#define SENDS        0x0001     /* frames sent */
#define RECEIVES     0x0002     /* frames received */
#define TIMEOUTS     0x0004     /* timeouts */
#define PERIODIC     0x0008     /* periodic printout for use with long runs */

#define DEADLOCK (3 * timeout_interval)	/* defines what a deadlock is */
#define MANY 256		/* big enough to clear pipe at the end */

/* Status variables used by the workers, M0 and M1. */
bigint ack_timer[NR_TIMERS];    /* ack timers */
unsigned int seqs[NR_TIMERS];   /* last sequence number sent per timer */
bigint lowest_timer;            /* lowest of the timers */
bigint aux_timer;               /* value of the auxiliary timer */
int network_layer_status;       /* 0 is disabled, 1 is enabled */
unsigned int next_net_pkt;      /* seq of next network packet to fetch */
unsigned int last_pkt_given= 0xFFFFFFFF;        /* seq of last pkt delivered*/
frame last_frame;               /* arrive frames are kept here */
int offset;                     /* to prevent multiple timeouts on same tick*/
bigint tick;                    /* current time */
int retransmitting;             /* flag that is set on a timeout */
int nseqs = NR_TIMERS;          /* must be MAX_SEQ + 1 after startup */
unsigned int oldest_frame = NR_TIMERS;  /* tells which frame timed out */

char *badgood[] = {"bad ", "good"};
char *tag[] = {"Data", "Ack ", "Nak "};

/* Statistics */
int data_sent;                  /* number of data frames sent */
int data_retransmitted;         /* number of data frames retransmitted */
int data_lost;                  /* number of data frames lost */
int data_not_lost;              /* number of data frames not lost */
int good_data_recd;             /* number of data frames received */
int cksum_data_recd;            /* number of bad data frames received */

int acks_sent;                  /* number of ack frames sent */
int acks_lost;                  /* number of ack frames lost */
int acks_not_lost;              /* number of ack frames not lost */
int good_acks_recd;             /* number of ack frames received */
int cksum_acks_recd;            /* number of bad ack frames received */

int payloads_accepted;          /* number of pkts passed to network layer */
int timeouts;                   /* number of timeouts */
int ack_timeouts;               /* number of ack timeouts */

/* Incoming frames are buffered here for later processing. */
frame queue[MAX_QUEUE];         /* buffered incoming frames */
frame *inp = &queue[0];         /* where to put the next frame */
frame *outp = &queue[0];        /* where to remove the next frame from */
int nframes=0;  /*JH*/          /* number of queued frames */

bigint tick = 0;		/* the current time, measured in events */
bigint last_tick;		/* when to stop the simulation */
int exited[2];			/* set if exited (for each worker) */
int hanging[2];			/* # times a process has done nothing */
struct sigaction act, oact;

/* Logfiles */
char version[]="1.0";  /*JH*/               /* VERSION */
time_t curtime;        /*JH*/
struct tm *loctime;    /*JH*/
FILE *flog;            /*JH*/
char logfile[]="logX"; /*JH*/ 
/* for different processes x will be replaced by m or 0 or 1 */
                               

/* Prototypes. */
void start_simulator(void (*p1)(), void (*p2)(), long event, int tm_out, int pk_loss, int grb, int d_flags);
void set_up_pipes(void);
void fork_off_workers(void);
void terminate(char *s);

void init_max_seqnr(unsigned int o);
unsigned int get_timedout_seqnr(void);
void wait_for_event(event_type *event);
void init_frame(frame *s);
void queue_frames(void);
int pick_event(void);
event_type frametype(void);
void from_network_layer(packet *p);
void to_network_layer(packet *p);
void from_physical_layer(frame *r);
void to_physical_layer(frame *s);
void start_timer(seq_nr k);
void stop_timer(seq_nr k);
void start_ack_timer(void);
void stop_ack_timer(void);
void enable_network_layer(void);
void disable_network_layer(void);
int check_timers(void);
int check_ack_timer(void);
void flog_frame(frame *f, char sr);    /*JH*/
void flog_string(char *str_out);       /*JH*/
void print_queue(void);                /*JH*/
unsigned int pktnum(packet *p);
void fr(frame *f);
void recalc_timers(void);
void print_statistics(void);
void sim_error(char *s);
int parse_first_five_parameters(int argc, char *argv[], long *event, int *timeout_interval, int *pkt_loss, int *garbled, int *debug_flags);

void start_simulator(void (*p1)(), void (*p2)(), long event, int tm_out, int pk_loss, int grb, int d_flags)
{
/* The simulator has three processes: main(this process), M0, and M1, all of
 * which run independently.  Set them all up first.  Once set up, main
 * maintains the clock (tick), and picks a process to run.  Then it writes a
 * 32-bit word to that process to tell it to run.  The process sends back an
 * answer when it is done.  Main then picks another process, and the cycle
 * repeats.
 */

  int process = 0;		/* whose turn is it */
  int rfd, wfd;			/* file descriptor for talking to workers */
  bigint word;			/* message from worker */

  act.sa_handler = SIG_IGN;
  setvbuf(stdout, (char *) 0, _IONBF, (size_t) 0);	/* disable buffering*/

  proc1 = p1;
  proc2 = p2;
  /* Each event uses DELTA ticks to make it possible for each timeout to
   * occur at a different tick.  For example, with DELTA = 10, ticks will
   * occur at 0, 10, 20, etc.  This makes it possible to schedule multiple
   * events (e.g. in protocol 5), all at unique times, e.g. 1070, 1071, 1072,
   * 1073, etc.  This property is needed to make sure timers go off in the
   * order they were set.  As a consequence, internally, the variable tick
   * is bumped by DELTA on each event. Thus asking for a simulation run of
   * 1000 events will give 1000 events, but they internally they will be
   * called 0 to 10,000.
   */
  last_tick = DELTA * event; 

  /* Convert from external units to internal units so the user does not see
   * the internal units at all.
   */
  timeout_interval = DELTA * tm_out;

  /* Packet loss takes place at the sender.  Packets selected for being lost
   * are not put on the wire at all.  Internally, pkt_loss and garbled are
   * from 0 to 990 so they can be compared to 10 bit random numbers.  The
   * inaccuracy here is about 2.4% because 1000 != 1024.  In effect, it is
   * not possible to say that all packets are lost.  The most that can be
   * be lost is 990/1024.
   */

  pkt_loss = 10 * pk_loss; /* for our purposes, 1000 == 1024 */

  /* This arg tells what fraction of arriving packets are garbled.  Thus if
   * pkt_loss is 50 and garbled is 50, half of all packets (actually,
   * 500/1024 of all packets) will not be sent at all, and of the ones that
   * are sent, 500/1024 will arrive garbled.
   */

  garbled = 10 * grb; /* for our purposes, 1000 == 1024 */

  /* Turn tracing options on or off.  The bits are defined in worker.c. */
  debug_flags = d_flags;
  printf("\n\nEvents: %u    Parameters: %u %d %u\n",
      last_tick/DELTA, timeout_interval/DELTA, pkt_loss/10, garbled/10);

  set_up_pipes();		/* create five pipes */
  fork_off_workers();		/* fork off the worker processes */

  /* Main simulation loop. */
  while (tick <last_tick) {
	process = rand() & 1;		/* pick process to run: 0 or 1 */
	tick = tick + DELTA;
	rfd = (process == 0 ? r4 : r6);
	if (read(rfd, &word, TICK_SIZE) != TICK_SIZE) terminate("");
 /**/ fprintf(flog,"XM01 %d process=%d word=%d\n", tick/DELTA, process, word);
 /**/ fflush;
	if (word == OK) hanging[process] = 0;
	if (word == NOTHING) hanging[process] += DELTA;
	if (hanging[0] >= DEADLOCK && hanging[1] >= DEADLOCK)
		terminate("A deadlock has been detected");

	/* Write the time to the selected process to tell it to run. */
	wfd = (process == 0 ? w3 : w5);
	if (write(wfd, &tick, TICK_SIZE) != TICK_SIZE)
		terminate("Main could not write to worker");
 /**/ fprintf(flog,"XM02 %d process=%d\n", tick/DELTA, process);
 /**/ fflush;

  }

  /* Simulation run has finished. */
  terminate("End of simulation");
}


void set_up_pipes(void)
{
/* Create six pipes so main, M0 and M1 can communicate pairwise. */

  int fd[2];

  pipe(fd);  r1 = fd[0];  w1 = fd[1];	/* M0 to M1 for frames */
  pipe(fd);  r2 = fd[0];  w2 = fd[1];	/* M1 to M0 for frames */
  pipe(fd);  r3 = fd[0];  w3 = fd[1];	/* main to M0 for go-ahead */
  pipe(fd);  r4 = fd[0];  w4 = fd[1];	/* M0 to main to signal readiness */
  pipe(fd);  r5 = fd[0];  w5 = fd[1];	/* main to M1 for go-ahead */
  pipe(fd);  r6 = fd[0];  w6 = fd[1];	/* M1 to main to signal readiness */
}

void fork_off_workers(void)
{
/* Fork off the two workers, M0 and M1. */
  curtime=time(NULL);
  loctime=localtime(&curtime);
  if (fork() != 0) {
	/* This is the Parent.  It will become main, but first fork off M1. */
	if (fork() != 0) {
		/* This is main. */
		sigaction(SIGPIPE, &act, &oact);
	        setvbuf(stdout, (char *)0, _IONBF, (size_t)0);/*don't buffer*/
		close(r1);
		close(w1);
		close(r2);
		close(w2);
		close(r3);
		close(w4);
		close(r5);
		close(w6);
        /* now open the log file */
                logfile[3]='M';
                if ((flog=fopen(logfile,"w"))==NULL)
                   sim_error("error in opening file logm");
                fprintf(flog,"XXX0 version:%s, logfile: %s, %s\n",
                        version, logfile, asctime(loctime));               
		return;
	} else {
		/* This is the code for M1. Run protocol. */
		sigaction(SIGPIPE, &act, &oact);
	        setvbuf(stdout, (char *)0, _IONBF, (size_t)0);/*don't buffer*/
		close(w1);
		close(r2);
		close(r3);
		close(w3);
		close(r4);
		close(w4);
		close(w5);
		close(r6);
                if (fcntl(r1,F_SETFL,O_NONBLOCK+O_ASYNC)<0) /*JH*/
                   sim_error("pipe initialization failed for M1");	
		id = 1;		/* M1 gets id 1 */
		mrfd = r5;	/* fd for reading time from main */
		mwfd = w6;	/* fd for writing reply to main */
		prfd = r1;	/* fd for reading frames from worker 0 */
                /* open the logfile for p1 */
                logfile[3]='1';
                if ((flog=fopen(logfile,"w"))==NULL)
                   sim_error("error in opening file log1");
                fprintf(flog,"XXX0 version:%s, logfile: %s, %s\n",
                        version, logfile, asctime(loctime));               
		(*proc2)();	/* call the user-defined protocol function */
                return;
	}
  } else {
	/* This is the code for M0. Run protocol. */
	sigaction(SIGPIPE, &act, &oact);
        setvbuf(stdout, (char *)0, _IONBF, (size_t)0);/*don't buffer*/
	close(r1);
	close(w2);
	close(w3);
	close(r4);
	close(r5);
	close(w5);
	close(r6);
        close(w6); /*jh */

        if (fcntl(r2,F_SETFL,O_NONBLOCK+O_ASYNC)<0) /*JH*/
            sim_error("pipe initialization failed for M1");	

	id = 0;		/* M0 gets id 0 */
	mrfd = r3;	/* fd for reading time from main */
	mwfd = w4;	/* fd for writing reply to main */
	prfd = r2;	/* fd for reading frames from worker 1 */
        /* open the logfile for process p0 */
        logfile[3]='0';
        if ((flog=fopen(logfile,"w"))==NULL)
            sim_error("error in opening file log1");

                fprintf(flog,"XXX0 version:%s, logfile: %s, %s\n",
                        version, logfile, asctime(loctime));               
	(*proc1)();	/* call the user-defined protocol function */
        return;
  }
}

void terminate(char *s)
{
/* End the simulation run by sending each worker a 32-bit zero command. */

  int n, k1, k2, res1[MANY], res2[MANY], eff, acc, sent;

  for (n = 0; n < MANY; n++) {res1[n] = 0; res2[n] = 0;}
  write(w3, &zero, TICK_SIZE);
  write(w5, &zero, TICK_SIZE);
  sleep(4);

  /* Clean out the pipe.  The zero word indicates start of statistics. */
  n = read(r4, res1, MANY*sizeof(int));
  k1 = 0;
  while (res1[k1] != 0) k1++;
  k1++;				/* res1[k1] = accepted, res1[k1+1] = sent */

  /* Clean out the other pipe and look for statistics. */
  n = read(r6, res2, MANY*sizeof(int));
  k2 = 0;
  while (res1[k2] != 0) k2++;
  k2++;				/* res1[k2] = accepted, res1[k2+1] = sent */

  if (strlen(s) > 0) {
	acc = res1[k1] + res2[k2];
	sent = res1[k1+1] + res2[k2+1];
	if (sent > 0) {
		eff = (100 * acc)/sent;
 	        printf("\nEfficiency (payloads accepted/data pkts sent) = %d%c\n", eff, '%');
	}
	printf("%s.  Time=%u\n",s, tick/DELTA);
  }
  exit(1);
 }

void init_max_seqnr(unsigned int o)
{
  nseqs = oldest_frame = o;
}

unsigned int get_timedout_seqnr(void)
{
  return(oldest_frame);
}

void wait_for_event(event_type *event)
{
/* Wait_for_event reads the pipe from main to get the time.  Then it
 * fstat's the pipe from the other worker to see if any
 * frames are there.  If so, if collects them all in the queue array.
 * Once the pipe is empty, it makes a decision about what to do next.
 */
 
 bigint ct, word = OK;

  offset = 0;			/* prevents two timeouts at the same tick */
  retransmitting = 0;		/* counts retransmissions */
  while (true) {
	queue_frames();		/* go get any newly arrived frames */
	if (write(mwfd, &word, TICK_SIZE) != TICK_SIZE) print_statistics();
 /**/ fprintf(flog,"XWF1 %d word=%d\n", tick/DELTA, word);
 /**/ fflush;
	if (read(mrfd, &ct, TICK_SIZE) != TICK_SIZE) print_statistics();
	if (ct == 0) print_statistics();
	tick = ct;		/* update time */
	if ((debug_flags & PERIODIC) && (tick%INTERVAL == 0))
		printf("Tick %u. Proc %d. Data sent=%d  Payloads accepted=%d  Timeouts=%d\n", tick/DELTA, id, data_sent, payloads_accepted, timeouts);

	/* Now pick event. */
	*event = pick_event();
	if (*event == NO_EVENT) {
		word = (lowest_timer == 0 ? NOTHING : OK);
		continue;
	}
	word = OK;
	if (*event == timeout) {
		timeouts++;
		retransmitting = 1;	/* enter retransmission mode */
        fprintf(flog,"XXX1%6d T%2d timeout for frame %d\n",
                 tick/DELTA, id, oldest_frame);
		if (debug_flags & TIMEOUTS)
		      printf("Tick %u. Proc %d got timeout for frame %d\n",
					       tick/DELTA, id, oldest_frame);
	}

	if (*event == ack_timeout) {
		ack_timeouts++;
		if (debug_flags & TIMEOUTS)
		      printf("Tick %u. Proc %d got ack timeout\n",
					       tick/DELTA, id);
	}
	return;
  }
}

void init_frame(frame *s)
{
  /* Fill in fields that that the simulator expects. Protocols may update
   * some of these fields. This filling is not strictly needed, but makes the
   * simulation trace look better, showing unused fields as zeros.
   */

  s->seq = 0;
  s->ack = 0;
  s->kind = (id == 0 ? data : ack);
  s->info.data[0] = 0;
  s->info.data[1] = 0;
  s->info.data[2] = 0;
  s->info.data[3] = 0;
}

void queue_frames(void)
{
/* See if there is room in the circular buffer queue[]. If so try to read as 
 * much from the pipe as possible.
 * If inp is near the top of queue[], a single call here
 * may read a few frames into the top of queue[] and then some more starting
 * at queue[0].  This is done in two read operations.
 */

  int prfd, frct, k;
  frame *top;
  struct stat statbuf;

  prfd = (id == 0 ? r2 : r1);	/* which file descriptor is pipe on */

/* How many frames can be read consecutively? */
  top = (outp <= inp ? &queue[MAX_QUEUE] : outp);/* how far can we rd?*/
  k = top - inp;	/* number of frames that can be read consecutively */
 /**/ fprintf(flog,"XQF1 k=%d, nframes=%d\n",k, nframes);
  frct =read(prfd, inp, k * FRAME_SIZE) ;
 /**/ fprintf(flog,"XQF2 k=%d, nframes=%d\n",k, nframes);
  if (frct<0) {
    if (errno != EAGAIN) sim_error("error in reading the pipe 1");}
  if (frct > 0)
  { nframes = nframes + frct/FRAME_SIZE;
 /**/ fprintf(flog,"XQF3 k=%d, nframes=%d\n",k, nframes);
    inp = inp + frct/FRAME_SIZE;
    if (inp == &queue[MAX_QUEUE]) inp = queue;
 /**/ if (nframes>0) print_queue();
    if (frct/FRAME_SIZE==k)     /*are there residual frames to be read? */
    { k = outp - inp;
 /**/ fprintf(flog,"XQF4 k=%d, nframes=%d\n",k, nframes);
      frct = read (prfd, inp, k * FRAME_SIZE);
 /**/ fprintf(flog,"XQF5 k=%d, nframes=%d\n",k, nframes);
      if (frct<0) {
        if (errno != EAGAIN) sim_error("error in reading the pipe 2"); }
      if (frct > 0)
      { nframes = nframes + frct/FRAME_SIZE;
 /**/ fprintf(flog,"XQF6 k=%d, nframes=%d\n",k, nframes);
        inp = inp + frct/FRAME_SIZE;
 /**/ if (nframes>1) print_queue();
        if (frct/FRAME_SIZE==k)
           sim_error("queue full");
      }
    }
   }
 
}


int pick_event(void)
{
/* Pick a random event that is now possible for the process.
 * Note that the order in which the tests is made is critical, as it gives
 * priority to some events over others.  For example, for protocols 3 and 4
 * frames will be delivered before a timeout will be caused.  This is probably
 * a reasonable strategy, and more closely models how a real line works.
 */

  if (check_ack_timer() > 0) return(ack_timeout);
  if (nframes > 0) return((int)frametype());
  if (network_layer_status) return(network_layer_ready);
  if (check_timers() >= 0) return(timeout);	/* timer went off */
  return(NO_EVENT);
}


event_type frametype(void)
{
/* This function is called after it has been decided that a frame_arrival
 * event will occur.  The earliest frame is removed from queue[] and copied
 * to last_frame.  This copying is needed to avoid messing up the simulation
 * in the event that the protocol does not actually read the incoming frame.
 * For example, in protocols 2 and 3, the senders do not call
 * from_physical_layer() to collect the incoming frame. If frametype() did
 * not remove incoming frames from queue[], they never would be removed.
 * Of course, one could change sender2() and sender3() to have them call
 * from_physical_layer(), but doing it this way is more robust.
 *
 * This function determines (stochastically) whether the arrived frame is good
 * or bad (contains a checksum error).
 */

  int n, i;
  event_type event;

  /* Remove one frame from the queue. */
  last_frame = *outp;		/* copy the first frame in the queue */
  outp++;
  if (outp == &queue[MAX_QUEUE]) outp = queue;
  nframes--;

  /* Generate frames with checksum errors at random. */
  n = rand() & 01777;
  if (n < garbled) {
	/* Checksum error.*/
	event = cksum_err;
	if (last_frame.kind == data) cksum_data_recd++;
	if (last_frame.kind == ack) cksum_acks_recd++;
	i = 0;
  } else {
	event = frame_arrival;
	if (last_frame.kind == data) good_data_recd++;
	if (last_frame.kind == ack) good_acks_recd++;
	i = 1;
  }

  if (debug_flags & RECEIVES) {
	printf("Tick %u. Proc %d got %s frame:  ",
						tick/DELTA,id,badgood[i]);
	fr(&last_frame);
  }
  return(event);
}


void from_network_layer(packet *p)
{
/* Fetch a packet from the network layer for transmission on the channel. */

  p->data[0] = (next_net_pkt >> 24) & BYTE;
  p->data[1] = (next_net_pkt >> 16) & BYTE;
  p->data[2] = (next_net_pkt >>  8) & BYTE;
  p->data[3] = (next_net_pkt      ) & BYTE;
  next_net_pkt++;
}


void to_network_layer(packet *p)
{
/* Deliver information from an inbound frame to the network layer. A check is
 * made to see if the packet is in sequence.  If it is not, the simulation
 * is terminated with a "protocol error" message.
 */

  unsigned int num;

  num = pktnum(p);
  if (num != last_pkt_given + 1) {
	printf("Tick %u. Proc %d got protocol error.  Packet delivered out of order.\n", tick/DELTA, id); 
	printf("Expected payload %d but got payload %d\n",last_pkt_given+1,num);
	exit(0);
  }
  last_pkt_given = num;
  payloads_accepted++;
}

  
void from_physical_layer (frame *r)
{
/* Copy the newly-arrived frame to the user. */
 *r = last_frame;
  fprintf(flog,"PFF4 tick %u, from_ph: r->seq=%d, r->ack=%d\n", tick/DELTA, r->seq, r->ack);
  fflush(flog);
  flog_frame(r,'R'); 
}

void to_physical_layer(frame *s)
{
/* Pass the frame to the physical layer for writing on pipe 1 or 2. 
 * However, this is where bad packets are discarded: they never get written.
 */

  int fd, got, k;



  /* The following statement is essential to later on determine the timed
   * out sequence number, e.g. in protocol 6. Keeping track of
   * this information is a bit tricky, since the call to start_timer()
   * does not tell what the sequence number is, just the buffer.  The
   * simulator keeps track of sequence numbers using the array seqs[],
   * which records the sequence number of each data frame sent, so on a
   * timeout, knowing the buffer number makes it possible to determine
   * the sequence number.
   */
  if (s->kind==data) seqs[s->seq % nseqs] = s->seq; /*JH*/ 

  if (s->kind == data) data_sent++;
  if (s->kind == ack) acks_sent++;
  if (retransmitting) data_retransmitted++;
  fprintf(flog,"PTF5 tick %u, to_ph: s->seq=%d, s->ack=%d\n", tick/DELTA, s->seq, s->ack);
  fflush(flog);
  flog_frame(s,'S');
  /* Bad transmissions (checksum errors) are simulated here. */
  k = rand() & 01777;		/* 0 <= k <= about 1000 (really 1023) */
  if (k < pkt_loss) {	/* simulate packet loss */
	if (debug_flags & SENDS) {
		printf("Tick %u. Proc %d sent frame that got lost: ",
							    tick/DELTA, id);
		fr(s);
	}
	if (s->kind == data) data_lost++;	/* statistics gathering */
	if (s->kind == ack) acks_lost++;	/* ditto */
	return;

  }
  if (s->kind == data) data_not_lost++;		/* statistics gathering */
  if (s->kind == ack) acks_not_lost++;		/* ditto */
  fd = (id == 0 ? w1 : w2);

  got = write(fd, s, FRAME_SIZE);
  if (got != FRAME_SIZE) print_statistics();	/* must be done */

  if (debug_flags & SENDS) {
	printf("Tick %u. Proc %d sent frame: ", tick/DELTA, id);
	fr(s);
  }
}


void start_timer(seq_nr k)
{
/* Start a timer for a data frame. */

  ack_timer[k % nseqs] = tick + timeout_interval + offset; /*JH*/
  offset++;
  recalc_timers();		/* figure out which timer is now lowest */
}


void stop_timer(seq_nr k)
{
/* Stop a data frame timer. */

  ack_timer[k % nseqs] = 0; /*JH*/
  recalc_timers();		/* figure out which timer is now lowest */
}


void start_ack_timer(void)
{
/* Start the auxiliary timer for sending separate acks. The length of the
 * auxiliary timer is arbitrarily set to half the main timer.  This could
 * have been another simulation parameter, but that is unlikely to have
 * provided much extra insight.
 */

  aux_timer = tick + timeout_interval/AUX;
  offset++;
}


void stop_ack_timer(void)
{
/* Stop the ack timer. */

  aux_timer = 0;
}


void enable_network_layer(void)
{
/* Allow network_layer_ready events to occur. */

  network_layer_status = 1;
}


void disable_network_layer(void)
{
/* Prevent network_layer_ready events from occuring. */

  network_layer_status = 0;
}


int check_timers(void)
{
/* Check for possible timeout.  If found, reset the timer. */

  int i;

  /* See if a timeout event is even possible now. */
  if (lowest_timer == 0 || tick < lowest_timer) return(-1);

  /* A timeout event is possible.  Find the lowest timer. Note that it is
   * impossible for two frame timers to have the same value, so that when a
   * hit is found, it is the only possibility.  The use of the offset variable
   * guarantees that each successive timer set gets a higher value than the
   * previous one.
   */
  for (i = 0; i < NR_TIMERS; i++) {
	if (ack_timer[i] == lowest_timer) {
		ack_timer[i] = 0;	/* turn the timer off */
		recalc_timers();	/* find new lowest timer */
                oldest_frame = seqs[i];	/* timed out sequence number */
		return(i);
	}
  }
  printf("Impossible.  check_timers failed at %d\n", lowest_timer);
  exit(1);
}


int check_ack_timer()
{
/* See if the ack timer has expired. */

  if (aux_timer > 0 && tick >= aux_timer) {
	aux_timer = 0;
	return(1);
  } else {
	return(0);
  }
}

void flog_frame(frame *f, char sr)
{
   fprintf(flog,"XXXX%6d %c%2d",tick/DELTA,sr,id);
   if (id==0) {
     fprintf(flog,"%4d %4s%4d%4d ",
                 pktnum(&f->info), tag[f->kind], f->seq, f->ack);
     if (sr=='S') fprintf(flog,"-->\n"); 
     else fprintf(flog,"<--\n");
   }
   else {
     fprintf(flog,"%36s"," ");
     if (sr=='S') fprintf(flog,"<--");
        else fprintf(flog,"-->");
     fprintf(flog,"%4d %4s%4d%4d\n",
                 pktnum(&f->info), tag[f->kind], f->seq, f->ack);
   }
}

void flog_string(char *str_out)
{ fprintf(flog,"%s",str_out);
}

void print_queue(void) /*JH*/
{
  int i,k,kk=0;
  frame *top;
  frame prt_frame;
  fprintf(flog,"XPQ0\n"); fflush(flog);
  top=(outp<inp ? inp : &queue[MAX_QUEUE]);
  k = top -outp;
  for (i=0; i<k; i++)
  { kk=outp-queue;
    prt_frame = queue[kk+i];
    fprintf(flog, "XPQ1 pos=%d, seq=%d, ack=%d, info=%d\n",
           kk+i, prt_frame.seq, prt_frame.ack, pktnum(&prt_frame.info));
  }
  fflush(flog);
  kk=0;
  if (inp < outp)
  {  kk = inp-queue;
     for (i=0;i<kk; i++)   
     {  prt_frame = queue[i];
        fprintf(flog, "XPQ2 pos=%d, seq=%d, ack=%d, info=%d\n",
           i, prt_frame.seq, prt_frame.ack, pktnum(&prt_frame.info));
     }
     fflush(flog);
  }
  fprintf(flog,"XPQ3, nframes=%d, frames printed=%d\n", nframes, k+kk);
  fflush(flog);
}

unsigned int pktnum(packet *p)
{
/* Extract packet number from packet. */

  unsigned int num, b0, b1, b2, b3;

  b0 = p->data[0] & BYTE;
  b1 = p->data[1] & BYTE;
  b2 = p->data[2] & BYTE;
  b3 = p->data[3] & BYTE;
  num = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
  return(num);
}


void fr(frame *f)
{
/* Print frame information for tracing. */

  printf("type=%s  seq=%d  ack=%d  payload=%d\n",
	tag[f->kind], f->seq, f->ack, pktnum(&f->info));
}

void recalc_timers(void)
{
/* Find the lowest timer */

  int i;
  bigint t = UINT_MAX;

  for (i = 0; i < NR_TIMERS; i++) {
	if (ack_timer[i] > 0 && ack_timer[i] < t) t = ack_timer[i];
  }
  lowest_timer = t;

  fprintf(flog,"XRC1%6d %3d seqs=", tick, id); /*JH*/
  for (i=0; i < NR_TIMERS; i++) {            /*JH*/
      fprintf(flog,"%2d",seqs[i]);           /*JH*/
  }                                          /*JH*/
  fprintf(flog,"ack_timer=");                /*JH*/
  for (i=0; i < NR_TIMERS; i++) {            /*JH*/
  fprintf(flog,"%4d", ack_timer[i]);         /*JH*/
  }                                          /*JH*/ 
  fprintf(flog,"lowest=%d\n",lowest_timer);  /*JH*/
}


void print_statistics(void)
{
/* Display statistics. */

  int word[3];

  sleep(id+1);  /* let p0 and p1 sleep for different times */ /*jh*/
  printf("\nProcess %d:\n", id);
  printf("\tTotal data frames sent:  %9d\n", data_sent);
  printf("\tData frames lost:        %9d\n", data_lost);
  printf("\tData frames not lost:    %9d\n", data_not_lost);
  printf("\tFrames retransmitted:    %9d\n", data_retransmitted);
  printf("\tGood ack frames rec'd:   %9d\n", good_acks_recd);
  printf("\tBad ack frames rec'd:    %9d\n\n", cksum_acks_recd);

  printf("\tGood data frames rec'd:  %9d\n", good_data_recd);
  printf("\tBad data frames rec'd:   %9d\n", cksum_data_recd);
  printf("\tPayloads accepted:       %9d\n", payloads_accepted);
  printf("\tTotal ack frames sent:   %9d\n", acks_sent);
  printf("\tAck frames lost:         %9d\n", acks_lost);
  printf("\tAck frames not lost:     %9d\n", acks_not_lost);

  printf("\tTimeouts:                %9d\n", timeouts);
  printf("\tAck timeouts:            %9d\n", ack_timeouts);
  fflush(stdin);

  word[0] = 0;
  word[1] = payloads_accepted;
  word[2] = data_sent;
  write(mwfd, word, 3*sizeof(int));	/* tell main we are done printing */
  sleep(1);
  exit(0);
}

void sim_error(char *s)
{
/* A simulator error has occurred. */

  int fd;

  printf("%s\n", s);
  fd = (id == 0 ? w4 : w6);
  write(fd, &zero, TICK_SIZE);
  exit(1);
}

int parse_first_five_parameters(int argc, char *argv[], long *event, int *timeout_interval, int *pkt_loss, int *garbled, int *debug_flags)
{
/* Help function for protocol writers to parse first five command-line
 * parameters that the simulator needs.
 */

  if (argc < 6) {
        printf("Need at least five command-line parameters.\n");
        return(0);
  }
  *event = atol(argv[1]);
  if (*event < 0) {
        printf("Number of simulation events must be positive\n");
        return(0);
  }
  *timeout_interval = atoi(argv[2]);
  if (*timeout_interval < 0){
        printf("Timeout interval must be positive\n");
        return(0);
  }
  *pkt_loss = atoi(argv[3]);     /* percent of sends that chuck pkt out */
  if (*pkt_loss < 0 || *pkt_loss > 99) {
        printf("Packet loss rate must be between 0 and 99\n");
        return(0);
  }
  *garbled = atoi(argv[4]);
  if (*garbled < 0 || *garbled > 99) {
        printf("Packet cksum rate must be between 0 and 99\n");
        return(0);
  }
  *debug_flags = atoi(argv[5]);
  if (*debug_flags < 0) {
        printf("Debug flags may not be negative\n");
        return(0);
  }
  return(1);
}

