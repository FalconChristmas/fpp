

OBJECTS_fpp_so += \
	channeloutput/ChannelOutputBase.o \
	channeloutput/ThreadedChannelOutputBase.o \
	channeloutput/channeloutput.o \
	channeloutput/channeloutputthread.o \
	channeloutput/ArtNet.o \
	channeloutput/ColorOrder.o \
	channeloutput/DebugOutput.o \
	channeloutput/DDP.o \
	channeloutput/E131.o \
	channeloutput/FBMatrix.o \
	channeloutput/FBVirtualDisplay.o \
	channeloutput/FPD.o \
	channeloutput/GenericSerial.o \
	channeloutput/GPIO.o \
	channeloutput/GPIO595.o \
	channeloutput/HTTPVirtualDisplay.o \
	channeloutput/UDPOutput.o \
	channeloutput/LOR.o \
	channeloutput/Matrix.o \
	channeloutput/MAX7219Matrix.o \
	channeloutput/MCP23017.o \
	channeloutput/PanelMatrix.o \
	channeloutput/PixelString.o \
	channeloutput/RHL_DVI_E131.o \
	channeloutput/serialutil.o \
	channeloutput/SPInRF24L01.o \
	channeloutput/SPIws2801.o \
	channeloutput/Triks-C.o \
	channeloutput/USBDMX.o \
	channeloutput/USBPixelnet.o \
	channeloutput/USBRelay.o \
	channeloutput/USBRenard.o \
	channeloutput/VirtualDisplay.o \
    channeloutput/processors/OutputProcessor.o \
    channeloutput/processors/RemapOutputProcessor.o \
    channeloutput/processors/SetValueOutputProcessor.o \
    channeloutput/processors/BrightnessOutputProcessor.o \
    channeloutput/processors/ColorOrderOutputProcessor.o \
	channeltester/ChannelTester.o \
	channeltester/TestPatternBase.o \
	channeltester/RGBChase.o \
	channeltester/RGBCycle.o \
	channeltester/RGBFill.o \
	channeltester/SingleChase.o \
	command.o \
	common.o \
	e131bridge.o \
	effects.o \
	events.o \
	falcon.o \
	fppversion.o \
	FrameBuffer.o \
	fseq/FSEQFile.o \
	gpio.o \
	httpAPI.o \
	log.o \
	MultiSync.o \
	mediadetails.o \
	mediaoutput/MediaOutputBase.o \
	mediaoutput/mediaoutput.o \
	mediaoutput/mpg123.o \
	mediaoutput/ogg123.o \
	mediaoutput/omxplayer.o \
	mediaoutput/SDLOut.o \
	mqtt.o \
	ping.o \
	PixelOverlay.o \
	Playlist.o \
	playlist/Playlist.o \
	playlist/PlaylistEntryBase.o \
	playlist/PlaylistEntryBoth.o \
	playlist/PlaylistEntryBranch.o \
	playlist/PlaylistEntryBrightness.o \
	playlist/PlaylistEntryChannelTest.o \
	playlist/PlaylistEntryDynamic.o \
	playlist/PlaylistEntryEffect.o \
	playlist/PlaylistEntryEvent.o \
	playlist/PlaylistEntryImage.o \
	playlist/PlaylistEntryMedia.o \
	playlist/PlaylistEntryMQTT.o \
	playlist/PlaylistEntryPause.o \
	playlist/PlaylistEntryPixelOverlay.o \
	playlist/PlaylistEntryPlaylist.o \
	playlist/PlaylistEntryPlugin.o \
	playlist/PlaylistEntryRemap.o \
	playlist/PlaylistEntryScript.o \
	playlist/PlaylistEntrySequence.o \
	playlist/PlaylistEntryURL.o \
	playlist/PlaylistEntryVolume.o \
	Plugins.o \
	Scheduler.o \
	ScheduleEntry.o \
	scripts.o \
	sensors/Sensors.o \
	Sequence.o \
	settings.o \
	SunSet.o \
    $(OBJECTS_GPIO_ADDITIONS)


LIBS_fpp_so += \
	-lboost_filesystem \
	-lboost_system \
	-lboost_date_time \
	-lpthread \
    -lzstd -lz \
	-lhttpserver \
	-ljsoncpp \
	-lm \
	-lmosquitto \
	-lutil \
	-ltag \
	-lSDL2 \
	-lavformat \
	-lavcodec \
	-lavutil \
	-lswresample \
	-lswscale \
    $(LIBS_GPIO_ADDITIONS)


OLAFLAGS := $(shell pkg-config --cflags --libs libola 2> /dev/null)
ifneq '$(OLAFLAGS)' ''
	CFLAGS += -DUSEOLA
	LDFLAGS_fpp_so += $(OLAFLAGS)
	OBJECTS_fpp_so += channeloutput/OLAOutput.o
endif

TARGETS += libfpp.so
OBJECTS_ALL+=$(OBJECTS_fpp_so)

libfpp.so: $(OBJECTS_fpp_so) $(DEPS_fpp_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_so) $(LIBS_fpp_so) $(LIBS_fpp_so) $(LDFLAGS) $(LDFLAGS_fpp_so) -o $@
