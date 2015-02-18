CPP := cpp
PASM_DIR ?= ../../external/LEDscape/am335x/pasm
PASM := $(PASM_DIR)/pasm

# Add new .bin files here to compile/install their corresponding .p
# files automatically
BINARIES := FalconMatrix.bin FalconWS281x.bin

#
all: $(BINARIES)

%.bin: %.p $(PASM)
	$(CPP) - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

release: $(BINARIES)
	cp -rp $(BINARIES) ../../lib/

clean:
	rm -f $(BINARIES)
