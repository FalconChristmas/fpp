#include "fpp-pch.h"

#include "PlaylistCommands.h"
#include "playlist/Playlist.h"


std::unique_ptr<Command::Result> StopPlaylistCommand::run(const std::vector<std::string> &args) {
    playlist->StopNow();
    return std::make_unique<Command::Result>("Stopped");
}
std::unique_ptr<Command::Result> StopGracefullyPlaylistCommand::run(const std::vector<std::string> &args) {
    bool loop = false;
    if (args.size() > 1) {
        loop = args[1] == "true" || args[1] == "1";
    }
    playlist->StopGracefully(loop, loop);
    return std::make_unique<Command::Result>("Stopping");
}
std::unique_ptr<Command::Result> RestartPlaylistCommand::run(const std::vector<std::string> &args) {
    playlist->RestartItem();
    return std::make_unique<Command::Result>("Restarting");
}
std::unique_ptr<Command::Result> NextPlaylistCommand::run(const std::vector<std::string> &args) {
    playlist->NextItem();
    return std::make_unique<Command::Result>("Next Item Playing");
}
std::unique_ptr<Command::Result> PrevPlaylistCommand::run(const std::vector<std::string> &args) {
    playlist->PrevItem();
    return std::make_unique<Command::Result>("Prev Item Playing");
}


std::unique_ptr<Command::Result> StartPlaylistCommand::run(const std::vector<std::string> &args) {
    bool r = false;
    if (args.size() > 1) {
        r = args[1] == "true" || args[1] == "1";
    }
    playlist->Play(args[0].c_str(), -1, r);
    return std::make_unique<Command::Result>("Playlist Starting");
}

std::unique_ptr<Command::Result> TogglePlaylistCommand::run(const std::vector<std::string> &args) {
    bool r = false;
    std::string stopType = "Gracefully";
    if (args.size() > 1) {
        r = args[1] == "true" || args[1] == "1";
    }
    if (args.size() > 2) {
        stopType = args[2];
    }
    if (playlist->IsPlaying() && playlist->GetPlaylistName() == args[0]) {
        if (stopType == "Now") {
            playlist->StopNow();
        } else if (stopType == "After Loop") {
            playlist->StopGracefully(true, true);
        } else {
            playlist->StopGracefully(false, false);
        }
        return std::make_unique<Command::Result>("Playlist Stopping");
    }
    playlist->Play(args[0].c_str(), -1, r);
    return std::make_unique<Command::Result>("Playlist Starting");
}

std::unique_ptr<Command::Result> StartPlaylistAtCommand::run(const std::vector<std::string> &args) {
    bool r = false;
    int idx = std::atoi(args[1].c_str());
    if (args.size() > 2) {
        r = args[2] == "true" || args[2] == "1";
    }
    playlist->Play(args[0].c_str(), idx - 1, r);
    return std::make_unique<Command::Result>("Playlist Starting");
}

std::unique_ptr<Command::Result> StartPlaylistAtRandomCommand::run(const std::vector<std::string> &args) {
    bool r = false;
    if (args.size() > 1) {
        r = args[1] == "true" || args[1] == "1";
    }
    playlist->Play(args[0].c_str(), -2, r);
    return std::make_unique<Command::Result>("Playlist Starting");
}

std::unique_ptr<Command::Result> InsertPlaylistCommand::run(const std::vector<std::string> &args) {
    int start = -1;
    int end = -1;
    if (args.size() > 1) {
        start = std::atoi(args[1].c_str());
    }
    if (args.size() > 2) {
        end = std::atoi(args[2].c_str());
    }
    playlist->InsertPlaylistAsNext(args[0], start - 1, end - 1);
    return std::make_unique<Command::Result>("Playlist Inserted");
}


std::unique_ptr<Command::Result> InsertPlaylistImmediate::run(const std::vector<std::string> &args) {
    int start = -1;
    int end = -1;
    if (args.size() > 1) {
        start = std::atoi(args[1].c_str());
    }
    if (args.size() > 2) {
        end = std::atoi(args[2].c_str());
    }
    playlist->InsertPlaylistImmediate(args[0], start - 1, end - 1);
    return std::make_unique<Command::Result>("Playlist Inserted");
}


std::unique_ptr<Command::Result> PlaylistPauseCommand::run(const std::vector<std::string> &args) {
    playlist->Pause();
    return std::make_unique<Command::Result>("Playlist Inserted");
}
std::unique_ptr<Command::Result> PlaylistResumeCommand::run(const std::vector<std::string> &args) {
    playlist->Resume();
    return std::make_unique<Command::Result>("Playlist Inserted");
}
