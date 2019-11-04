#include "PlaylistCommands.h"
#include "playlist/Playlist.h"


std::unique_ptr<Command::Result> StopPlaylistCommand::run(const std::vector<std::string> &args) {
    playlist->StopNow();
    return std::make_unique<Command::Result>("Stopped");
}
std::unique_ptr<Command::Result> StopGracefullyPlaylistCommand::run(const std::vector<std::string> &args) {
    playlist->StopGracefully();
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
    playlist->InsertPlaylistAsNext(args[0]);
    return std::make_unique<Command::Result>("Playlist Inserted");
}
