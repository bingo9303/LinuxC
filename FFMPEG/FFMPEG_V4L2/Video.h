
#ifndef VIDEO_H
#define VIDEO_H


#include "define.h"

struct MediaState;
/**
 * ������Ƶ��������ݷ�װ
 */
struct VideoState
{
	struct PacketQueue* videoq;        // �����video packet�Ķ��л���

	int stream_index;           // index of video stream
	AVCodecContext *video_ctx;  // have already be opened by avcodec_open2
	AVStream *stream;           // video stream

	struct FrameQueue frameq;          // ���������ԭʼ֡����
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


int decode(void *arg); // ��packet���룬����������Frame����FrameQueue������


#endif