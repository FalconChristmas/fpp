#pragma once

#include <string>

class MediaDetails {
public:
    MediaDetails();
    ~MediaDetails();
    
    std::string title;
    std::string artist;
    std::string album;
	int   year;
    std::string comment;
	int   track;
    std::string genre;

	int length;
	int seconds;
	int minutes;
    int lengthMS;

	int bitrate;
	int sampleRate;
	int channels;

    void ParseMedia(const char *mediaFilename);
    void Clear();
    
    static MediaDetails INSTANCE;
    
private:

};
