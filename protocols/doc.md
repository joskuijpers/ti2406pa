Here are some notes on the simulator.

The simulator code is in simulator.c and the header file is simulator.h.
The simulator uses three process:

	main:	controls the simulation (this is the user process that calls
                start_simulator() function).
	M0:	machine 0 (simulates one end: sender for protocols 2 and 3)
	M1:	machine 1 (simulates the other end: receiver for protocols 2
                           and 3)

The function start_simulator() starts the simulation. It first initializes
all the simulation parameters, and then creates six pipes so the three
processes can communicate pairwise.  The file descriptors created are named as
follows.

    M0 - M1 communication:
	w1, r1: M0 to M1 for frames
	w2, r2: M1 to M0 for frames

    Main - M0 communication:
	w3, r3: main to M0 for go-ahead
	w4, r4: M0 to main to signal readiness

    Main - M1 communication:
	w5, r5: main to M1 for go-ahead
	w6, r6: M1 to main to signal readiness

After the pipes have been created, the main process forks off two children,
M0 and M1.  Each of these then calls the appropriate protocol as a subroutine.
The name of this subroutine is passed as a parameter in start_simulator()
function invocation.

Each protocol runs and does its own initialization.  Eventually it calls
wait_for_event() to get work.  A brief description of this routine and all
the others that a protocol designer may use are given in protocol.h.
Wait_for_event() sets some counters, the reads any pending frames from the
other worker, M0 or M1.  This is done to get them out of the pipe, to prevents
the pipes from clogging.  The frames read are stored in the array queue[],
and removed from there as needed.  The pointers inp and outp point to the
first empty slot in queue[] and the next frame to remove, respectively.
Nframes keeps track of the number of queued frames.

Once the input pipe is sucked dry, wait_for_event() sends a 4-byte
message to main to tell main that it is prepared to process an event.
At that point it waits for main to give it the go-ahead.

Main picks a worker to run and sends it the current time on file descriptors
w3 or w5.  This is the go-ahead signal.  The worker sets its own time to the
value read from the pipe, so the two workers remain synchronized in time.
Then it calls pick_event() to determine which event to return.  The list of
potential events differs for each protocol simulated.

Once the event has been returned, wait_for_event returns to the caller, one
of the protocol routines, which then executes.  These routines can call the
library routines that are defined in the file protocol.h. They manage timers,
write frames to the pipe, etc.  The code is straightforward and full of
comments.

The rest of start_simulation function is simple.  It picks a process and
gives it the go-ahead by writing the time to its communication pipe as a
4-byte integer.  That process then checks to see if it is able to run.  If
it is, it returns the code OK. If it cannot run now and no timers are pending,
it returns the code NOTHING. If both processes return NOTHING for DEADLOCK
ticks in a row, a deadlock is declared.  DEADLOCK is set to 3 times the
timeout interval, which is probably overly conservative, but probably
eliminates false deadlock announcements.

