

QSRCS = h qregd cmd parse util qregs ini mx qna rp
QOBJS = $(QSRCS:%=obj/%.o)

TSRCS = h util corr qregs ini mx parse qna rp
TOBJS = obj/tst.o $(TSRCS:%=obj/%.o)
THDRS = $(TSRCS:%=src/%.h)

USRCS = h util corr mx ini parse h_vhdl_extract cmd qregs qna rp i2c
UOBJS = $(USRCS:%=obj/%.o)

VARSRCS = ini mx parse
VAROBJS = $(VARSRCS:%=obj/%.o)


all: obj tst u qregd


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
	gcc $(TOBJS) -L../qnicll -lm -liio  -lqnicll -o $@ 

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
