

QSRCS = h qregd cmd parse util qregs ini mx qna rp
QOBJS = $(QSRCS:%=obj/%.o)

TSRCS = h util corr qregs ini mx parse qna rp qregc
TOBJS = obj/tst.o $(TSRCS:%=obj/%.o)
THDRS = $(TSRCS:%=src/%.h)


TSSRCS = h util corr qregs ini mx parse qna rp tsd cmd hdl
TSOBJS = obj/ts.o $(TSSRCS:%=obj/%.o)


USRCS = h util corr mx ini parse h_vhdl_extract cmd qregs qregc qna rp i2c
UOBJS = $(USRCS:%=obj/%.o)

VARSRCS = ini mx parse
VAROBJS = $(VARSRCS:%=obj/%.o)


all: obj tst u qregd ts


#libvars.a: $(VAROBJS)
#	ar r $@ $(VAROBJS)


qregd: $(QOBJS)
	gcc $(QOBJS) -lm -liio -pthread -o $@

libqregs.a: obj/qregs.o
	echo $(QOBJS)
	ar r $@ obj/qregs.o

obj/qregs.o: src/h_vhdl_extract.h

obj/tst.o: src/tst.c ../qnicll/qnicll.h
	gcc -D'OPT_QNICLL=1' -I../qnicll $< -c -o $@

tst: $(TOBJS)
	gcc $(TOBJS) -L../qnicll -lm -liio  -o $@ 

ts: $(TSOBJS)
	gcc $(TSOBJS) -L../qnicll -lm -liio -pthread -o $@ 


# utilities
obj/u.o: src/qregs.h
u:  obj/u.o $(UOBJS)
	gcc obj/u.o $(UOBJS) -L. -lm -liio  -o $@ 


$(TOBJS): $(THDRS)

obj/%.o: src/%.c
	gcc $< -c -o $@

clean:
	rm -rf obj/*

obj:
	mkdir obj

tar:
	rm -f src/*~
	tar -cf src.tar src

diff:
	tar --diff -f src.tar src

send:
	scp src.tar root@zcu1:/home/analog/board_code
