#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <jpeglib.h>
#include <jerror.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h> 
#include <errno.h>
#include <sys/time.h>
#include <getopt.h> 
#include <semaphore.h>
#include <signal.h>
#include <getopt.h> 
#define __USE_GNU  				//绑定核心需要的头文件
#include <sched.h> 				//绑定核心需要的头文件，需要在pthread.h之前
#include <pthread.h>
#include <sys/syscall.h>
#include <SDL2/SDL.h>


#include <iostream>

extern "C" { 

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h> 
#include <libavdevice/avdevice.h>

#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

}




using namespace std;


#define MAX_CH_NUM		2


typedef struct
{
	char inputPort;		// 0 ~ 3  port
	char freeze;        // 0 -- off, 1 -- on
	char horizontalFlip; // 0 -- off, 1 -- on
	char rotate; // 0 -- off, 1 -- left rotation, 2 -- right rotation.
	SDL_Rect crop;
	SDL_Rect scale;	
}LayerSetting;



typedef struct
{
	unsigned char layer[MAX_CH_NUM];
	unsigned char alpha[MAX_CH_NUM];
	char alphaFlag[MAX_CH_NUM];				//1-淡入  2-淡出0-不做处理
} OutLayerSetting;


typedef struct _sdl2_display_info_multiple_input
{
/* 输出参数 */
	int outputChNum;
	char* windowsName;
	int windowsSize_width;
	int windowsSize_height;

/* 输入参数 */
	char** inputFile[MAX_CH_NUM];
	int inputSourceNum;	
	int imageSize_width[MAX_CH_NUM];
	int imageSize_height[MAX_CH_NUM];
	unsigned char* imageFrameBuffer[MAX_CH_NUM][3];		//一帧图像的buff	
	int pixformat[MAX_CH_NUM];
	int components[MAX_CH_NUM];
	unsigned int oneLineByteSize[MAX_CH_NUM][3];
	int frame_time[MAX_CH_NUM];
	double frame_rate[MAX_CH_NUM];
/* 图层参数 */
	LayerSetting layerInfo[MAX_CH_NUM];
	OutLayerSetting outLayerAlpha;
	int alphaTime;

/* FFMEPH参数 */
	AVFormatContext *pFormatCtx[MAX_CH_NUM];
	AVCodecContext *pCodecCtx[MAX_CH_NUM];
	AVCodec *pCodec[MAX_CH_NUM];
	AVPacket *packet[MAX_CH_NUM];
	AVFrame *pFrame[MAX_CH_NUM];
	AVFrame *pFrameDecode[MAX_CH_NUM];
	uint8_t *out_buffer[MAX_CH_NUM];
	struct SwsContext *sws_ctx[MAX_CH_NUM];
	int v_stream_idx[MAX_CH_NUM];

/* SDL2 info */
	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture[MAX_CH_NUM];

	
}camera_info;






#define ABS(x) ((x)<0?-(x):(x))


#define TEST_ENABLE	1

typedef void * (*_pthreadFunc) (void*);

int fd_tty;


pthread_t tid_alpha;
pthread_t tid_command;
pthread_t tid_clear;
pthread_t tid_decode[MAX_CH_NUM];
pthread_t tid_display[MAX_CH_NUM];

pthread_mutex_t sdl2_mutex = PTHREAD_MUTEX_INITIALIZER ;

static int stopAllTaskFlag=0;

sem_t		display_sem[MAX_CH_NUM];

/* 时间戳 */
static struct timeval decode_tv[MAX_CH_NUM][2];
static struct timeval display_tv[MAX_CH_NUM][2];


static const char short_options [] = "i:x:y:";  
  
static const struct option  
long_options [] = {  
       	{ "input",     	 required_argument,     NULL,          'i' }, 
        { "sizex",     	 no_argument,       	NULL,          'x' }, 
        { "sizey",       no_argument,           NULL,          'y' },  
        { "layer",       no_argument,           NULL,          'l' }, 
        { "alpha",		 no_argument,           NULL,          'a' },
        { 0, 0, 0, 0 }  
};  
		
pid_t gettid() {
 return syscall(SYS_gettid);
}





int init_sdl2_display(camera_info* dev)
{
	int i;
	if(dev->windowsName == NULL) dev->windowsName = "RgbLink Uvc Camera Video";
		
	if (SDL_Init(SDL_INIT_VIDEO)) {
		  printf("Could not initialize SDL - %s\n", SDL_GetError());
		  exit (EXIT_FAILURE); 
	}

   	dev->screen = SDL_CreateWindow(dev->windowsName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        dev->windowsSize_width, dev->windowsSize_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	if (!dev->screen) {
        printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
        exit (EXIT_FAILURE);
    }

	dev->sdlRenderer = SDL_CreateRenderer(dev->screen, -1, SDL_RENDERER_ACCELERATED);

	for(i=0;i<dev->inputSourceNum;i++)
	{
		LayerSetting* layer = &dev->layerInfo[i];
		dev->sdlTexture[i] = SDL_CreateTexture(dev->sdlRenderer, SDL_PIXELFORMAT_BGR888/*dev->pixformat[layer->inputPort]*/, SDL_TEXTUREACCESS_STREAMING, dev->imageSize_width[layer->inputPort], dev->imageSize_height[layer->inputPort]);
		SDL_SetTextureBlendMode(dev->sdlTexture[i],SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod(dev->sdlTexture[i],dev->outLayerAlpha.alpha[i]);
	}
	
    return (0);
}

void quit_sdl2_display(camera_info* dev)
{
	SDL_Quit();
}


static void set_sdl_yuv_conversion_mode(AVFrame *frame)
{
#if SDL_VERSION_ATLEAST(2,0,8)
    SDL_YUV_CONVERSION_MODE mode = SDL_YUV_CONVERSION_AUTOMATIC;
    if (frame && (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUYV422 || frame->format == AV_PIX_FMT_UYVY422)) {
        if (frame->color_range == AVCOL_RANGE_JPEG)
            mode = SDL_YUV_CONVERSION_JPEG;
        else if (frame->colorspace == AVCOL_SPC_BT709)
            mode = SDL_YUV_CONVERSION_BT709;
        else if (frame->colorspace == AVCOL_SPC_BT470BG || frame->colorspace == AVCOL_SPC_SMPTE170M || frame->colorspace == AVCOL_SPC_SMPTE240M)
            mode = SDL_YUV_CONVERSION_BT601;
    }
    SDL_SetYUVConversionMode(mode);
#endif
}


void sdl2_display_frame(camera_info* dev,int layerCh)
{
	LayerSetting* layer = &dev->layerInfo[layerCh];
#if 0
	/*if(dev->pixformat[layer->inputPort] == SDL_PIXELFORMAT_YUY2)
	{
		SDL_UpdateYUVTexture(dev->sdlTexture[layerCh], NULL,
                             dev->imageFrameBuffer[layer->inputPort][0], dev->oneLineByteSize[layer->inputPort][0],
                             dev->imageFrameBuffer[layer->inputPort][1], dev->oneLineByteSize[layer->inputPort][1],
                             dev->imageFrameBuffer[layer->inputPort][2], dev->oneLineByteSize[layer->inputPort][2]);
	}
	else*/
	{
		SDL_UpdateTexture(dev->sdlTexture[layerCh], NULL, dev->imageFrameBuffer[layer->inputPort][0], dev->oneLineByteSize[layer->inputPort][0]);
		
	}
    SDL_RenderCopy(dev->sdlRenderer, dev->sdlTexture[layerCh], &layer->crop, &layer->scale);
#endif

	SDL_UpdateTexture(dev->sdlTexture[layerCh], NULL, dev->pFrame[0]->data[0], dev->pFrame[0]->linesize[0]);

	set_sdl_yuv_conversion_mode(dev->pFrame[0]);
	SDL_RenderCopy(dev->sdlRenderer, dev->sdlTexture[layerCh], &layer->crop, &layer->scale);
	// SDL_RenderCopyEx(dev->sdlRenderer, dev->sdlTexture[layerCh], NULL, &layer->scale, 0, NULL, 0);
	   set_sdl_yuv_conversion_mode(NULL);
}

void sdl2_clear_frame(camera_info* dev)
{
	SDL_RenderClear(dev->sdlRenderer);
}

void sdl2_present_frame(camera_info* dev)
{
	SDL_RenderPresent(dev->sdlRenderer);
}


void sdl2_SetAlpha(camera_info* dev,int layerCh,int value)
{
	unsigned char temp = value & 0xFF;
	SDL_SetTextureAlphaMod(dev->sdlTexture[layerCh],temp);
}

int sdl2_GetAlpha(camera_info* dev,int layerCh,int* value)
{
	unsigned char temp;
	int res;
	res = SDL_GetTextureAlphaMod(dev->sdlTexture[layerCh],&temp);
	*value = temp;
	return res;
}




void* task_command(void* args)
{
	int inputcharNum;
	char buf_tty[10];
	camera_info* dev = (camera_info*)args;
	while(1)
	{/*
		//到时候要彻底优化一下命令行的输入机制
		memset(buf_tty,0,sizeof(buf_tty));
		inputcharNum=read(fd_tty,buf_tty,10);
        if(inputcharNum<0)
        {
        	//printf("no inputs\n");
        }
		else
		{
			if(buf_tty[0] == 'q')	
			{
				stopAllTaskFlag = 1;
				break;
			}
			else if(buf_tty[0] == 's')
			{
				OutLayerSetting* outLayerAlpha = &dev->outLayerAlpha;
				sdl2_GetAlpha(dev,0,&outLayerAlpha->alpha[0]);
				if(outLayerAlpha->alpha[0] != 0xFF)
				{
					outLayerAlpha->alphaFlag[0] = 1;
					outLayerAlpha->alphaFlag[1] = 2;
				}
				else
				{
					outLayerAlpha->alphaFlag[0] = 2;
					outLayerAlpha->alphaFlag[1] = 1;
				}
			}
			else if(buf_tty[0] == 't')
			{
				int ret = 0;
				int layerCh = buf_tty[1] - '0';
				OutLayerSetting* outLayerAlpha = &dev->outLayerAlpha;
				if((layerCh>=1) && (layerCh<=dev->outputChNum))
				{
					ret = sdl2_GetAlpha(dev,layerCh-1,&outLayerAlpha->alpha[layerCh-1]);
					if(ret == 0)
					{
						if(outLayerAlpha->alpha[layerCh-1] != 0xFF)
						{
							outLayerAlpha->alphaFlag[layerCh-1] = 1;
						}
						else
						{
							outLayerAlpha->alphaFlag[layerCh-1] = 2;
						}
					}
				}
			}
		}
		usleep(500000);*/
	}
	return (void*) 0;
}

void* task_clear(void* args)
{
	int layerCh,clearFlag=0;
	camera_info* dev = (camera_info*)args;
	int delayTime = (dev->alphaTime*1000)/255;
	while(1)
	{/*
		if(stopAllTaskFlag == 1)	break;
		clearFlag = 0;
		for(layerCh=0;layerCh<dev->outputChNum;layerCh++)
		{
			if(dev->outLayerAlpha.alphaFlag[layerCh] != 0)
			{
				clearFlag = 1;
				break;
			}
		}

		if(clearFlag)
		{
			pthread_mutex_lock(&sdl2_mutex);
		//	sdl2_clear_frame(dev);
		//	sdl2_display_frame(dev,0);	
		//	sdl2_display_frame(dev,1);	
		//	sdl2_present_frame(dev);
			pthread_mutex_unlock(&sdl2_mutex);
			usleep(1);
		}
		else
		{
			usleep(delayTime);
		}*/
	}
	return (void*)0;
}


void* task_alpha(void* args)
{
	camera_info* dev = (camera_info*)args;
	int delayTime = (dev->alphaTime*1000)/255;
	int layerCh;
	while(1)
	{/*
		if(stopAllTaskFlag == 1)	break;
		for(layerCh=0;layerCh<dev->outputChNum;layerCh++)
		{
			if(dev->outLayerAlpha.alphaFlag[layerCh] == 1)
			{
				dev->outLayerAlpha.alpha[layerCh]++;
				sdl2_SetAlpha(dev,layerCh,(dev->outLayerAlpha.alpha[layerCh]&0xFF));
				if(dev->outLayerAlpha.alpha[layerCh] >= 0xFF)	
					dev->outLayerAlpha.alphaFlag[layerCh] = 0;
			}
			else if(dev->outLayerAlpha.alphaFlag[layerCh] == 2)
			{
				dev->outLayerAlpha.alpha[layerCh]--;
				sdl2_SetAlpha(dev,layerCh,(dev->outLayerAlpha.alpha[layerCh]&0xFF));
				if(dev->outLayerAlpha.alpha[layerCh] <= 0)	
					dev->outLayerAlpha.alphaFlag[layerCh] = 0;
			}
		}*/
		usleep(delayTime);
	}
}


 
 
static int init_filter_graph(AVFilterGraph **graph, AVFilterContext **src,
                             AVFilterContext **sink)
{
    char *filter_descr = "boxblur";//"scale=1920:1920";
    char args[512];
    int ret = 0;
    AVFilterGraph *filter_graph;
    AVFilterContext *buffer_ctx;
    AVFilterContext *buffersink_ctx;
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_0BGR32, AV_PIX_FMT_NONE };
    AVRational time_base = (AVRational){1, 25};
    
    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=0/1",
             1920, 1080, 13,
             1, 1000000);

			printf("%s\r\n",args);

			//	video_size=1920x1080:pix_fmt=13:time_base=1/1000000:pixel_aspect=0/1


			
    ret = avfilter_graph_create_filter(&buffer_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        goto end;
    }
	printf("aaaaaa\r\n"); 
    
    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
        goto end;
    }
	printf("bbbbbbbb\r\n"); 
    
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
        goto end;
    }
	printf("ccccccc\r\n"); 
    
    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */
    
    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffer_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;
    
    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
    
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_descr,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;
	printf("dddddd\r\n"); 

    
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;
    printf("eeeeeee\r\n"); 
    
    *graph = filter_graph;
    *src   = buffer_ctx;
    *sink  = buffersink_ctx;
    
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);


	printf("end\r\n");
    
    return ret;
}



void* task_decode_0(void* args)
{
	camera_info* dev = (camera_info*)args;
	int got_picture, ret;
	int delayTime; 

	char cpuIndex;
	int res;
	cpu_set_t cpuset;

	
	pid_t pid = gettid();
	pthread_t tid = pthread_self();
	printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);
	CPU_ZERO(&cpuset);
	for (cpuIndex = 4; cpuIndex < 6; cpuIndex++)
		CPU_SET(cpuIndex, &cpuset);
	
	res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
	if (res != 0)
	{
		printf("pthread_setaffinity_np faild\r\n");
	}


	
	AVFilterGraph *graph;
	AVFilterContext *src, *sink;
	int err;
	err = init_filter_graph(&graph, &src, &sink);
   	if (err < 0) {
	   fprintf(stderr, "Unable to init filter graph:");
	   exit(1);
   }

	
	while (av_read_frame(dev->pFormatCtx[0], dev->packet[0]) >= 0)
    {
    	if(stopAllTaskFlag == 1)	break;
		
        if (dev->packet[0]->stream_index == dev->v_stream_idx[0])
        {
        	gettimeofday(&decode_tv[0][0],NULL);
			printf("0.delayTime = %d\r\n",decode_tv[0][0].tv_usec);
			{
				ret = avcodec_send_packet(dev->pCodecCtx[0], dev->packet[0]);
				got_picture = avcodec_receive_frame(dev->pCodecCtx[0], dev->pFrame[0]); //got_picture = 0 success, a frame was returned
				
	            if (ret < 0)
	            {
	                printf("%s","decode error\r\n");
	                continue;
	            }

	            //为0说明解码完成，非0正在解码
	            if (got_picture == 0)
	            {	
					//AVPixelFormat
					
						
	            /*	sws_scale(dev->sws_ctx[0],(const uint8_t **) dev->pFrame[0]->data, dev->pFrame[0]->linesize, 0, dev->pCodecCtx[0]->height,
                          dev->pFrameDecode[0]->data, dev->pFrameDecode[0]->linesize);*/
					gettimeofday(&decode_tv[0][1],NULL);
					delayTime = dev->frame_time[0]
								- ABS(decode_tv[0][1].tv_usec - decode_tv[0][0].tv_usec)
								- ABS(display_tv[0][1].tv_usec - display_tv[0][0].tv_usec);

					printf("1.delayTime = %d,,format=%d\r\n",decode_tv[0][1].tv_usec,dev->pFrame[0]->format);
					if(delayTime > 0)
					{
						//printf("0.delayTime = %d\r\n",delayTime);
						//usleep(delayTime);
					}	
					if(dev->outLayerAlpha.alpha[0] != 0)
					{
						sem_post(&display_sem[0]);
					}	
	            }
			}
        }		
        av_packet_unref(dev->packet[0]);		//av_packet_unref是清空里边的数据,不是释放
    }

	int value;
	while(sem_getvalue(&display_sem[0],&value) == 0)
	{
		if(stopAllTaskFlag == 1)	break;
		if(value == 0)	
		{
			break;
		}
	}
	sem_post(&display_sem[0]);
	printf("decode 0 end\r\n");
	return (void*)0;
}

void* task_decode_1(void* args)
{
	camera_info* dev = (camera_info*)args;
	int got_picture, ret;
	int delayTime;
	
	while (av_read_frame(dev->pFormatCtx[1], dev->packet[1]) >= 0)
    {
    	if(stopAllTaskFlag == 1)	break;
		
        if (dev->packet[1]->stream_index == dev->v_stream_idx[1])
        {
        	gettimeofday(&decode_tv[1][0],NULL);
			{
				ret = avcodec_send_packet(dev->pCodecCtx[1], dev->packet[1]);
				got_picture = avcodec_receive_frame(dev->pCodecCtx[1], dev->pFrame[1]); //got_picture = 0 success, a frame was returned
				
	            if (ret < 0)
	            {
	                printf("%s","decode error\r\n");
	                continue;
	            }

	            //为0说明解码完成，非0正在解码
	            if (got_picture == 0)
	            {
	                sws_scale(dev->sws_ctx[1],(const uint8_t **) dev->pFrame[1]->data, dev->pFrame[1]->linesize, 0, dev->pCodecCtx[1]->height,
	                          dev->pFrameDecode[1]->data, dev->pFrameDecode[1]->linesize);
					gettimeofday(&decode_tv[1][1],NULL);
					delayTime = dev->frame_time[1]
								- ABS(decode_tv[1][1].tv_usec - decode_tv[1][0].tv_usec)
								- ABS(display_tv[1][1].tv_usec - display_tv[1][0].tv_usec);
					if(delayTime > 0)
					{
						//printf("1.delayTime = %d\r\n",delayTime);
						usleep(delayTime);
					}	
					if(dev->outLayerAlpha.alpha[1] != 0)
					{
						sem_post(&display_sem[1]);
					}
	            }
			}
        }		
        av_packet_unref(dev->packet[1]);		//av_packet_unref是清空里边的数据,不是释放
    }
	int value;
	while(sem_getvalue(&display_sem[1],&value) == 0)
	{
		if(stopAllTaskFlag == 1)	break;
		if(value == 0)	
		{
			
			break;
		}
	}
	sem_post(&display_sem[1]);
	printf("decode 1 end\r\n");
	return (void*)0;
}

void* task_decode_2(void* args)
{

}

void* task_decode_3(void* args)
{

}

void* task_decode_4(void* args)
{

}


void* task_display_0(void* args)
{
	camera_info* dev = (camera_info*)args;

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
	if (res != 0)
	{
		printf("pthread_setaffinity_np faild\r\n");
	}
	
	while(1)
	{
		sem_wait(&display_sem[0]);
		if((pthread_kill(tid_decode[0],0) != 0) || (stopAllTaskFlag == 1))	break;
		pthread_mutex_lock(&sdl2_mutex);
		//sdl2_clear_frame(sdl2_dev);//要改，不然会闪烁
	/*	dev->imageFrameBuffer[0][0] = dev->pFrameDecode[0]->data[0];
		dev->imageFrameBuffer[0][1] = dev->pFrameDecode[0]->data[1];
		dev->imageFrameBuffer[0][2] = dev->pFrameDecode[0]->data[2];
		dev->oneLineByteSize[0][0] = dev->pFrameDecode[0]->linesize[0];
		dev->oneLineByteSize[0][1] = dev->pFrameDecode[0]->linesize[1];
		dev->oneLineByteSize[0][2] = dev->pFrameDecode[0]->linesize[2];*/

		dev->imageFrameBuffer[0][0] = dev->pFrame[0]->data[0];
		dev->imageFrameBuffer[0][1] = dev->pFrame[0]->data[1];
		dev->imageFrameBuffer[0][2] = dev->pFrame[0]->data[2];
		dev->oneLineByteSize[0][0] = dev->pFrame[0]->linesize[0];
		dev->oneLineByteSize[0][1] = dev->pFrame[0]->linesize[1];
		dev->oneLineByteSize[0][2] = dev->pFrame[0]->linesize[2];
	
		dev->layerInfo[0].crop.x = 0;
		dev->layerInfo[0].crop.y = 0;
		dev->layerInfo[0].crop.w = 1920;/*dev->imageSize_width[0]*/;
		dev->layerInfo[0].crop.h = 1080;/*dev->imageSize_height[0]*/;

		dev->layerInfo[0].scale.x = 0;
       	dev->layerInfo[0].scale.y = 0;
        dev->layerInfo[0].scale.w = 1920;/*dev->windowsSize_width*/;		
        dev->layerInfo[0].scale.h = 1080;/*dev->windowsSize_height*/;


		//uint8_t* p[1] = { data.get() };
		//int dst_stride[1] = { frame->width * 3 };
		//sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height, p, dst_stride);
		


		
		if(dev->outLayerAlpha.alphaFlag[0] == 0)
		{
			gettimeofday(&display_tv[0][0],NULL);
			
			printf("2.delayTime = %d\r\n",display_tv[0][0].tv_usec);
			sdl2_display_frame(dev,0); 
			sdl2_present_frame(dev);
			gettimeofday(&display_tv[0][1],NULL);
			printf("3.delayTime = %d\r\n",display_tv[0][1].tv_usec);
		}
		
		pthread_mutex_unlock(&sdl2_mutex);
	}
	printf("display 0 end\r\n");
	return (void*)0;
}

void* task_display_1(void* args)
{
	camera_info* dev = (camera_info*)args;
	while(1)
	{
		sem_wait(&display_sem[1]);
		if((pthread_kill(tid_decode[1],0) != 0) || (stopAllTaskFlag == 1))	break;
		pthread_mutex_lock(&sdl2_mutex);
		//sdl2_clear_frame(sdl2_dev);
		dev->imageFrameBuffer[1][0] = dev->pFrameDecode[1]->data[0];
		dev->imageFrameBuffer[1][1] = dev->pFrameDecode[1]->data[1];
		dev->imageFrameBuffer[1][2] = dev->pFrameDecode[1]->data[2];
		dev->oneLineByteSize[1][0] = dev->pFrameDecode[1]->linesize[0];
		dev->oneLineByteSize[1][1] = dev->pFrameDecode[1]->linesize[1];
		dev->oneLineByteSize[1][2] = dev->pFrameDecode[1]->linesize[2];
		
		dev->layerInfo[1].crop.x = 0;
		dev->layerInfo[1].crop.y = 0;
		dev->layerInfo[1].crop.w = dev->imageSize_width[1];
		dev->layerInfo[1].crop.h = dev->imageSize_height[1];

		dev->layerInfo[1].scale.x = 0;//sdl2_dev->windowsSize_width/2;
		dev->layerInfo[1].scale.y = 0;
		dev->layerInfo[1].scale.w = dev->windowsSize_width; 	
		dev->layerInfo[1].scale.h = dev->windowsSize_height;
		if(dev->outLayerAlpha.alphaFlag[1] == 0)
		{
			gettimeofday(&display_tv[1][0],NULL);
			sdl2_display_frame(dev,1); 
			sdl2_present_frame(dev);	
			gettimeofday(&display_tv[1][1],NULL);
		}
		
		pthread_mutex_unlock(&sdl2_mutex);
	}
	printf("display 1 end\r\n");
	return (void*)0;
}

void* task_display_2(void* args)
{

}

void* task_display_3(void* args)
{

}

void* task_display_4(void* args)
{

}

static int init_device(camera_info* dev)
{
	int m;
	avdevice_register_all();
	for(m=0;m<dev->inputSourceNum;m++)
	{
		dev->components[m] = 3;
		dev->v_stream_idx[m] = -1;
		dev->pixformat[m] = SDL_PIXELFORMAT_YUY2;

		const char* url = "/dev/video10";
	 
		AVInputFormat* input_fmt = av_find_input_format("video4linux2");
	

		if(input_fmt == NULL) printf("find av_find_input_format faild\r\n");
		
		dev->pFormatCtx[m] = avformat_alloc_context();
		AVDictionary *options = NULL;
	

		av_dict_set(&options, "fflags", "nobuffer", 0);
	    av_dict_set(&options, "max_delay", "100000", 0);
	    av_dict_set(&options, "framerate", "30", 0);
	    av_dict_set(&options, "input_format", "mjpeg", 0);
	    av_dict_set(&options, "video_size", "1920x1080", 0);
		
		//2.打开输入视频文件
	    if (avformat_open_input(&dev->pFormatCtx[m], url, input_fmt, &options) != 0)
	    {
	        printf("%s","open input file faild\r\n");
	        return 1;
	    }

		 //3.获取视频文件信息
	    if (avformat_find_stream_info(dev->pFormatCtx[m],NULL) < 0)
	    {
	        printf("%s","get video info faild\r\n");
	        return 1;
	    }

		//获取视频流的索引位置
	    //遍历所有类型的流（音频流、视频流、字幕流），找到视频流的ID
	    
	    int i = 0;
	    //number of streams
	    for (; i < dev->pFormatCtx[m]->nb_streams; i++)
	    {
	        //流的类型
	        if (dev->pFormatCtx[m]->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
	        {
	            dev->v_stream_idx[m] = i;
	            break;
	        }
	    }

	    if (dev->v_stream_idx[m] == -1)
	    {
	        printf("%s","can't find video stream\r\n");
	        return 1;
	    }



		
	    dev->pCodecCtx[m] = avcodec_alloc_context3(NULL);
		if (dev->pCodecCtx[m] == NULL) {
			printf("%s","can't alloc pCodecCtx\r\n");
			return 1;
		}
		avcodec_parameters_to_context(dev->pCodecCtx[m], dev->pFormatCtx[m]->streams[dev->v_stream_idx[m]]->codecpar);	//新版需要这么转化
	

		//av_opt_set(dev->pCodecCtx[m]->priv_data,"preset","ultrafast",0);
	    //4.根据编解码上下文中的编码id查找对应的解码
	    
	    dev->pCodec[m] = avcodec_find_decoder(dev->pCodecCtx[m]->codec_id);
	    if (dev->pCodec[m] == NULL)
	    {
	        printf("%s","can't find codec\r\n");
	        return 1;
	    }

		

	    //5.打开解码器
	    if (avcodec_open2(dev->pCodecCtx[m],dev->pCodec[m],NULL)<0)
	    {
	        printf("%s","can't open codec\r\n");
	        return 1;
	    }

		dev->imageSize_width[m] = dev->pCodecCtx[m]->width;
		dev->imageSize_height[m] = dev->pCodecCtx[m]->height;
		//sdl2_dev->oneLineByteSize[m] = sdl2_dev->pCodecCtx[m]->width * sdl2_dev->components[m];
		
		if((dev->pFormatCtx[m]->streams[dev->v_stream_idx[m]]->avg_frame_rate.num != 0) && (dev->pFormatCtx[m]->streams[dev->v_stream_idx[m]]->avg_frame_rate.den != 0))
		{
			dev->frame_rate[m] = (double)dev->pFormatCtx[m]->streams[dev->v_stream_idx[m]]->avg_frame_rate.num/dev->pFormatCtx[m]->streams[dev->v_stream_idx[m]]->avg_frame_rate.den;
			dev->frame_time[m] = (int)(1000000/dev->frame_rate[m]);
		}

		//输出视频信息
		printf("[%d]video codec id: %d\r\n",m,dev->pCodecCtx[m]->codec_id);
		printf("[%d]video format：%s\r\n",m,dev->pFormatCtx[m]->iformat->name);
		printf("[%d]video time：%d\r\n", m,(int)(dev->pFormatCtx[m]->duration)/1000000);
		printf("[%d]video sizeX=%d,sizeY=%d\r\n",m,dev->pCodecCtx[m]->width,dev->pCodecCtx[m]->height);
		printf("[%d]video fps=%4.2f\r\n",m,dev->frame_rate[m]);
		printf("[%d]video codec name：%s\r\n",m,dev->pCodec[m]->name);


		//准备读取
	    //AVPacket用于存储一帧一帧的压缩数据(解封装后的数据)
	    //缓冲区，开辟空间
	    dev->packet[m] = (AVPacket*)av_malloc(sizeof(AVPacket));

	    //AVFrame用于存储解码后的像素数据(YUV)
	    //内存分配
	    dev->pFrame[m] = av_frame_alloc();//用于存储一帧解码后，原始像素格式的数据
	    //RGB888	
	    dev->pFrameDecode[m] = av_frame_alloc();//用于存储一帧解码后，由原始像素格式转换成rgb后的数据


		
	    //只有指定了AVFrame的像素格式、画面大小才能真正分配内存
	    //缓冲区分配内存
	    dev->out_buffer[m] = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUVJ422P, dev->pCodecCtx[m]->width, dev->pCodecCtx[m]->height,1));
	    //初始化缓冲区
	    #if 1
	    av_image_fill_arrays(dev->pFrameDecode[m]->data,dev->pFrameDecode[m]->linesize,dev->out_buffer[m],AV_PIX_FMT_YUVJ422P,dev->pCodecCtx[m]->width, dev->pCodecCtx[m]->height,1);
		

		dev->sws_ctx[m] = sws_getContext(dev->pCodecCtx[m]->width,dev->pCodecCtx[m]->height,AV_PIX_FMT_PAL8/*dev->pCodecCtx[m]->pix_fmt*/,
                                dev->pCodecCtx[m]->width, dev->pCodecCtx[m]->height, AV_PIX_FMT_BGRA/*AV_PIX_FMT_YUV420P*/,
                                0/*SWS_BICUBIC*/, NULL, NULL, NULL);
		#endif
	}
	init_sdl2_display(dev);
	return 0;
}

void Stop(int signo) 
{
    printf("stop!!!\n");
    _exit(0);
}


int main(int argc, char *argv[])
{
	int m;
	
	camera_info* dev;
/*
	if (argc < 2) 
	{
		printf("insuffient auguments");
		exit(-1);
	}*/

	dev = (camera_info*)malloc(sizeof(camera_info));
	
	memset(dev,0,sizeof(camera_info));

	

	for (;;) 
    {  
        int index;  
        int c;  
            
        c = getopt_long (argc, argv,short_options, long_options,&index);  

        if (-1 == c)  
                break;  

        switch (c) 
		{  
	        case 0: /* getopt_long() flag */  
	                break;  

	        case 'i':  
					//dev->inputFile[dev->inputSourceNum] = (char**)malloc(100);
					strcpy((char*)dev->inputFile[dev->inputSourceNum],optarg);
					dev->inputSourceNum++;
	                break;  
	        case 'x':  
					dev->windowsSize_width = atoi(optarg);
					break;
	        case 'y':  
	                dev->windowsSize_height = atoi(optarg);
	                break; 
			case 'l':  
					dev->outputChNum = atoi(optarg);
	                break;  
			case 'a':
					dev->alphaTime = atoi(optarg);
					break;
	        default:  
	                printf("invalid config argc %s\r\n",optarg);  
	                exit(-1); 
	    }  
    }  
	signal(SIGINT, Stop); 

/*	if(dev->inputSourceNum == 0)
	{
		printf("no input file\r\n");
		exit(-1);
	}*/
	dev->inputSourceNum =1 ;

	if((dev->windowsSize_width == 0) && (dev->windowsSize_height == 0))
	{
		dev->windowsSize_width = 1920;
		dev->windowsSize_height = 1080;
	}

	
	if(dev->outputChNum == 0)
	{
		dev->outputChNum = dev->inputSourceNum;
	}

	if(dev->alphaTime == 0)
	{
		dev->alphaTime = 1000;
	}

	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//用只读和非阻塞的方式打开文件dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }

	
	for(m=0;m<dev->outputChNum;m++)
	{
		dev->layerInfo[m].inputPort = m;
		dev->outLayerAlpha.alpha[m] = 0xFF;
	}
	dev->outLayerAlpha.alpha[1] = 0;
	
	dev->windowsName = "play camera";

	if(init_device(dev))
	{
		printf("init sdl2 display faild\r\n");
		exit(-1);
	}



	

	_pthreadFunc  pthread_decode[MAX_CH_NUM] = {task_decode_0,task_decode_1};
	_pthreadFunc  pthread_display[MAX_CH_NUM] = {task_display_0,task_display_1};
	
	pthread_create(&tid_alpha,NULL,task_alpha,dev);//创建一个线程，用以淡入淡出
	pthread_create(&tid_command,NULL,task_command,dev);//创建一个线程，用以读取终端信息
	pthread_create(&tid_clear,NULL,task_clear,dev);//创建一个线程，用以清屏显示

	for(m=0;m<dev->inputSourceNum;m++)
	{
		sem_init(&display_sem[m], 0, 0);
		pthread_create(&tid_decode[m],NULL,pthread_decode[m],dev);
	}

	for(m=0;m<dev->outputChNum;m++)
	{
		pthread_create(&tid_display[m],NULL,pthread_display[m],dev);
	}

	for(m=0;m<dev->inputSourceNum;m++)
	{
		pthread_join(tid_decode[m],NULL);
	}

	for(m=0;m<dev->outputChNum;m++)
	{
		pthread_join(tid_display[m],NULL);
	}

	for(m=0;m<dev->inputSourceNum;m++)
	{
		sem_destroy(&display_sem[m]);
	}

	
FREE_MEM:
	for(m=0;m<dev->inputSourceNum;m++)
	{
		sws_freeContext(dev->sws_ctx[m]);
		av_free(dev->out_buffer[m]);
		av_frame_free(&dev->pFrameDecode[m]);
		av_frame_free(&dev->pFrame[m]);
		av_packet_free(&dev->packet[m]);		//这才是释放
		avcodec_close(dev->pCodecCtx[m]);
		avcodec_free_context(&dev->pCodecCtx[m]);
	    avformat_close_input(&dev->pFormatCtx[m]);
	    avformat_free_context(dev->pFormatCtx[m]);
	}

	free(dev);	
	printf("free mem\r\n");
	return 0;
}




#if 0

#include "funset.hpp"
#include <stdio.h>
#include <iostream>
#include <memory>
#include <fstream>
 
#ifdef __cplusplus
extern "C" {
#endif
 
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>
#include <libavutil/imgutils.h>
 
#ifdef __cplusplus
}
#endif
 
#include <opencv2/opencv.hpp>
 
int test_ffmpeg_decode_show_old()
{
	avdevice_register_all();
 
	const char* input_format_name = "video4linux2";
	const char* url = "/dev/video0";

 
	AVInputFormat* input_fmt = av_find_input_format(input_format_name);
	AVFormatContext* format_ctx = avformat_alloc_context();
	int ret = avformat_open_input(&format_ctx, url, input_fmt, nullptr);

	
	if (ret != 0) {
		fprintf(stderr, "fail to open url: %s, return value: %d\n", url, ret);
		return -1;
	}
 
	ret = avformat_find_stream_info(format_ctx, nullptr);
	if (ret < 0) {
		fprintf(stderr, "fail to get stream information: %d\n", ret);
		return -1;
	}
 
	int video_stream_index = -1;
	for (int i = 0; i < format_ctx->nb_streams; ++i) {
		const AVStream* stream = format_ctx->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream_index = i;
			fprintf(stdout, "type of the encoded data: %d, dimensions of the video frame in pixels: width: %d, height: %d, pixel format: %d\n",
				stream->codecpar->codec_id, stream->codecpar->width, stream->codecpar->height, stream->codecpar->format);
		}
	}
 
	if (video_stream_index == -1) {
		fprintf(stderr, "no video stream\n");
		return -1;
	}
 
	AVCodecContext* codec_ctx = format_ctx->streams[video_stream_index]->codec;
	AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
	if (!codec) {
		fprintf(stderr, "no decoder was found\n");
		return -1;
	}
 
	ret = avcodec_open2(codec_ctx, codec, nullptr);
	if (ret != 0) {
		fprintf(stderr, "fail to init AVCodecContext: %d\n", ret);
		return -1;
	}
 
	AVFrame* frame = av_frame_alloc();
	AVPacket* packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	SwsContext* sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, AV_PIX_FMT_BGR24, 0, nullptr, nullptr, nullptr);
	if (!frame || !packet || !sws_ctx) {
		fprintf(stderr, "fail to alloc\n");
		return -1;
	}
 
	int got_picture = -1;
	std::unique_ptr<uint8_t[]> data(new uint8_t[codec_ctx->width * codec_ctx->height * 3]);
	cv::Mat mat(codec_ctx->height, codec_ctx->width, CV_8UC3);
	int width_new = 320, height_new = 240;
	cv::Mat dst(height_new, width_new, CV_8UC3);
	const char* winname = "usb video1";
	cv::namedWindow(winname);
 
	while (1) {
		ret = av_read_frame(format_ctx, packet);
		if (ret < 0) {
			fprintf(stderr, "fail to av_read_frame: %d\n", ret);
			continue;
		}
 
		if (packet->stream_index == video_stream_index) {
			ret = avcodec_decode_video2(codec_ctx, frame, &got_picture, packet);
			if (ret < 0) {
				fprintf(stderr, "fail to avcodec_decode_video2: %d\n", ret);
				av_free_packet(packet);
				continue;
			}
 
			if (got_picture) {
				uint8_t* p[1] = { data.get() };
				int dst_stride[1] = { frame->width * 3 };
				sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height, p, dst_stride);
				mat.data = data.get();
				cv::resize(mat, dst, cv::Size(width_new, height_new));
				cv::imshow(winname, dst);
			}
		}
 
		av_free_packet(packet);
 
		int key = cv::waitKey(25);
		if (key == 27) break;
	}
 
	cv::destroyWindow(winname);
	av_frame_free(&frame);
	sws_freeContext(sws_ctx);
	av_free(packet);
	avformat_close_input(&format_ctx);
 
	fprintf(stdout, "test finish\n");
	return 0;
}

#endif

