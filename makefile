

QSRCS = qregd cmd parse qna_usb util qregs
QOBJS = $(QSRCS:%=obj/%.o)

TSRCS = util qregs corr mx ini parse h_vhdl_extract
TOBJS = obj/tst.o $(TSRCS:%=obj/%.o)
THDRS = $(TSRCS:%=src/%.h)

USRCS = util qregs corr mx ini parse h_vhdl_extract cmd
UOBJS = $(USRCS:%=obj/%.o)

default: tst u


qregd: obj/qregd.o $(QOBJS)
	gcc obj/qregd.o $(QOBJS) -lm -liio  -o $@

tst:  $(TOBJS)
	gcc $(TOBJS) -lm -liio  -o $@ 

# utilities
u:  obj/u.o $(UOBJS)
	gcc obj/u.o $(UOBJS) -lm -liio  -o $@ 


$(TOBJS): $(THDRS)

obj/%.o: src/%.c
	gcc $< -c -o $@

clean:
	rm -rf obj

obj:
	mkdir obj
