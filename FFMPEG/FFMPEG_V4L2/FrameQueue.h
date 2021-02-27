
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



struct FrameQueue
{
	std::queue<AVFrame*> queue;

	uint32_t nb_frames;

	SDL_mutex* mutex;
	SDL_cond * cond;

	FrameQueue();
	bool enQueue(const AVFrame* frame);
	bool deQueue(AVFrame **frame);
};



#endif