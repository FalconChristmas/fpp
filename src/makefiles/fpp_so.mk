

OBJECTS_fpp_so += \
	channeloutput/ChannelOutputBase.o \
	channeloutput/ThreadedChannelOutputBase.o \
	channeloutput/channeloutput.o \
	channeloutput/channeloutputthread.o \
	channeloutput/ColorOrder.o \
	channeloutput/FPD.o \
	channeloutput/LOR.o \
	channeloutput/Matrix.o \
	channeloutput/PanelMatrix.o \
	channeloutput/PixelString.o \
	channeloutput/serialutil.o \
	channeloutput/SPInRF24L01.o \
	channeloutput/Triks-C.o \
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
	-lpthread -lrt \
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


TARGETS += libfpp.so
OBJECTS_ALL+=$(OBJECTS_fpp_so)

libfpp.so: $(OBJECTS_fpp_so) $(DEPS_fpp_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_so) $(LIBS_fpp_so) $(LIBS_fpp_so) $(LDFLAGS) $(LDFLAGS_fpp_so) -o $@
