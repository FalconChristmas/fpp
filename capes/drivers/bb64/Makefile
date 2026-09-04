
# Device-tree headers shipped by the base image under /opt/source. The tree set
# is a rolling window: the vendor image drops older kernel series as it adds
# newer ones, so this must track the series the base image actually ships (and
# the running kernel), not an older one that merely still happened to be there.
DTD_BASE := /opt/source/dtb-6.18.x

# Guard: the vendor image periodically drops the series DTD_BASE points at.
# Without this the build emits a pile of "no include path in which to search
# for dt-bindings/..." errors that say nothing about the real cause.
DTD_CHECK = @test -d $(DTD_BASE) || { \
	    echo "ERROR: $(DTD_BASE) does not exist."; \
	    echo "       The base image no longer ships this device-tree series."; \
	    echo "       Series currently available under /opt/source:"; \
	    ( ls -d /opt/source/dtb-*.x 2>/dev/null || echo "(none)" ) | sed 's|^|         |'; \
	    echo "       Update DTD_BASE in $(firstword $(MAKEFILE_LIST)) to a shipped series."; \
	    exit 1; }

all: fpp-base-overlay.dtb fpp-cape-overlay.dtb

fpp-base-overlay.dtb: fpp-base-overlay.dts
	$(DTD_CHECK)
	cpp -I$(DTD_BASE)/include/ -I$(DTD_BASE)/src/arm64/ti/ -nostdinc -undef -x assembler-with-cpp fpp-base-overlay.dts > .fpp-base-overlay.dts
	dtc --include $(DTD_BASE)/include/ --include $(DTD_BASE)/src/arm64/ti/ -O dtb -o fpp-base-overlay.dtb .fpp-base-overlay.dts	
	rm .fpp-base-overlay.dts	

fpp-cape-overlay.dtb: fpp-cape-overlay.dts
	dtc -O dtb -o fpp-cape-overlay.dtb fpp-cape-overlay.dts

install_cape_overlay:
	dtc -O dtb -o /boot/firmware/overlays/fpp-cape-overlay.dtb /opt/fpp/capes/drivers/bb64/fpp-cape-overlay.dts

install: fpp-base-overlay.dtb fpp-cape-overlay.dtb	
	cp fpp-base-overlay.dtb /boot/firmware/overlays
	cp fpp-cape-overlay.dtb /boot/firmware/overlays
	cp fpp-cape-overlay.dtb /boot/firmware/overlays/fpp-cape-overlay-default.dtb
	cp extlinux/extlinux.conf /boot/firmware/extlinux/extlinux.conf

clean:
	rm -f *.dtb
