# Board Code

This code runs on the board (typically the zcu106).

## qregd

This is the qreg demon (which listens on port 5000).  For now, you have to manually start it.  This must be running on the zcu106 in order for the the qnicll library to work.


## ech

This transmits headers out the DAC, and captures the signal on the ADC, and stores it in the out directory.  This (or something like it) was the main workhorse during the happycamper event.  There are a few command line options, and to see them run "ech ?".  See also the qnicll repository.


