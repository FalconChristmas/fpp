#ifndef __MEDIADETAILS_H__
#define __MEDIADETAILS_H__

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

	int bitrate;
	int sampleRate;
	int channels;

    void ParseMedia(const char *mediaFilename);
    
    static MediaDetails INSTANCE;
    
private:
    void clearPreviousMedia();

};

#endif //__MEDIADETAILS_H__
