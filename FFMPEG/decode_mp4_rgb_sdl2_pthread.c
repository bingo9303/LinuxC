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
#include "sdl2_display.h"
#include <getopt.h> 
#include <pthread.h>

#define ABS(x) ((x)<0?-(x):(x))


#define TEST_ENABLE	1

typedef void * (*_pthreadFunc) (void*);


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

  
void* task_alpha(void* args)
{
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	int delayTime = (sdl2_dev->alphaTime*1000)/255;
	int layerCh;
	while(1)
	{
		for(layerCh=0;layerCh<sdl2_dev->outputChNum;layerCh++)
		{
			if(sdl2_dev->outLayerAlpha.alphaFlag[layerCh] == 1)
			{
				sdl2_dev->outLayerAlpha.alpha[layerCh]++;
				sdl2_SetAlpha(sdl2_dev,layerCh,(sdl2_dev->outLayerAlpha.alpha[layerCh]&0xFF));
				if(sdl2_dev->outLayerAlpha.alpha[layerCh] >= 0xFF)	
					sdl2_dev->outLayerAlpha.alphaFlag[layerCh] = 0;
			}
			else if(sdl2_dev->outLayerAlpha.alphaFlag[layerCh] == 2)
			{
				sdl2_dev->outLayerAlpha.alpha[layerCh]--;
				sdl2_SetAlpha(sdl2_dev,layerCh,(sdl2_dev->outLayerAlpha.alpha[layerCh]&0xFF));
				if(sdl2_dev->outLayerAlpha.alpha[layerCh] <= 0)	
					sdl2_dev->outLayerAlpha.alphaFlag[layerCh] = 0;
			}
		}
		usleep(delayTime);
	}
}


void* task_decode_0(void* args)
{

}

void* task_decode_1(void* args)
{

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

}

void* task_display_1(void* args)
{

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





int main(int argc, char *argv[])
{
	int m;
    int fd_tty,inputcharNum=0;
	char buf_tty[10];
	double frame_rate[MAX_CH_NUM] = {0};
	char** inputFile[MAX_CH_NUM];
	sdl2_display_info sdl2_dev;
	memset(&sdl2_dev,0,sizeof(sdl2_dev));

	if (argc < 2) 
	{
		printf("insuffient auguments");
		exit(-1);
	}

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
					inputFile[sdl2_dev.inputSourceNum] = malloc(100);
					strcpy((char*)inputFile[sdl2_dev.inputSourceNum],optarg);
					sdl2_dev.inputSourceNum++;
	                break;  
	        case 'x':  
					sdl2_dev.windowsSize_width = atoi(optarg);
					break;
	        case 'y':  
	                sdl2_dev.windowsSize_height = atoi(optarg);
	                break; 
			case 'l':  
					sdl2_dev.outputChNum = atoi(optarg);
	                break;  
			case 'a':
					sdl2_dev.alphaTime = atoi(optarg);
					break;
	        default:  
	                printf("invalid config argc %s\r\n",optarg);  
	                exit (EXIT_FAILURE);  
	    }  
    }  

	if(sdl2_dev.inputSourceNum == 0)
	{
		printf("no input file\r\n");
		exit(-1);
	}

	if((sdl2_dev.windowsSize_width == 0) && (sdl2_dev.windowsSize_height == 0))
	{
		sdl2_dev.windowsSize_width = 960;
		sdl2_dev.windowsSize_height = 540;
	}

	
	if(sdl2_dev.outputChNum == 0)
	{
		sdl2_dev.outputChNum = sdl2_dev.inputSourceNum;
	}

	if(sdl2_dev.alphaTime == 0)
	{
		sdl2_dev.alphaTime = 1000;
	}

	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//用只读和非阻塞的方式打开文件dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }
	
	sdl2_dev.windowsName = "play mp4";
	
    //1.注册所有组件
  //  av_register_all();

    //封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
    AVFormatContext *pFormatCtx[sdl2_dev.inputSourceNum];
	AVCodecContext *pCodecCtx[sdl2_dev.inputSourceNum];
	AVCodec *pCodec[sdl2_dev.inputSourceNum];
	AVPacket *packet[sdl2_dev.inputSourceNum];
	AVFrame *pFrame[sdl2_dev.inputSourceNum];
	AVFrame *pFrameRGB[sdl2_dev.inputSourceNum];
	uint8_t *out_buffer[sdl2_dev.inputSourceNum];
	struct SwsContext *sws_ctx[sdl2_dev.inputSourceNum];
	int v_stream_idx[sdl2_dev.inputSourceNum];
	memset(v_stream_idx,-1,sizeof(v_stream_idx));
	for(m=0;m<sdl2_dev.inputSourceNum;m++)
	{
		sdl2_dev.pixformat[m] = SDL_PIXELFORMAT_RGB24;
		sdl2_dev.components[m] = 3;
		pFormatCtx[m] = avformat_alloc_context();
		//2.打开输入视频文件
	    if (avformat_open_input(&pFormatCtx[m], (char*)inputFile[m], NULL, NULL) != 0)
	    {
	        printf("%s","open input file faild\r\n");
	        return 1;
	    }

		 //3.获取视频文件信息
	    if (avformat_find_stream_info(pFormatCtx[m],NULL) < 0)
	    {
	        printf("%s","get video info faild\r\n");
	        return 1;
	    }

		//获取视频流的索引位置
	    //遍历所有类型的流（音频流、视频流、字幕流），找到视频流的ID
	    
	    int i = 0;
	    //number of streams
	    for (; i < pFormatCtx[m]->nb_streams; i++)
	    {
	        //流的类型
	        if (pFormatCtx[m]->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
	        {
	            v_stream_idx[m] = i;
	            break;
	        }
	    }

	    if (v_stream_idx[m] == -1)
	    {
	        printf("%s","can't find video stream\r\n");
	        return 1;
	    }



		
	    pCodecCtx[m] = avcodec_alloc_context3(NULL);
		if (pCodecCtx[m] == NULL) {
			printf("%s","can't alloc pCodecCtx\r\n");
			return 1;
		}
		avcodec_parameters_to_context(pCodecCtx[m], pFormatCtx[m]->streams[v_stream_idx[m]]->codecpar);	//新版需要这么转化
		
	    //4.根据编解码上下文中的编码id查找对应的解码
	    pCodec[m] = avcodec_find_decoder(pCodecCtx[m]->codec_id);
	    if (pCodec[m] == NULL)
	    {
	        printf("%s","can't find codec\r\n");
	        return 1;
	    }

	    //5.打开解码器
	    if (avcodec_open2(pCodecCtx[m],pCodec[m],NULL)<0)
	    {
	        printf("%s","can't open codec\r\n");
	        return 1;
	    }

		sdl2_dev.imageSize_width[m] = pCodecCtx[m]->width;
		sdl2_dev.imageSize_height[m] = pCodecCtx[m]->height;
		sdl2_dev.oneLineByteSize[m] = pCodecCtx[m]->width * sdl2_dev.components[m];
		
		if((pFormatCtx[m]->streams[v_stream_idx[m]]->avg_frame_rate.num != 0) && (pFormatCtx[m]->streams[v_stream_idx[m]]->avg_frame_rate.den != 0))
		{
			sdl2_dev.frame_rate[m] = (double)pFormatCtx[m]->streams[v_stream_idx[m]]->avg_frame_rate.num/pFormatCtx[m]->streams[v_stream_idx[m]]->avg_frame_rate.den;
			sdl2_dev.frame_time[m] = (int)(1000000/sdl2_dev.frame_rate[m]);
		}

		//输出视频信息
		printf("[%d]video format：%s\r\n",m,pFormatCtx[m]->iformat->name);
		printf("[%d]video time：%d\r\n", m,(int)(pFormatCtx[m]->duration)/1000000);
		printf("[%d]video sizeX=%d,sizeY=%d\r\n",m,pCodecCtx[m]->width,pCodecCtx[m]->height);
		printf("[%d]video fps=%4.2f\r\n",m,frame_rate[m]);
		printf("[%d]video codec name：%s\r\n",m,pCodec[m]->name);


		//准备读取
	    //AVPacket用于存储一帧一帧的压缩数据(解封装后的数据)
	    //缓冲区，开辟空间
	    packet[m] = (AVPacket*)av_malloc(sizeof(AVPacket));

	    //AVFrame用于存储解码后的像素数据(YUV)
	    //内存分配
	    pFrame[m] = av_frame_alloc();//用于存储一帧解码后，原始像素格式的数据
	    //RGB888	
	    pFrameRGB[m] = av_frame_alloc();//用于存储一帧解码后，由原始像素格式转换成rgb后的数据


		
	    //只有指定了AVFrame的像素格式、画面大小才能真正分配内存
	    //缓冲区分配内存
	    out_buffer[m] = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx[m]->width, pCodecCtx[m]->height,1));
	    //初始化缓冲区
	    av_image_fill_arrays(pFrameRGB[m]->data,pFrameRGB[m]->linesize,out_buffer[m],AV_PIX_FMT_RGB24,pCodecCtx[m]->width, pCodecCtx[m]->height,1);
	  
	    //用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
	    sws_ctx[m] = sws_getContext(pCodecCtx[m]->width,pCodecCtx[m]->height,pCodecCtx[m]->pix_fmt,
                                pCodecCtx[m]->width, pCodecCtx[m]->height, AV_PIX_FMT_RGB24,
                                SWS_BICUBIC, NULL, NULL, NULL);
	}
    int got_picture[sdl2_dev.inputSourceNum], ret[sdl2_dev.inputSourceNum];
	
	for(m=0;m<sdl2_dev.outputChNum;m++)
	{
		sdl2_dev.layerInfo[m].inputPort = m;
		sdl2_dev.outLayerAlpha.alpha[m] = 0xFF;
	}
	init_sdl2_display(&sdl2_dev);

	_pthreadFunc  pthread_decode[MAX_CH_NUM] = {task_decode_0,task_decode_1,task_decode_2,task_decode_3,task_decode_4};
	_pthreadFunc  pthread_display[MAX_CH_NUM] = {task_display_0,task_display_1,task_display_2,task_display_3,task_display_4};
	pthread_t tid_alpha;
	pthread_t tid_decode[sdl2_dev.inputSourceNum];
	pthread_t tid_display[sdl2_dev.outputChNum];
	
	pthread_create(&tid_alpha,NULL,task_alpha,&sdl2_dev);//创建一个线程，用以淡入淡出

	for(m=0;m<sdl2_dev.inputSourceNum;m++)
	{
		pthread_create(&tid_decode[m],NULL,pthread_decode[m],&sdl2_dev);
	}

	for(m=0;m<sdl2_dev.outputChNum;m++)
	{
		pthread_create(&tid_display[m],NULL,pthread_display[m],&sdl2_dev);
	}

	for(m=0;m<sdl2_dev.inputSourceNum;m++)
	{
		pthread_join(tid_decode[m],NULL);
	}

	for(m=0;m<sdl2_dev.outputChNum;m++)
	{
		pthread_join(tid_display[m],NULL);
	}
	

	quit_sdl2_display(&sdl2_dev);

	for(m=0;m<sdl2_dev.inputSourceNum;m++)
	{
		sws_freeContext(sws_ctx[m]);
		av_free(out_buffer[m]);
		av_frame_free(&pFrameRGB[m]);
		av_frame_free(&pFrame[m]);
		av_packet_free(&packet[m]);		//这才是释放
		avcodec_close(pCodecCtx[m]);
		avcodec_free_context(&pCodecCtx[m]);
	    avformat_close_input(&pFormatCtx[m]);
	    avformat_free_context(pFormatCtx[m]);
	}
	
	printf("free mem\r\n");
	return 0;
}









