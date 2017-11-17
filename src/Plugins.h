#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#include <stdbool.h>
#include <vector>
#include <string>

#include "Playlist.h"


// Use these to make code more readable
#define PLAYLIST_STARTING				true
#define PLAYLIST_STOPPING				false


class Callback
{
public:
	Callback(std::string, std::string);
	virtual ~Callback();

	std::string getName() { return mName; }
	std::string getFilename() { return mFilename; }

private:
	std::string mName;
	std::string mFilename;
};

class MediaCallback : public Callback
{
public:
	MediaCallback(std::string a, std::string b) : Callback(a, b) {}
	~MediaCallback();

	void run();
private:
};

class PlaylistCallback : public Callback
{
public:
	PlaylistCallback(std::string a, std::string b) : Callback(a, b) {}
	~PlaylistCallback();

	void run(PlaylistDetails *, bool);
private:
};

class NextPlaylistEntryCallback : public Callback
{
public:
	NextPlaylistEntryCallback(std::string a, std::string b) : Callback(a, b) {}
	~NextPlaylistEntryCallback();

	int run(const char *, int, int, bool, PlaylistEntry *);
private:
};

class EventCallback : public Callback
{
public:
	EventCallback(std::string a, std::string b) : Callback(a, b) {}
	~EventCallback();

	void run(char *, char *);
private:
};

class PluginCallbackManager
{
public:
	PluginCallbackManager();
	~PluginCallbackManager();
	void init(void);

	int nextPlaylistEntryCallback(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, PlaylistEntry *pe);
	void playlistCallback(PlaylistDetails *playlistDetails, bool starting);
	void eventCallback(char *id, char *impetus);
	void mediaCallback();

private:
	std::vector<Callback *> mCallbacks;
};

#endif //__PLUGINS_H__
