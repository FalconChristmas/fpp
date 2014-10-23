VERSION=3.0.1

DEPENDANT_PACKAGES = \
	libgtk2.0-dev \
	libgstreamer-plugins-base0.10-dev \
	$(NULL)

.PHONY: all $(DEPENDANT_PACKAGES)
all: $(DEPENDANT_PACKAGES) wxWidgets-$(VERSION)/Makefile
	make -C wxWidgets-$(VERSION)

$(DEPENDANT_PACKAGES):
	@if ! dpkg --get-selections | grep -qc $@; \
	then \
		echo >&2; \
		echo "You must install $@ to build wxWidgets" >&2; \
		echo "try running:" >&2; \
		echo "  sudo apt-get install $(DEPENDANT_PACKAGES)" >&2; \
		echo >&2; \
		exit 1; \
	fi

wxWidgets-$(VERSION).tar.bz2:
	wget --continue -o/dev/null -O$@ https://sourceforge.net/projects/wxwindows/files/$(VERSION)/wxWidgets-$(VERSION).tar.bz2

wxWidgets-$(VERSION): wxWidgets-$(VERSION).tar.bz2
	mkdir -p $@
	tar --strip-components=1 -xjpf $< -C $@

wxWidgets-$(VERSION)/Makefile: wxWidgets-$(VERSION)
	cd wxWidgets-$(VERSION) && CXXFLAGS=-std=gnu++0x ./configure --enable-mediactrl --enable-graphics_ctx

clean:
	make -C wxWidgets-$(VERSION) clean

dist-clean:
	rm -rf wxWidgets-$(VERSION).tar.bz2 wxWidgets-$(VERSION)
