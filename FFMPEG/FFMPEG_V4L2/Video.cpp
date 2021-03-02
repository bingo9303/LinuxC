
#include "define.h"

VideoState::VideoState()
{
	video_ctx        = nullptr;
	stream_index     = -1;
	stream           = nullptr;
	sws_ctx			 = nullptr;

	window           = nullptr;
	bmp              = nullptr;
	renderer         = nullptr;

	frame            = nullptr;
	displayFrame     = nullptr;

	videoq           = new PacketQueue();
}

VideoState::~VideoState()
{
	delete videoq;
	sws_freeContext(sws_ctx);
	av_frame_free(&frame);
	av_free(displayFrame->data[0]);
	av_frame_free(&displayFrame);
}

void VideoState::video_play(MediaState *media)
{
	int width = video_ctx->width;
	int height = video_ctx->height;
	// ´´½¨sdl´°¿Ú
	window = SDL_CreateWindow("FFmpeg Decode", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		width, height, /*SDL_WINDOW_OPENGL*/SDL_WINDOW_FULLSCREEN_DESKTOP);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE/*SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC*/);
	bmp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR888/*SDL_PIXELFORMAT_BGR888*/, SDL_TEXTUREACCESS_STREAMING,
		width, height);

	rect_scale.x = 0;
	rect_scale.y = 0;
	rect_scale.w = width;
	rect_scale.h = height;

	
	rect_crop.x = 0;
	rect_crop.y = 0;
	rect_crop.w = video_ctx->width;
	rect_crop.h = video_ctx->height;

	frame = av_frame_alloc();
	displayFrame = av_frame_alloc();

	displayFrame->format = AV_PIX_FMT_0BGR32;
	displayFrame->width = width;
	displayFrame->height = height;

	uint8_t *buffer = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_0BGR32,video_ctx->width,video_ctx->height,1));

	
	av_image_fill_arrays(displayFrame->data,displayFrame->linesize,buffer,AV_PIX_FMT_0BGR32,video_ctx->width, video_ctx->height,1);

	sws_ctx = sws_getContext(video_ctx->width, video_ctx->height, video_ctx->pix_fmt,
					displayFrame->width,displayFrame->height,(AVPixelFormat)displayFrame->format, 0, nullptr, nullptr, nullptr);
					


	SDL_CreateThread(decode, "", this);

	schedule_refresh(media, 40); // start display
}


int  decode(void *arg)
{
	VideoState *video = (VideoState*)arg;

	AVFrame *frame = av_frame_alloc();

	AVPacket packet;
	char cpuIndex;
	int res;
	cpu_set_t cpuset;
	
	pid_t pid = gettid();
	pthread_t tid = pthread_self();
	printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);
	CPU_ZERO(&cpuset);
	for (cpuIndex = 0; cpuIndex < 4; cpuIndex++)
		CPU_SET(cpuIndex, &cpuset);
	
	res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
		
	
	while (true)
	{
		video->videoq->deQueue(&packet, true);

		int ret = avcodec_send_packet(video->video_ctx, &packet);
		if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
			continue;

		ret = avcodec_receive_frame(video->video_ctx, frame);
		if (ret < 0 && ret != AVERROR_EOF)
			continue;
		
		
		bingo_log(("222...%ld,,nb_frames = %d\r\n",getDebugTime(),video->frameq.nb_frames));

	/*	#if (BINGO_DEBUG==4)
			SDL_Delay(40);	
		#else if(BINGO_DEBUG==2)
			SDL_Delay(30);
		#endif*/
		
		av_packet_unref(&packet);
		video->frameq.enQueue(frame);
		/*
		while(video->frameq.queue.size() >= BUFFER_NUM)	
		{
			SDL_Delay(2);
		}*/

		av_frame_unref(frame);
		//usleep(10000);
	}


	av_frame_free(&frame);

	return 0;
}

