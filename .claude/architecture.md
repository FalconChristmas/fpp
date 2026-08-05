# Architecture

## Daemon Startup (`src/fppd.cpp`)

The daemon initializes these core modules in order: signal handlers, platform GPIO provider, CurlManager, EPollManager (Linux) or kqueue (macOS), Events, FileMonitor, FPPLocale, NetworkMonitor, OutputMonitor, Settings, Warnings, Commands, Plugins, Player, Scheduler, Sequence, MultiSync, ChannelOutputSetup, ChannelTester, MediaOutput, PixelOverlayManager, HTTPApi (port 32322 localhost).

## Main Loop and Event Architecture

The main loop calls `EPollManager::INSTANCE.waitForEvents(sleepms)` (50ms idle, 10ms playing) which blocks on epoll (Linux) or kqueue (macOS). EPollManager manages a single timerfd (Linux) or `EVFILT_TIMER` (macOS) that wakes the loop precisely when `Timers` deadlines are due, avoiding unnecessary latency. The timer callback fires `Timers::INSTANCE.fireTimers()`. After epoll returns, the loop also runs `Timers::fireTimers()`, `CurlManager::processCurls()`, and `GPIOManager::CheckGPIOInputs()`.

## Core Shared Library (`libfpp.so` / `fpp_so.mk`)

Contains 125+ object files covering all core functionality. Key linked libraries: zstd, jsoncpp, curl, mosquitto, SDL2, FFmpeg (avformat/avcodec/avutil/swresample/swscale), GraphicsMagick, drogon/trantor, libgpiod, tag (audio metadata), vlc (optional), kms++ (optional).

## C++ Backend (`src/`)

- **fppd** (`fppd.cpp`): Main daemon entry point
- **fpp** (`fpp.cpp`): CLI tool connecting via domain socket, framebuffer device queries
- **libfpp** (`fpp_so.mk`): Core shared library containing all subsystems
