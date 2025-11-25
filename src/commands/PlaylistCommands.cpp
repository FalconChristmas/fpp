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

#include "fpp-pch.h"

#include "../common.h"
#include "../log.h"

#include "Player.h"
#include "PlaylistCommands.h"
#include "Scheduler.h"

StopPlaylistCommand::StopPlaylistCommand() :
    Command("Stop Now") {
}
std::unique_ptr<Command::Result> StopPlaylistCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.StopNow(1);
    return std::make_unique<Command::Result>("Stopped");
}

StopGracefullyPlaylistCommand::StopGracefullyPlaylistCommand() :
    Command("Stop Gracefully") {
    args.push_back(CommandArg("loop", "bool", "After Loop", false));
}
std::unique_ptr<Command::Result> StopGracefullyPlaylistCommand::run(const std::vector<std::string>& args) {
    bool loop = false;
    if (args.size()) {
        loop = args[0] == "true" || args[0] == "1";
    }
    Player::INSTANCE.StopGracefully(1, loop);
    return std::make_unique<Command::Result>("Stopping");
}

RestartPlaylistCommand::RestartPlaylistCommand() :
    Command("Restart Playlist Item") {
}
std::unique_ptr<Command::Result> RestartPlaylistCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.RestartItem();
    return std::make_unique<Command::Result>("Restarting");
}

NextPlaylistCommand::NextPlaylistCommand() :
    Command("Next Playlist Item") {
}
std::unique_ptr<Command::Result> NextPlaylistCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.NextItem();
    return std::make_unique<Command::Result>("Next Item Playing");
}

PrevPlaylistCommand::PrevPlaylistCommand() :
    Command("Prev Playlist Item") {
}
std::unique_ptr<Command::Result> PrevPlaylistCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.PrevItem();
    return std::make_unique<Command::Result>("Prev Item Playing");
}

StartPlaylistCommand::StartPlaylistCommand() :
    Command("Start Playlist") {
    args.push_back(CommandArg("name", "string", "Playlist Name")
                       .setContentListUrl("api/playlists/playable"));
    args.push_back(CommandArg("repeat", "bool", "Repeat", true).setDefaultValue("false"));
    args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
}
std::unique_ptr<Command::Result> StartPlaylistCommand::run(const std::vector<std::string>& args) {
    bool r = false;
    bool iNR = false;
    if (args.empty()) {
        LogWarn(VB_COMMAND, "Ignoring StartPlaylistCommand as no Playlist was supplied\n");
        return std::make_unique<Command::Result>("Playlist is a requirement argument");
    }
    if (args.size() > 1) {
        r = args[1] == "true" || args[1] == "1";
    }
    if (args.size() > 2) {
        iNR = args[2] == "true" || args[2] == "1";
    }
    if (!iNR || args[0] != Player::INSTANCE.GetPlaylistName()) {
        LogInfo(VB_COMMAND, "StartPlaylistCommand: Requesting playlist '%s', current='%s', status=%d\n",
                args[0].c_str(), Player::INSTANCE.GetPlaylistName().c_str(), Player::INSTANCE.GetStatus());
        
        // Always stop if not idle to avoid deferred start mechanism
        if (Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) {
            LogInfo(VB_COMMAND, "StartPlaylistCommand: Stopping current playlist\n");
            // Clear force-stopped flag to allow scheduler to work if needed
            Player::INSTANCE.ClearForceStopped();
            Player::INSTANCE.StopNow(1);
            // Brief wait to let stop complete - don't wait too long to avoid scheduler interference
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            LogInfo(VB_COMMAND, "StartPlaylistCommand: After stop wait, status=%d\n", Player::INSTANCE.GetStatus());
        }
        Player::INSTANCE.StartPlaylist(args[0], r);
    }
    return std::make_unique<Command::Result>("Playlist Starting");
}

TogglePlaylistCommand::TogglePlaylistCommand() :
    Command("Toggle Playlist") {
    args.push_back(CommandArg("name", "string", "Playlist Name")
                       .setContentListUrl("api/playlists/playable"));
    args.push_back(CommandArg("repeat", "bool", "Repeat", true).setDefaultValue("false"));
    args.push_back(CommandArg("stop", "string", "Stop Type")
                       .setContentList({ "Gracefully", "After Loop", "Now" })
                       .setDefaultValue("Gracefully"));
}
std::unique_ptr<Command::Result> TogglePlaylistCommand::run(const std::vector<std::string>& args) {
    bool r = false;
    std::string stopType = "Gracefully";
    if (args.size() > 1) {
        r = args[1] == "true" || args[1] == "1";
    }
    if (args.size() > 2) {
        stopType = args[2];
    }
    if (Player::INSTANCE.IsPlaying() && Player::INSTANCE.GetPlaylistName() == args[0]) {
        if (stopType == "Now") {
            Player::INSTANCE.StopNow(1);
        } else if (stopType == "After Loop") {
            Player::INSTANCE.StopGracefully(true, true);
        } else {
            Player::INSTANCE.StopGracefully(true, false);
        }
        return std::make_unique<Command::Result>("Playlist Stopping");
    }
    
    LogInfo(VB_COMMAND, "TogglePlaylistCommand: Requesting playlist '%s', current='%s', status=%d\n",
            args[0].c_str(), Player::INSTANCE.GetPlaylistName().c_str(), Player::INSTANCE.GetStatus());
    
    // If a different playlist is playing, stop it first to avoid race conditions
    if (Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) {
        LogInfo(VB_COMMAND, "TogglePlaylistCommand: Stopping current playlist\n");
        Player::INSTANCE.ClearForceStopped();
        Player::INSTANCE.StopNow(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        LogInfo(VB_COMMAND, "TogglePlaylistCommand: After stop wait, status=%d\n", Player::INSTANCE.GetStatus());
    }
    
    Player::INSTANCE.StartPlaylist(args[0], r);
    return std::make_unique<Command::Result>("Playlist Starting");
}

StartPlaylistAtCommand::StartPlaylistAtCommand() :
    Command("Start Playlist At Item") {
    args.push_back(CommandArg("name", "string", "Playlist Name")
                       .setContentListUrl("api/playlists"));
    args.push_back(CommandArg("item", "int", "Item Index").setRange(1, 100));
    args.push_back(CommandArg("repeat", "bool", "Repeat", true).setDefaultValue("false"));
    args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
}
std::unique_ptr<Command::Result> StartPlaylistAtCommand::run(const std::vector<std::string>& args) {
    bool r = false;

    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Playlist is a requirement argument");
    }
    int idx = 1;
    if (args.size() > 1) {
        idx = std::atoi(args[1].c_str());
    }
    bool iNR = false;
    if (args.size() > 2) {
        r = args[2] == "true" || args[2] == "1";
    }
    if (args.size() > 3) {
        iNR = args[3] == "true" || args[3] == "1";
    }
    if (!iNR || args[0] != Player::INSTANCE.GetPlaylistName()) {
        int scheduledRepeat = 0;
        std::string playlistName = scheduler->GetPlaylistThatShouldBePlaying(scheduledRepeat);
        bool repeat = scheduledRepeat;
        
        LogInfo(VB_COMMAND, "StartPlaylistAtCommand: args[0]='%s', status=%d, currentPlaylist='%s', scheduledPlaylist='%s'\n",
                 args[0].c_str(), Player::INSTANCE.GetStatus(), 
                 Player::INSTANCE.GetPlaylistName().c_str(), playlistName.c_str());
        
        // Only defer to scheduler if we're idle AND the requested playlist matches what should be scheduled
        // AND it's not currently being played. This prevents the race condition where we're playing a
        // scheduled background playlist and want to start a different playlist via API.
        if ((Player::INSTANCE.GetStatus() == FPP_STATUS_IDLE) &&
            (args[0] == playlistName) &&
            (r == repeat)) {
            LogInfo(VB_COMMAND, "StartPlaylistAtCommand: Deferring to scheduler\n");
            // Allow the scheduler to restart even if force stopped
            Player::INSTANCE.ClearForceStopped();
            scheduler->CheckIfShouldBePlayingNow(1);
        } else {
            // Always directly start the playlist if:
            // 1. Something is already playing (even if it's the scheduled playlist)
            // 2. The requested playlist doesn't match what's scheduled
            // 3. The repeat mode doesn't match
            
            // If playing a different playlist, stop the current one first
            if ((Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) &&
                (Player::INSTANCE.GetPlaylistName() != args[0])) {
                LogInfo(VB_COMMAND, "StartPlaylistAtCommand: Stopping current playlist '%s' before starting '%s'\n",
                         Player::INSTANCE.GetPlaylistName().c_str(), args[0].c_str());
                Player::INSTANCE.StopNow(1);
            }
            
            LogInfo(VB_COMMAND, "StartPlaylistAtCommand: Starting playlist '%s'\n", args[0].c_str());
            Player::INSTANCE.StartPlaylist(args[0], r, idx - 1);
        }
    }
    return std::make_unique<Command::Result>("Playlist Starting");
}

StartPlaylistAtRandomCommand::StartPlaylistAtRandomCommand() :
    Command("Start Playlist At Random Item") {
    args.push_back(CommandArg("name", "string", "Playlist Name")
                       .setContentListUrl("api/playlists"));
    args.push_back(CommandArg("repeat", "bool", "Repeat", true).setDefaultValue("false"));
    args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
}
std::unique_ptr<Command::Result> StartPlaylistAtRandomCommand::run(const std::vector<std::string>& args) {
    bool r = false;
    bool iNR = false;
    if (args.size() > 1) {
        r = args[1] == "true" || args[1] == "1";
    }
    if (args.size() > 2) {
        iNR = args[2] == "true" || args[2] == "1";
    }
    if (!iNR || args[0] != Player::INSTANCE.GetPlaylistName()) {
        LogInfo(VB_COMMAND, "StartPlaylistAtRandomCommand: Requesting playlist '%s', current='%s', status=%d\n",
                args[0].c_str(), Player::INSTANCE.GetPlaylistName().c_str(), Player::INSTANCE.GetStatus());
        
        // Always stop if not idle to avoid deferred start mechanism
        if (Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) {
            LogInfo(VB_COMMAND, "StartPlaylistAtRandomCommand: Stopping current playlist\n");
            // Clear force-stopped flag to allow scheduler to work if needed
            Player::INSTANCE.ClearForceStopped();
            Player::INSTANCE.StopNow(1);
            // Brief wait to let stop complete - don't wait too long to avoid scheduler interference
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            LogInfo(VB_COMMAND, "StartPlaylistAtRandomCommand: After stop wait, status=%d\n", Player::INSTANCE.GetStatus());
        }
        Player::INSTANCE.StartPlaylist(args[0], r, -2);
    }
    return std::make_unique<Command::Result>("Playlist Starting");
}

InsertPlaylistCommand::InsertPlaylistCommand() :
    Command("Insert Playlist After Current", "After the current item of the currently running playlist is complete, run the new playlist.  When complete, resumes the original playlist at the next position.") {
    args.push_back(CommandArg("name", "string", "Playlist Name")
                       .setContentListUrl("api/playlists/playable"));
    args.push_back(CommandArg("startItem", "int", "Start Item Index", true).setRange(-1, 100).setDefaultValue("-1"));
    args.push_back(CommandArg("endItem", "int", "End Item Index", true).setRange(-1, 100).setDefaultValue("-1"));
    args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
}
std::unique_ptr<Command::Result> InsertPlaylistCommand::run(const std::vector<std::string>& args) {
    int start = -1;
    int end = -1;
    bool iNR = false;
    if (args.size() > 1) {
        start = std::atoi(args[1].c_str());
    }
    if (args.size() > 2) {
        end = std::atoi(args[2].c_str());
    }
    if (args.size() > 3) {
        iNR = args[3] == "true" || args[3] == "1";
    }
    if (!iNR || args[0] != Player::INSTANCE.GetPlaylistName()) {
        Player::INSTANCE.InsertPlaylistAsNext(args[0], start > 0 ? start - 1 : -1, end > 0 ? end - 1 : -1);
    }
    return std::make_unique<Command::Result>("Playlist Inserted");
}

InsertPlaylistImmediate::InsertPlaylistImmediate() :
    Command("Insert Playlist Immediate", "If possible, pauses the currently running playlist and then runs the new playlist.  When complete, resumes the original playlist where it was paused.") {
    args.push_back(CommandArg("name", "string", "Playlist Name")
                       .setContentListUrl("api/playlists/playable"));
    args.push_back(CommandArg("startItem", "int", "Start Item Index", true).setRange(0, 100).setDefaultValue("0"));
    args.push_back(CommandArg("endItem", "int", "End Item Index", true).setRange(0, 100).setDefaultValue("0"));
    args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
}
std::unique_ptr<Command::Result> InsertPlaylistImmediate::run(const std::vector<std::string>& args) {
    int start = -1;
    int end = -1;
    bool iNR = false;
    if (args.size() > 1) {
        start = std::atoi(args[1].c_str());
    }
    if (args.size() > 2) {
        end = std::atoi(args[2].c_str());
    }
    if (args.size() > 3) {
        iNR = args[3] == "true" || args[3] == "1";
    }
    if (!iNR || args[0] != Player::INSTANCE.GetPlaylistName()) {
        Player::INSTANCE.InsertPlaylistImmediate(args[0], start > 0 ? start - 1 : -1, end > 0 ? end - 1 : -1);
    }
    return std::make_unique<Command::Result>("Playlist Inserted");
}

InsertRandomItemFromPlaylistCommand::InsertRandomItemFromPlaylistCommand() :
    Command("Insert Random Item From Playlist", "Run a random item from the given playlist.  When complete, resumes the original playlist.") {
    args.push_back(CommandArg("name", "string", "Playlist Name")
                       .setContentListUrl("api/playlists/playable"));
    args.push_back(CommandArg("immediate", "bool", "Immediate", true).setDefaultValue("false"));
}
std::unique_ptr<Command::Result> InsertRandomItemFromPlaylistCommand::run(const std::vector<std::string>& args) {
    bool immediate = false;
    if (args.size() > 1) {
        immediate = args[1] == "true" || args[1] == "1";
    }
    if (immediate) {
        Player::INSTANCE.InsertPlaylistImmediate(args[0], -2, 0);
    } else {
        Player::INSTANCE.InsertPlaylistAsNext(args[0], -2, 0);
    }
    return std::make_unique<Command::Result>("Playlist Inserted");
}

PlaylistPauseCommand::PlaylistPauseCommand() :
    Command("Pause Playlist", "If possible, pauses the currently running playlist.  This command can not be run from inside a playlist") {
}
std::unique_ptr<Command::Result> PlaylistPauseCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.Pause();
    return std::make_unique<Command::Result>("Playlist Paused");
}

PlaylistResumeCommand::PlaylistResumeCommand() :
    Command("Resume Playlist") {
}
std::unique_ptr<Command::Result> PlaylistResumeCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.Resume();
    return std::make_unique<Command::Result>("Playlist Restarted");
}
