#include "define.h"

extern bool quit;

MediaState::MediaState(char* input_file)
	:filename(input_file)
{
	pFormatCtx = nullptr;

	video = new VideoState();
	//quit = false;
}

MediaState::~MediaState()
{
	if (video)
		delete video;
}

bool MediaState::openInput()
{
	// Open input file
	
	pFormatCtx = avformat_alloc_context();
	avdevice_register_all();

	AVInputFormat* input_fmt = av_find_input_format("video4linux2");
	if(input_fmt == NULL) printf("find av_find_input_format faild\r\n");
	AVDictionary *options = NULL;
	av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "max_delay", "100000", 0);
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "input_format", "mjpeg", 0);
    av_dict_set(&options, "video_size", "1920x1080", 0);

	if (avformat_open_input(&pFormatCtx, filename, input_fmt, &options) < 0)
	{
		printf("avformat_open_input false\r\n");
		return false;
	}
		

	if (avformat_find_stream_info(pFormatCtx, nullptr) < 0)
	{
		printf("avformat_find_stream_info false\r\n");
		return false;
	}


	

	// Output the stream info to standard 
	av_dump_format(pFormatCtx, 0, filename, 0);

	for (uint32_t i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && video->stream_index < 0)
			video->stream_index = i;
	}

	if (video->stream_index < 0)
	{
		printf("%s","can't find video stream\r\n");
		return false;
	}
		

	// Fill video state
	video->video_ctx = avcodec_alloc_context3(NULL);
	if (video->video_ctx == NULL) {
		printf("%s","can't alloc pCodecCtx\r\n");
		return false;
	}
	
	avcodec_parameters_to_context(video->video_ctx, pFormatCtx->streams[video->stream_index]->codecpar);
	
	AVCodec *pVCodec = avcodec_find_decoder(video->video_ctx->codec_id);
	
	if (!pVCodec)
		return false;

	video->stream = pFormatCtx->streams[video->stream_index];
	
	avcodec_open2(video->video_ctx,pVCodec,NULL);


	printf("video codec id: %d\r\n",video->video_ctx->codec_id);
	printf("video format£º%s\r\n",pFormatCtx->iformat->name);
	printf("video time£º%d\r\n", (int)(pFormatCtx->duration)/1000000);
	printf("video sizeX=%d,sizeY=%d\r\n",video->video_ctx->width,video->video_ctx->height);
//	printf("video fps=%4.2f\r\n",dev->frame_rate[m]);
	printf("video codec name£º%s\r\n",pVCodec->name);
	printf("video pix_fmt £º%d\r\n",video->video_ctx->pix_fmt);

	return true;
}


int decode_thread(void *data)
{
	MediaState *media = (MediaState*)data;

	AVPacket *packet = av_packet_alloc();

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
		int ret = av_read_frame(media->pFormatCtx, packet);
		if (ret < 0)
		{
			if (ret == AVERROR_EOF)
				break;
			if (media->pFormatCtx->pb->error == 0) // No error,wait for user input
			{
				SDL_Delay(100);
				continue;
			}
			else
				break;
		}
	
		if (packet->stream_index == media->video->stream_index) // video stream
		{
			bingo_log(("111...%ld,,nb_packets = %d\r\n",getDebugTime(),media->video->videoq->nb_packets));
			media->video->videoq->enQueue(packet);
			av_packet_unref(packet);
		}		
		else
			av_packet_unref(packet);
	}

	av_packet_free(&packet);

	return 0;
}
