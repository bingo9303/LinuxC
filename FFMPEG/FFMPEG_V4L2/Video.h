
#ifndef VIDEO_H
#define VIDEO_H


#include "define.h"

struct MediaState;
/**
 * 播放音频所需的数据封装
 */
struct VideoState
{
	struct PacketQueue* videoq;        // 保存的video packet的队列缓存

	int stream_index;           // index of video stream
	AVCodecContext *video_ctx;  // have already be opened by avcodec_open2
	AVStream *stream;           // video stream

	struct FrameQueue frameq;          // 保存解码后的原始帧数据
	AVFrame *frame;
	AVFrame *displayFrame;

	SwsContext *sws_ctx;


	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *bmp;
	SDL_Rect rect_scale;
	SDL_Rect rect_crop;


	void video_play(MediaState *media);
	
	VideoState();

	~VideoState();
};


int decode(void *arg); // 将packet解码，并将解码后的Frame放入FrameQueue队列中


#endif