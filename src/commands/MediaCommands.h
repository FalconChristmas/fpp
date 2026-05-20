#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "Commands.h"
#include "mediaoutput/GStreamerOut.h"
#include "mediaoutput/mediaoutput.h"

class SetVolumeCommand : public Command {
public:
    SetVolumeCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class AdjustVolumeCommand : public Command {
public:
    AdjustVolumeCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class IncreaseVolumeCommand : public Command {
public:
    IncreaseVolumeCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class DecreaseVolumeCommand : public Command {
public:
    DecreaseVolumeCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class URLCommand : public Command {
public:
    URLCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

#ifdef HAS_GSTREAMER
class PlayMediaCommand : public Command {
public:
    PlayMediaCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StopMediaCommand : public Command {
public:
    StopMediaCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StopAllMediaCommand : public Command {
public:
    StopAllMediaCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StopMediaSlotCommand : public Command {
public:
    StopMediaSlotCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class SetSlotVolumeCommand : public Command {
public:
    SetSlotVolumeCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class MediaSlotStatusCommand : public Command {
public:
    MediaSlotStatusCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
#endif

#ifdef HAS_AES67_GSTREAMER
class AES67ApplyCommand : public Command {
public:
    AES67ApplyCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class AES67CleanupCommand : public Command {
public:
    AES67CleanupCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class AES67TestCommand : public Command {
public:
    AES67TestCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class ApplyRoutingPresetCommand : public Command {
public:
    ApplyRoutingPresetCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
#endif

#ifdef HAS_OPUS_RTP_GSTREAMER
class OpusRTPApplyCommand : public Command {
public:
    OpusRTPApplyCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class OpusRTPCleanupCommand : public Command {
public:
    OpusRTPCleanupCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
#endif
