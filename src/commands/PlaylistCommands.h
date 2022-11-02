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

class StopPlaylistCommand : public Command {
public:
    StopPlaylistCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StopGracefullyPlaylistCommand : public Command {
public:
    StopGracefullyPlaylistCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class RestartPlaylistCommand : public Command {
public:
    RestartPlaylistCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class NextPlaylistCommand : public Command {
public:
    NextPlaylistCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class PrevPlaylistCommand : public Command {
public:
    PrevPlaylistCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartPlaylistCommand : public Command {
public:
    StartPlaylistCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class TogglePlaylistCommand : public Command {
public:
    TogglePlaylistCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartPlaylistAtCommand : public Command {
public:
    StartPlaylistAtCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartPlaylistAtRandomCommand : public Command {
public:
    StartPlaylistAtRandomCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class InsertPlaylistCommand : public Command {
public:
    InsertPlaylistCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class InsertPlaylistImmediate : public Command {
public:
    InsertPlaylistImmediate();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class InsertRandomItemFromPlaylistCommand : public Command {
public:
    InsertRandomItemFromPlaylistCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class PlaylistPauseCommand : public Command {
public:
    PlaylistPauseCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class PlaylistResumeCommand : public Command {
public:
    PlaylistResumeCommand();
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
