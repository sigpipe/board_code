# Board Code

This code runs on the board (typically the zcu106).

## qregd

This is the qreg demon.  For now, you have to manually start it.  You can connect to qregd using the qnicll library.  See also the qnicll repository.


## ech

This transmits headers out the DAC, and captures the signal on the ADC, and stores it in the out directory.  This (or something like it) was the main workhorse during the happycamper event.


