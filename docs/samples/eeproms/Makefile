#############################################################################
# Instructions for using these samples:
#
# Copy the sample directory to the fpp user's home directory:
#
#     mkdir /home/fpp/eeproms
#     rsync -av /opt/fpp/docs/samples/eeproms/ /home/fpp/eeproms/
#
# Create a new directory for your cape's EEPROM and populate with a copy of
# one of the Pi or Beagle sample EEPROM directories:
#
#     cd /home/fpp/eeproms
#     mkdir MyCape-16
#     rsync -av sample-pi-strings/ MyCape-16/
#
# Edit the Variables below to fill in information for your new cape EEPROM.
#
# Run the 'make' command to generate the EEPROM .bin file:
#
#     make
#
# NOTE: If you have multiple capes or different versions of the same cape,
# you may edit the 'CAPES' variable below to define multiple cape names
# and also uncomment the two lines at the bottom of this file and copy/modify
# them to define different options for each such as different version numbers
# or different location values.
#
#############################################################################
# Variables to set:
#
# Name of your cape and the directory containing its config contents
CAPE_NAME = sample-pi-strings
#
# Cape Version
CAPE_VERS = 1.0
#
# Cape Serial Number (does not have to be unique)
CAPE_SERIAL = $(shell date +%Y%m%d%H%M%S)
#
# EEPROM location ('any', 'eeprom' for physical EEPROM, or 'file' for virtual)
CAPE_LOCATION = any
#
#############################################################################
GENEEPROM = /opt/fpp/scripts/generateEEPROM

CAPES = $(CAPE_NAME)

# Uncomment this line to build all sample .bin files
#CAPES = sample-bbb-strings sample-pi-strings

OBJECTS = $(addsuffix -eeprom.bin,$(CAPES))

all: $(OBJECTS)

info: $(OBJECTS)
	@for FILE in $(OBJECTS) ; do \
		echo "================================================" ; \
		$(GENEEPROM) -e $${FILE} -i ; \
	done

# Example options given:
# -n sample-bbb-strings                 - Cape name
# -v 1.0                                - Version 1.0 of cape
# -d sample-bbb-strings                 - Package up the 'sample-bbb-strings' directory
# -l any                                - Allow EEPROM contents on both physical and virtual EEPROMs
# -s 20220707201643                     - Set the serial number (to current timestamp)
# -o sample-bbb-strings-eeprom.bin      - Name of .bin file to generate.  If the filename ends in
#                                         'eeprom.bin', FPP will automatically copy it to
#                                         /home/fpp/media/config/cape-eeprom.bin if the file is uploaded
#                                         to FPP via the FPP File Manager.
%-eeprom.bin: %/* %/*/* %/*/*/*
	$(GENEEPROM) -n $* -v $(CAPE_VERS) -s $(CAPE_SERIAL) -l $(CAPE_LOCATION) -d $* -o $@

clean:
	rm -rf *.bin || true

# Uncomment and modify these 2 lines if you edit the "CAPES" variable above to allow building multiple cape
# EEPROMs from a single Makefile.  This allows each EEPROM to have its own version, serial, and location.
#
#sample-pi-strings-eeprom.bin: sample-pi-strings/* sample-pi-strings/*/* sample-pi-strings/*/*/*
#	$(GENEEPROM) -n sample-pi-strings -v 2.0 -s $(CAPE_SERIAL) -l $(CAPE_LOCATION) -d sample-pi-strings -o $@

