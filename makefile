

QSRCS = qregd cmd parse qna_usb util qregs
TSRCS = tst util qregs corr

default: qregd tst


qregd: $(QSRCS:%=obj/%.o)
	gcc $(QSRCS:%=obj/%.o) -lm -liio -o $@ 
tst: $(TSRCS:%=obj/%.o)
	gcc $(TSRCS:%=obj/%.o) -lm -liio -o $@ 

obj/%.o: src/%.c
	gcc $< -c -o $@

clean:
	rm -rf obj

obj:
	mkdir obj
