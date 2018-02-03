#ifndef __MEDIADETAILS_H__
#define __MEDIADETAILS_H__

typedef struct mediaDetails {
	char *title;
	char *artist;
	char *album;
	int   year;
	char *comment;
	int   track;
	char *genre;

	int length;
	int seconds;
	int minutes;

	int bitrate;
	int sampleRate;
	int channels;
} MediaDetails;

void initMediaDetails(void);
void ParseMedia(const char *mediaFilename);

#endif //__MEDIADETAILS_H__
