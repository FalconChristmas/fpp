#include "fpp-pch.h"

#include "Player.h"
#include "PlaylistCommands.h"
#include "Scheduler.h"

std::unique_ptr<Command::Result> StopPlaylistCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.StopNow(1);
    return std::make_unique<Command::Result>("Stopped");
}
std::unique_ptr<Command::Result> StopGracefullyPlaylistCommand::run(const std::vector<std::string>& args) {
    bool loop = false;
    if (args.size()) {
        loop = args[0] == "true" || args[0] == "1";
    }
    Player::INSTANCE.StopGracefully(1, loop);
    return std::make_unique<Command::Result>("Stopping");
}
std::unique_ptr<Command::Result> RestartPlaylistCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.RestartItem();
    return std::make_unique<Command::Result>("Restarting");
}
std::unique_ptr<Command::Result> NextPlaylistCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.NextItem();
    return std::make_unique<Command::Result>("Next Item Playing");
}
std::unique_ptr<Command::Result> PrevPlaylistCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.PrevItem();
    return std::make_unique<Command::Result>("Prev Item Playing");
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
        Player::INSTANCE.StartPlaylist(args[0], r);
    }
    return std::make_unique<Command::Result>("Playlist Starting");
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
    Player::INSTANCE.StartPlaylist(args[0], r);
    return std::make_unique<Command::Result>("Playlist Starting");
}

std::unique_ptr<Command::Result> StartPlaylistAtCommand::run(const std::vector<std::string>& args) {
    bool r = false;
    int idx = std::atoi(args[1].c_str());
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
        // if we should be playing this playlist and repeat mode matches then let scheduler start it
        if (((Player::INSTANCE.GetStatus() == FPP_STATUS_IDLE) ||
             (Player::INSTANCE.GetPlaylistName() != args[0])) &&
            (args[0] == playlistName) &&
            (r == repeat)) {
            // Allow the scheduler to restart even if force stopped
            Player::INSTANCE.ClearForceStopped();
            scheduler->CheckIfShouldBePlayingNow(1);
        } else {
            Player::INSTANCE.StartPlaylist(args[0], r, idx - 1);
        }
    }
    return std::make_unique<Command::Result>("Playlist Starting");
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
        Player::INSTANCE.StartPlaylist(args[0], r, -2);
    }
    return std::make_unique<Command::Result>("Playlist Starting");
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

std::unique_ptr<Command::Result> PlaylistPauseCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.Pause();
    return std::make_unique<Command::Result>("Playlist Inserted");
}
std::unique_ptr<Command::Result> PlaylistResumeCommand::run(const std::vector<std::string>& args) {
    Player::INSTANCE.Resume();
    return std::make_unique<Command::Result>("Playlist Inserted");
}
