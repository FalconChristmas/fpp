CPP := cpp
PASM_DIR ?= ../../external/LEDscape/am335x/pasm
PASM := $(PASM_DIR)/pasm

# Add new .bin files here to compile/install their corresponding .p
# files automatically
BINARIES := FalconMatrix.bin FalconMatrix_32x32.bin RGBCape48C.bin FalconDMX.bin FalconPixelnet.bin \
    FalconWS281x_48.bin FalconWS281x_40.bin FalconWS281x_16.bin FalconWS281x_4.bin

#
all: $(BINARIES)

FalconWS281x_4.bin: FalconWS281x.p $(PASM)
	$(CPP) -DOUTPUTS=4 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconWS281x_16.bin: FalconWS281x.p $(PASM)
	$(CPP) -DOUTPUTS=16 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconWS281x_40.bin: FalconWS281x.p $(PASM)
	$(CPP) -DOUTPUTS=40 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconWS281x_48.bin: FalconWS281x.p $(PASM)
	$(CPP) -DOUTPUTS=48 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i


%.bin: %.p $(PASM)
	$(CPP) - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

release: $(BINARIES)
	cp -rp $(BINARIES) ../../lib/

clean:
	rm -f $(BINARIES)
