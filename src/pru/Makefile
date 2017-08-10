CPP := cpp
PASM_DIR ?= ../../external/LEDscape/am335x/pasm
PASM := $(PASM_DIR)/pasm

# Add new .bin files here to compile/install their corresponding .p
# files automatically
BINARIES :=  \
	FalconMatrix_1.bin FalconMatrix_2.bin FalconMatrix_3.bin FalconMatrix_4.bin \
	FalconMatrix_5.bin FalconMatrix_6.bin FalconMatrix_7.bin FalconMatrix_8.bin \
	FalconMatrix_v2_1.bin FalconMatrix_v2_2.bin FalconMatrix_v2_3.bin FalconMatrix_v2_4.bin \
	FalconMatrix_v2_5.bin FalconMatrix_v2_6.bin FalconMatrix_v2_7.bin FalconMatrix_v2_8.bin \
	FalconDMX.bin FalconDMX_4a.bin FalconDMX_4b.bin \
	FalconPixelnet.bin FalconPixelnet_4a.bin FalconPixelnet_4b.bin\
	FalconWS281x_48.bin FalconWS281x_40.bin FalconWS281x_16.bin FalconWS281x_F4.bin \
	FalconWS281x_F8_12.bin FalconWS281x_F8_16.bin FalconWS281x_F8_20.bin FalconWS281x_F8_EXP.bin \
	FalconWS281x_RGBCape48C.bin FalconWS281x_RGBCape48F.bin


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

FalconWS281x_F8_EXP.bin: FalconWS281x.p F8B.hp $(PASM)
	$(CPP) -DF8B -DF8B_EXP=1 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
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

FalconWS281x_RGBCape48C.bin: FalconWS281x.p RGBCape48C.hp $(PASM)
	$(CPP) -DOUTPUTS=48 -DRGBCape48C=1 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconWS281x_RGBCape48F.bin: FalconWS281x.p RGBCape48F.hp $(PASM)
	$(CPP) -DOUTPUTS=48 -DRGBCape48F=1 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconDMX.bin: FalconSerial.p FalconSerial.hp $(PASM)
	$(CPP) -DDMX - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconDMX_4a.bin: FalconSerial.p FalconSerial.hp $(PASM)
	$(CPP) -DONLYA -DDMX - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconDMX_4b.bin: FalconSerial.p FalconSerial.hp $(PASM)
	$(CPP) -DONLYB -DDMX - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconPixelnet.bin: FalconSerial.p FalconSerial.hp $(PASM)
	$(CPP) -DPIXELNET - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconPixelnet_4a.bin: FalconSerial.p FalconSerial.hp $(PASM)
	$(CPP) -DONLYA -DPIXELNET - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i

FalconPixelnet_4b.bin: FalconSerial.p FalconSerial.hp $(PASM)
	$(CPP) -DONLYB -DPIXELNET - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i
	$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $@)
	@$(RM) /tmp/$(basename $<).i


FalconMatrix_1.bin FalconMatrix_2.bin FalconMatrix_3.bin FalconMatrix_4.bin FalconMatrix_5.bin FalconMatrix_6.bin FalconMatrix_7.bin FalconMatrix_8.bin: FalconMatrix.p  FalconUtils.hp OctoscrollerV1.hp $(PASM)
	for i in `seq 1 8`; \
	do  \
        $(CPP) -DOUTPUTS=$$i - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i ; \
        $(PASM) -V3 -b /tmp/$(basename $<).i $(basename $<)_$$i ; \
        $(RM) /tmp/$(basename $<).i ; \
	done

FalconMatrix_v2_1.bin FalconMatrix_v2_2.bin FalconMatrix_v2_3.bin FalconMatrix_v2_4.bin FalconMatrix_v2_5.bin FalconMatrix_v2_6.bin FalconMatrix_v2_7.bin FalconMatrix_v2_8.bin: FalconMatrix.p  FalconUtils.hp OctoscrollerV2.hp $(PASM)
	for i in `seq 1 8`; \
    do  \
		$(CPP) -DOUTPUTS=$$i -DOCTO_V2 - < $< | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/$(basename $<).i ; \
		$(PASM) -V3 -b /tmp/$(basename $<).i $(basename $<)_v2_$$i ; \
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
