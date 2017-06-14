CPP := cpp
PASM_DIR ?= ../../external/LEDscape/am335x/pasm
PASM := $(PASM_DIR)/pasm

# Add new .bin files here to compile/install their corresponding .p
# files automatically
BINARIES := FalconMatrix.bin FalconMatrix_32x32.bin  \
	FalconMatrix_1.bin FalconMatrix_2.bin FalconMatrix_3.bin FalconMatrix_4.bin \
	FalconMatrix_5.bin FalconMatrix_6.bin FalconMatrix_7.bin FalconMatrix_8.bin \
	FalconMatrix_32x32_1.bin FalconMatrix_32x32_2.bin FalconMatrix_32x32_3.bin FalconMatrix_32x32_4.bin \
	FalconMatrix_32x32_5.bin FalconMatrix_32x32_6.bin FalconMatrix_32x32_7.bin FalconMatrix_32x32_8.bin \
	RGBCape48C.bin FalconDMX.bin FalconDMX_4a.bin FalconDMX_4b.bin \
	FalconPixelnet.bin FalconPixelnet_4a.bin FalconPixelnet_4b.bin\
	FalconWS281x_48.bin FalconWS281x_40.bin FalconWS281x_16.bin FalconWS281x_F4.bin \
	FalconWS281x_F8_12.bin FalconWS281x_F8_16.bin FalconWS281x_F8_20.bin


#
all: $(BINARIES)

FalconWS281x_F4.bin: FalconWS281x.p F4B.hp $(PASM)
	$(CPP) -DF4B - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconWS281x_F8_12.bin: FalconWS281x.p F8B.hp $(PASM)
	$(CPP) -DF8B -DOUTPUTS=12 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconWS281x_F8_16.bin: FalconWS281x.p F8B.hp $(PASM)
	$(CPP) -DF8B -DOUTPUTS=16 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconWS281x_F8_20.bin: FalconWS281x.p F8B.hp $(PASM)
	$(CPP) -DF8B -DOUTPUTS=20 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconWS281x_16.bin: FalconWS281x.p F16B.hp $(PASM)
	$(CPP) -DOUTPUTS=16 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconWS281x_40.bin: FalconWS281x.p F16B.hp $(PASM)
	$(CPP) -DOUTPUTS=40 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconWS281x_48.bin: FalconWS281x.p F16B.hp $(PASM)
	$(CPP) -DOUTPUTS=48 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconDMX_4a.bin: FalconDMX.p FalconSerial.hp $(PASM)
	$(CPP) -DONLYA - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconDMX_4b.bin: FalconDMX.p FalconSerial.hp $(PASM)
	$(CPP) -DONLYB - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconPixelnet_4a.bin: FalconPixelnet.p FalconSerial.hp $(PASM)
	$(CPP) -DONLYA - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconPixelnet_4b.bin: FalconPixelnet.p FalconSerial.hp $(PASM)
	$(CPP) -DONLYB - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconMatrix_32x32.bin: FalconMatrix.p $(PASM)
	$(CPP) -DMATRIX_HEIGHT=16 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i


FalconMatrix_1.bin FalconMatrix_2.bin FalconMatrix_3.bin FalconMatrix_4.bin FalconMatrix_5.bin FalconMatrix_6.bin FalconMatrix_7.bin FalconMatrix_8.bin: FalconMatrix.p $(PASM)
	for i in `seq 1 8`; \
	do  \
        $(CPP) -DOUTPUTS=$$i - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i ; \
        $(PASM) -V3 -b /tmp/$(basename $<).i $(basename $<)_$$i ; \
        $(RM) /tmp/$(basename $<).i ; \
	done

FalconMatrix_32x32_1.bin FalconMatrix_32x32_2.bin FalconMatrix_32x32_3.bin FalconMatrix_32x32_4.bin FalconMatrix_32x32_5.bin FalconMatrix_32x32_6.bin FalconMatrix_32x32_7.bin FalconMatrix_32x32_8.bin: FalconMatrix.p $(PASM)
	for i in `seq 1 8`; \
        do  \
        $(CPP) -DOUTPUTS=$$i -DMATRIX_HEIGHT=16 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i ; \
        $(PASM) -V3 -b /tmp/$(basename $<).i $(basename $<)_32x32_$$i ; \
        $(RM) /tmp/$(basename $<).i ; \
        done

%.bin: %.p $(PASM)
	$(CPP) - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

release: $(BINARIES)
	cp -rp $(BINARIES) ../../lib/

clean:
	rm -f $(BINARIES)
