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

class TriggerPresetCommand : public Command {
public:
    TriggerPresetCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class TriggerPresetInFutureCommand : public Command {
public:
    TriggerPresetInFutureCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class TriggerRemotePresetCommand : public Command {
public:
    TriggerRemotePresetCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class TriggerPresetSlotCommand : public Command {
public:
    TriggerPresetSlotCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class TriggerRemotePresetSlotCommand : public Command {
public:
    TriggerRemotePresetSlotCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class TriggerMultiplePresetsCommand : public Command {
public:
    TriggerMultiplePresetsCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class TriggerMultiplePresetSlotsCommand : public Command {
public:
    TriggerMultiplePresetSlotsCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class RunScriptEvent : public Command {
public:
    RunScriptEvent();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class RunRemoteScriptEvent : public Command {
public:
    RunRemoteScriptEvent();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartEffectCommand : public Command {
public:
    StartEffectCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartFSEQAsEffectCommand : public Command {
public:
    StartFSEQAsEffectCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StopEffectCommand : public Command {
public:
    StopEffectCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StopAllEffectsCommand : public Command {
public:
    StopAllEffectsCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StopFSEQAsEffectCommand : public Command {
public:
    StopFSEQAsEffectCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartRemoteEffectCommand : public Command {
public:
    StartRemoteEffectCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartRemoteFSEQEffectCommand : public Command {
public:
    StartRemoteFSEQEffectCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StopRemoteEffectCommand : public Command {
public:
    StopRemoteEffectCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartRemotePlaylistCommand : public Command {
public:
    StartRemotePlaylistCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class AllLightsOffCommand : public Command {
public:
    AllLightsOffCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class SwitchToPlayerModeCommand : public Command {
public:
    SwitchToPlayerModeCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class SwitchToRemoteModeCommand : public Command {
public:
    SwitchToRemoteModeCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

