

QSRCS = qregd cmd parse qna_usb util qregs

TSRCS = util qregs corr mx ini parse h_vhdl_extract


default: qregd tst


qregd: $(QSRCS:%=obj/%.o)
	gcc -L. $(QSRCS:%=obj/%.o) -lm -liio  -o $@ 
tst: obj/tst.o $(TSRCS:%=obj/%.o) $(TSRCS:%=src/%.h)
	gcc -L. obj/tst.o $(TSRCS:%=obj/%.o) -lm -liio  -o $@ 

obj/%.o: src/%.c
	gcc $< -c -o $@

clean:
	rm -rf obj

obj:
	mkdir obj
