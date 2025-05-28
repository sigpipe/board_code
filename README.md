# Board Code

This code runs on the board (typically the zcu106).

## qregd

This is the qreg demon (which listens on port 5000).  For now, you have to manually start it.  This must be running on the zcu106 in order for the the qnicll library to work.


## tst

This transmits headers out the DAC, and captures the signal on the ADC, and stores it in the out directory.  This (or something like it) was the main workhorse during the happycamper event.  There are a few command line options, and to see them run "ech ?".  See also the qnicll repository.

## source files

src/tst.c   - source code for tst

src/qregs.c - register access routines

src/ini.c   - manages variables in a separate namespace,
              can read & write them from/to a text file.
	      similar in concept to JSON or numerous other serialized formats.
	      This uses matlab syntax.
