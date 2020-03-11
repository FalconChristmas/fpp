

OBJECTS_fpp_so += \
	channeloutput/ChannelOutputBase.o \
	channeloutput/ThreadedChannelOutputBase.o \
	channeloutput/channeloutput.o \
	channeloutput/channeloutputthread.o \
	channeloutput/ColorOrder.o \
	channeloutput/FPD.o \
	channeloutput/Matrix.o \
	channeloutput/PanelMatrix.o \
	channeloutput/PixelString.o \
	channeloutput/serialutil.o \
	channeloutput/VirtualDisplay.o \
    channeloutput/processors/OutputProcessor.o \
    channeloutput/processors/RemapOutputProcessor.o \
    channeloutput/processors/HoldValueOutputProcessor.o \
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
	commands/Commands.o \
	commands/PlaylistCommands.o \
    commands/EventCommands.o \
    commands/MediaCommands.o \
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
	FPPLocale.o \
	MultiSync.o \
	mediadetails.o \
	mediaoutput/MediaOutputBase.o \
	mediaoutput/mediaoutput.o \
	mediaoutput/omxplayer.o \
	mediaoutput/SDLOut.o \
	mqtt.o \
 	NetworkMonitor.o \
	ping.o \
	overlays/PixelOverlay.o \
    overlays/PixelOverlayEffects.o \
	overlays/PixelOverlayModel.o \
	playlist/Playlist.o \
	playlist/PlaylistEntryBase.o \
	playlist/PlaylistEntryBoth.o \
	playlist/PlaylistEntryBranch.o \
	playlist/PlaylistEntryChannelTest.o \
	playlist/PlaylistEntryCommand.o \
	playlist/PlaylistEntryDynamic.o \
	playlist/PlaylistEntryEffect.o \
	playlist/PlaylistEntryEvent.o \
	playlist/PlaylistEntryImage.o \
	playlist/PlaylistEntryMedia.o \
	playlist/PlaylistEntryMQTT.o \
	playlist/PlaylistEntryPause.o \
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
	Warnings.o \
    util/GPIOUtils.o \
    util/I2CUtils.o \
    util/SPIUtils.o \
    util/tinyexpr.o \
    util/ExpressionProcessor.o \
    $(OBJECTS_GPIO_ADDITIONS)


LIBS_fpp_so += \
	-lstdc++fs \
	-lpthread -lrt \
    -lzstd -lz \
	-lgpiod \
	-lgpiodcxx \
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


ifneq ($(wildcard /usr/local/include/vlc/vlc.h),)
LIBS_fpp_so += -L/usr/local/lib -lvlc
OBJECTS_fpp_so += mediaoutput/VLCOut.o
CFLAGS_mediaoutput/mediaoutput.o+=-DHASVLC
endif


util/tinyexpr.o: util/tinyexpr.c fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk
	$(CCACHE) $(CCOMPILER) $(CFLAGS) $(CFLAGS_$@) -c $< -o $@


TARGETS += libfpp.so
OBJECTS_ALL+=$(OBJECTS_fpp_so)

libfpp.so: $(OBJECTS_fpp_so) $(DEPS_fpp_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_so) $(LIBS_fpp_so) $(LIBS_fpp_so) $(LDFLAGS) $(LDFLAGS_fpp_so) -Wl,-rpath=$(PWD):$(PWD)/../external/RF24/ -o $@
