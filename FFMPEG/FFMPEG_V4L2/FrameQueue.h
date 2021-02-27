
#ifndef FRAME_QUEUE_H
#define FRAME_QUEUE_H

#include "define.h"
extern "C"{

#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>

}


struct _frameBuffer
{
	AVFrame* frame;
	char useFlag;
};

struct FrameQueue
{
	std::queue<struct _frameBuffer*> queue;

	uint32_t nb_frames;

	SDL_mutex* mutex;
	SDL_cond * cond;

	struct _frameBuffer frameBuffer[BUFFER_NUM];

	FrameQueue();
	~FrameQueue();
	bool enQueue(const AVFrame* frame);
	bool deQueue(AVFrame **frame);

	struct _frameBuffer* getEmptyFrame(void);
};



#endif