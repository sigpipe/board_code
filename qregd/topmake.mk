all: qregd tst

%: build/%
	cp $< $@

