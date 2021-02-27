
#ifndef MEDIA_H
#define MEDIA_H

#include "define.h"



struct VideoState;

struct MediaState
{
	VideoState *video;
	AVFormatContext *pFormatCtx;

	char* filename;
	//bool quit;

	MediaState(char *filename);

	~MediaState();

	bool openInput();
};

int decode_thread(void *data);
pid_t gettid();
#endif