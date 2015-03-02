This package contains a simulator for simulating datalink protocols, and
codes for simulating the protocols of Chapter 3 of "Computer Networks 4/e"
by Andrew S. Tanenbaum, published by Prentice Hall PTR, 2002. It was written
by Andrew S. Tanenbaum and revised by Shivakant Mishra. It may be freely
distributed.

This package may be used in two ways: (1) using X window-based graphical
user interface, and (2) using the Unix/Linux command-line interface.

To use the X window-based graphical user interface, you need to have X
window system and Tcl/Tk toolkit installed on your system. Simply run the
script file gui.tcl to start the graphical user interface. You may have
to modify the first line of this file to reflect the correct path for
wish in your system. You can compile and run the simulator and the datalink
protocols using this interface.

To use the Unix/Linux command-line interface, a makefile is provided that
compiles the simulator and all five protocols.  To use this, just type
'make'.  If you want to use gcc instead of cc,
change the line

CC=cc

to

CC=gcc

in Makefile.

If you don't have make installed on your system, you can compile the
simulator and the protocols individually. The simulator code is in file
simulator.c with header file simulator.h, and files p2.c, p3.c, p4.c, p5.c,
and p6.c provide the protocol codes for the datalink protocols described in
chapter 3. Compile the simulator as follows:

	gcc -c simulator.c

This will create an object file called simulator.o that needs to be linked
with the protocol being simulated. Compile the protocols as follows:

        gcc -o protocol-name protocol-filename.c simulator.o

For example, to compile p6.c, use the following command:

	gcc -o protocol6 p6.c simulator.o

This protocol can then be executed as follows:

	protocol6 events  timeout  pct_loss  pct_cksum  debug_flags

where

        events tells how long to run the simulation
        timeout gives the timeout interval in ticks
        pct_loss gives the percentage of frames that are lost (0-99)
        pct_cksum gives the percentage of arriving frames that are bad (0-99)
        debug_flags enables various tracing flags:
                1        frames sent
                2        frames received
                4        timeouts
                8        periodic printout for use with long runs

For example

	protocol6 100000 40 20 10 3

will run protocol 6 for 100,000 events with a timeout interval of 40 ticks,
a 20% packet loss rate, a 10% rate of checksum errors (of the 80% that get
through), and will print a line for each frame sent or received.  Because
each peer process is represented by a different UNIX process, there is
(quasi)parallel processing going on.  This means that successive runs will
not give the same results due to timing fluctuations.

Protocol designers are advised to read file protocol.h. This file contains
the definitions of the data structures that the simulator uses, and a
description of the function prototypes that the simulator provides.

A set of possible student exercises is given in the file exercises.
