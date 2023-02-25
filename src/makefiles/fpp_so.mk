

OBJECTS_fpp_so += \
	channeloutput/ChannelOutput.o \
	channeloutput/ThreadedChannelOutput.o \
	channeloutput/ChannelOutputSetup.o \
	channeloutput/channeloutputthread.o \
	channeloutput/ColorOrder.o \
	channeloutput/FPD.o \
	channeloutput/Matrix.o \
	channeloutput/PanelMatrix.o \
	channeloutput/PixelString.o \
	channeloutput/serialutil.o \
	channeloutput/VirtualDisplayBase.o \
    channeloutput/processors/OutputProcessor.o \
    channeloutput/processors/RemapOutputProcessor.o \
    channeloutput/processors/HoldValueOutputProcessor.o \
    channeloutput/processors/SetValueOutputProcessor.o \
    channeloutput/processors/BrightnessOutputProcessor.o \
    channeloutput/processors/ColorOrderOutputProcessor.o \
    channeloutput/processors/ThreeToFourOutputProcessor.o \
	channeloutput/processors/OverrideZeroOutputProcessor.o \
	channeloutput/stringtesters/PixelCountStringTester.o \
    channeloutput/stringtesters/PixelFadeStringTester.o \
	channeloutput/stringtesters/PixelStringTester.o \
	channeloutput/stringtesters/PortNumberStringTester.o \
	channeltester/ChannelTester.o \
	channeltester/OutputTester.o \
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
	Events.o \
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
	mediaoutput/SDLOut.o \
	mediaoutput/VLCOut.o \
	mqtt.o \
	NetworkController.o \
 	NetworkMonitor.o \
	ping.o \
	Player.o \
	overlays/PixelOverlay.o \
    overlays/PixelOverlayEffects.o \
	overlays/PixelOverlayModel.o \
	overlays/PixelOverlayModelFB.o \
	overlays/PixelOverlayModelSub.o \
    overlays/WLEDEffects.o \
    overlays/wled/FX.o \
    overlays/wled/FX_fcn.o \
    overlays/wled/colorpalettes.o \
    overlays/wled/colorutils.o \
    overlays/wled/noise.o \
    overlays/wled/hsv2rgb.o \
	playlist/Playlist.o \
	playlist/PlaylistEntryBase.o \
	playlist/PlaylistEntryBoth.o \
	playlist/PlaylistEntryBranch.o \
	playlist/PlaylistEntryCommand.o \
	playlist/PlaylistEntryDynamic.o \
	playlist/PlaylistEntryEffect.o \
	playlist/PlaylistEntryImage.o \
	playlist/PlaylistEntryMedia.o \
	playlist/PlaylistEntryPause.o \
	playlist/PlaylistEntryPlaylist.o \
	playlist/PlaylistEntryPlugin.o \
	playlist/PlaylistEntryRemap.o \
	playlist/PlaylistEntryScript.o \
	playlist/PlaylistEntrySequence.o \
	playlist/PlaylistEntryURL.o \
	Plugins.o \
	Scheduler.o \
	ScheduleEntry.o \
	scripts.o \
	sensors/Sensors.o \
	Sequence.o \
	settings.o \
	SunRise.o \
	Timers.o \
	Warnings.o \
    util/GPIOUtils.o \
    util/I2CUtils.o \
    util/SPIUtils.o \
    util/tinyexpr.o \
    util/ExpressionProcessor.o \
	util/TmpFileGPIO.o \
	util/RegExCache.o \
    $(OBJECTS_GPIO_ADDITIONS)


LIBS_fpp_so += \
    -lzstd -lz \
	-lhttpserver \
	-ljsoncpp \
	-lm \
	-lcurl \
	-lmosquitto \
	-lutil \
	-ltag \
	-lSDL2 \
	-lavformat \
	-lavcodec \
	-lavutil \
	-lswresample \
	-L/usr/local/lib \
	-lswscale \
	-lGraphicsMagick \
	-lGraphicsMagickWand \
	-lGraphicsMagick++ \
    $(LIBS_GPIO_ADDITIONS) $(LD_FLAG_FS)

ifneq ($(wildcard /usr/local/include/vlc/vlc.h),)
LIBS_fpp_so += -lvlc
endif

ifneq ($(wildcard /usr/include/libdrm/drm.h),)
CXXFLAGS_FrameBuffer.o += -I/usr/include/libdrm
CXXFLAGS_overlays/PixelOverlay.o  += -I/usr/include/libdrm
CXXFLAGS_overlays/PixelOverlayModelFB.o  += -I/usr/include/libdrm
endif

util/tinyexpr.o: util/tinyexpr.c fppversion_defines.h Makefile makefiles/*.mk makefiles/platform/*.mk $(PCH_FILE)
	$(CCACHE) $(CCOMPILER) $(CFLAGS) $(CFLAGS_$@) -c $(SRCDIR)$< -o $@

TARGETS += libfpp.$(SHLIB_EXT)
OBJECTS_ALL+=$(OBJECTS_fpp_so)

libfpp.$(SHLIB_EXT): $(OBJECTS_fpp_so) $(DEPS_fpp_so)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_so) $(LIBS_fpp_so) $(LDFLAGS) $(LDFLAGS_fpp_so) -o $@


CXXFLAGS_overlays/PixelOverlay.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_overlays/PixelOverlayEffects.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_overlays/PixelOverlayModel.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_playlist/PlaylistEntryImage.o+=$(MAGICK_INCLUDE_PATH)
CXXFLAGS_channeloutput/VirtualDisplayBase.o+=$(MAGICK_INCLUDE_PATH)
