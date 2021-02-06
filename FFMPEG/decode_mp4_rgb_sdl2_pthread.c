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
#include <semaphore.h>
#include <signal.h>


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


void* task_command(void* args)
{
	int inputcharNum;
	char buf_tty[10];
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	while(1)
	{
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
				OutLayerSetting* outLayerAlpha = &sdl2_dev->outLayerAlpha;
				sdl2_GetAlpha(sdl2_dev,0,&outLayerAlpha->alpha[0]);
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
				OutLayerSetting* outLayerAlpha = &sdl2_dev->outLayerAlpha;
				if((layerCh>=1) && (layerCh<=sdl2_dev->outputChNum))
				{
					ret = sdl2_GetAlpha(sdl2_dev,layerCh-1,&outLayerAlpha->alpha[layerCh-1]);
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
		usleep(500000);
	}
	return (void*) 0;
}

void* task_clear(void* args)
{
	int layerCh,clearFlag=0;
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	int delayTime = (sdl2_dev->alphaTime*1000)/255;
	while(1)
	{
		if(stopAllTaskFlag == 1)	break;
		clearFlag = 0;
		for(layerCh=0;layerCh<sdl2_dev->outputChNum;layerCh++)
		{
			if(sdl2_dev->outLayerAlpha.alphaFlag[layerCh] != 0)
			{
				clearFlag = 1;
				break;
			}
		}

		if(clearFlag)
		{
			pthread_mutex_lock(&sdl2_mutex);
			sdl2_clear_frame(sdl2_dev);
			sdl2_display_frame(sdl2_dev,0);	
			sdl2_display_frame(sdl2_dev,1);	
			sdl2_present_frame(sdl2_dev);
			pthread_mutex_unlock(&sdl2_mutex);
			usleep(1);
		}
		else
		{
			usleep(delayTime);
		}
	}
	return (void*)0;
}


void* task_alpha(void* args)
{
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	int delayTime = (sdl2_dev->alphaTime*1000)/255;
	int layerCh;
	while(1)
	{
		if(stopAllTaskFlag == 1)	break;
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
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	int got_picture, ret;
	
	while (av_read_frame(sdl2_dev->pFormatCtx[0], sdl2_dev->packet[0]) >= 0)
    {
    	if(stopAllTaskFlag == 1)	break;
        if (sdl2_dev->packet[0]->stream_index == sdl2_dev->v_stream_idx[0])
        {
			ret = avcodec_send_packet(sdl2_dev->pCodecCtx[0], sdl2_dev->packet[0]);
			got_picture = avcodec_receive_frame(sdl2_dev->pCodecCtx[0], sdl2_dev->pFrame[0]); //got_picture = 0 success, a frame was returned
			
            if (ret < 0)
            {
                printf("%s","decode error\r\n");
                continue;
            }

            //为0说明解码完成，非0正在解码
            if (got_picture == 0)
            {
                sws_scale(sdl2_dev->sws_ctx[0],(const uint8_t **) sdl2_dev->pFrame[0]->data, sdl2_dev->pFrame[0]->linesize, 0, sdl2_dev->pCodecCtx[0]->height,
                          sdl2_dev->pFrameRGB[0]->data, sdl2_dev->pFrameRGB[0]->linesize);
				usleep(33000);
				sem_post(&display_sem[0]);
            }
        }		
        av_packet_unref(sdl2_dev->packet[0]);		//av_packet_unref是清空里边的数据,不是释放
    }

	int value;
	while(sem_getvalue(&display_sem[0],&value) == 0)
	{
		if(stopAllTaskFlag == 1)	break;
		if(value == 0)	break;
	}

	printf("decode 0 end\r\n");
	return (void*)0;
}

void* task_decode_1(void* args)
{
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	int got_picture, ret;
	
	while (av_read_frame(sdl2_dev->pFormatCtx[1], sdl2_dev->packet[1]) >= 0)
    {
    	if(stopAllTaskFlag == 1)	break;
        if (sdl2_dev->packet[1]->stream_index == sdl2_dev->v_stream_idx[1])
        {
			ret = avcodec_send_packet(sdl2_dev->pCodecCtx[1], sdl2_dev->packet[1]);
			got_picture = avcodec_receive_frame(sdl2_dev->pCodecCtx[1], sdl2_dev->pFrame[1]); //got_picture = 0 success, a frame was returned
			
            if (ret < 0)
            {
                printf("%s","decode error\r\n");
                continue;
            }

            //为0说明解码完成，非0正在解码
            if (got_picture == 0)
            {
                sws_scale(sdl2_dev->sws_ctx[1],(const uint8_t **) sdl2_dev->pFrame[1]->data, sdl2_dev->pFrame[1]->linesize, 0, sdl2_dev->pCodecCtx[1]->height,
                          sdl2_dev->pFrameRGB[1]->data, sdl2_dev->pFrameRGB[1]->linesize);
				usleep(33000);
				sem_post(&display_sem[1]);
				
            }
        }		
        av_packet_unref(sdl2_dev->packet[1]);		//av_packet_unref是清空里边的数据,不是释放
    }
	int value;
	while(sem_getvalue(&display_sem[1],&value) == 0)
	{
		if(stopAllTaskFlag == 1)	break;
		if(value == 0)	break;
	}
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
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	while(1)
	{
		if((pthread_kill(tid_decode[0],0) != 0) || (stopAllTaskFlag == 1))	break;
		sem_wait(&display_sem[0]);
		pthread_mutex_lock(&sdl2_mutex);
		//sdl2_clear_frame(sdl2_dev);//要改，不然会闪烁
		sdl2_dev->imageFrameBuffer[0] = sdl2_dev->pFrameRGB[0]->data[0];
		sdl2_dev->layerInfo[0].crop.x = 0;
		sdl2_dev->layerInfo[0].crop.y = 0;
		sdl2_dev->layerInfo[0].crop.w = sdl2_dev->imageSize_width[0];
		sdl2_dev->layerInfo[0].crop.h = sdl2_dev->imageSize_height[0];

		sdl2_dev->layerInfo[0].scale.x = 0;
       	sdl2_dev->layerInfo[0].scale.y = 0;
        sdl2_dev->layerInfo[0].scale.w = sdl2_dev->windowsSize_width;		
        sdl2_dev->layerInfo[0].scale.h = sdl2_dev->windowsSize_height;
		if(sdl2_dev->outLayerAlpha.alphaFlag[0] == 0)
		{
			sdl2_display_frame(sdl2_dev,0);	
			sdl2_present_frame(sdl2_dev);
		}
		
		pthread_mutex_unlock(&sdl2_mutex);
	}
	return (void*)0;
}

void* task_display_1(void* args)
{
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	while(1)
	{
		if((pthread_kill(tid_decode[1],0) != 0) || (stopAllTaskFlag == 1))	break;
		sem_wait(&display_sem[1]);
		pthread_mutex_lock(&sdl2_mutex);
		//sdl2_clear_frame(sdl2_dev);
		sdl2_dev->imageFrameBuffer[1] = sdl2_dev->pFrameRGB[1]->data[0];
		sdl2_dev->layerInfo[1].crop.x = 0;
		sdl2_dev->layerInfo[1].crop.y = 0;
		sdl2_dev->layerInfo[1].crop.w = sdl2_dev->imageSize_width[1];
		sdl2_dev->layerInfo[1].crop.h = sdl2_dev->imageSize_height[1];

		sdl2_dev->layerInfo[1].scale.x = 0;//sdl2_dev->windowsSize_width/2;
		sdl2_dev->layerInfo[1].scale.y = 0;
		sdl2_dev->layerInfo[1].scale.w = sdl2_dev->windowsSize_width; 	
		sdl2_dev->layerInfo[1].scale.h = sdl2_dev->windowsSize_height;
		if(sdl2_dev->outLayerAlpha.alphaFlag[1] == 0)
		{
			sdl2_display_frame(sdl2_dev,1); 
			sdl2_present_frame(sdl2_dev);	
		}
		
		pthread_mutex_unlock(&sdl2_mutex);
	}
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





int main(int argc, char *argv[])
{
	int m;
	char** inputFile[MAX_CH_NUM];
	sdl2_display_info* sdl2_dev;

	if (argc < 2) 
	{
		printf("insuffient auguments");
		exit(-1);
	}

	sdl2_dev = malloc(sizeof(sdl2_display_info));
	
	memset(sdl2_dev,0,sizeof(sdl2_dev));

	

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
					inputFile[sdl2_dev->inputSourceNum] = malloc(100);
					strcpy((char*)inputFile[sdl2_dev->inputSourceNum],optarg);
					sdl2_dev->inputSourceNum++;
	                break;  
	        case 'x':  
					sdl2_dev->windowsSize_width = atoi(optarg);
					break;
	        case 'y':  
	                sdl2_dev->windowsSize_height = atoi(optarg);
	                break; 
			case 'l':  
					sdl2_dev->outputChNum = atoi(optarg);
	                break;  
			case 'a':
					sdl2_dev->alphaTime = atoi(optarg);
					break;
	        default:  
	                printf("invalid config argc %s\r\n",optarg);  
	                exit(-1); 
	    }  
    }  

	if(sdl2_dev->inputSourceNum == 0)
	{
		printf("no input file\r\n");
		exit(-1);
	}

	if((sdl2_dev->windowsSize_width == 0) && (sdl2_dev->windowsSize_height == 0))
	{
		sdl2_dev->windowsSize_width = 960;
		sdl2_dev->windowsSize_height = 540;
	}

	
	if(sdl2_dev->outputChNum == 0)
	{
		sdl2_dev->outputChNum = sdl2_dev->inputSourceNum;
	}

	if(sdl2_dev->alphaTime == 0)
	{
		sdl2_dev->alphaTime = 1000;
	}

	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//用只读和非阻塞的方式打开文件dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }
	
	sdl2_dev->windowsName = "play mp4";
	
    //1.注册所有组件
  //  av_register_all();

	for(m=0;m<sdl2_dev->inputSourceNum;m++)
	{
		sdl2_dev->pixformat[m] = SDL_PIXELFORMAT_RGB24;
		sdl2_dev->components[m] = 3;
		sdl2_dev->v_stream_idx[m] = -1;
		sdl2_dev->pFormatCtx[m] = avformat_alloc_context();
		//2.打开输入视频文件
	    if (avformat_open_input(&sdl2_dev->pFormatCtx[m], (char*)inputFile[m], NULL, NULL) != 0)
	    {
	        printf("%s","open input file faild\r\n");
	        exit(-1);
	    }

		 //3.获取视频文件信息
	    if (avformat_find_stream_info(sdl2_dev->pFormatCtx[m],NULL) < 0)
	    {
	        printf("%s","get video info faild\r\n");
	        exit(-1);
	    }

		//获取视频流的索引位置
	    //遍历所有类型的流（音频流、视频流、字幕流），找到视频流的ID
	    
	    int i = 0;
	    //number of streams
	    for (; i < sdl2_dev->pFormatCtx[m]->nb_streams; i++)
	    {
	        //流的类型
	        if (sdl2_dev->pFormatCtx[m]->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
	        {
	            sdl2_dev->v_stream_idx[m] = i;
	            break;
	        }
	    }

	    if (sdl2_dev->v_stream_idx[m] == -1)
	    {
	        printf("%s","can't find video stream\r\n");
	        exit(-1);
	    }



		
	    sdl2_dev->pCodecCtx[m] = avcodec_alloc_context3(NULL);
		if (sdl2_dev->pCodecCtx[m] == NULL) {
			printf("%s","can't alloc pCodecCtx\r\n");
			exit(-1);
		}
		avcodec_parameters_to_context(sdl2_dev->pCodecCtx[m], sdl2_dev->pFormatCtx[m]->streams[sdl2_dev->v_stream_idx[m]]->codecpar);	//新版需要这么转化
		
	    //4.根据编解码上下文中的编码id查找对应的解码
	    sdl2_dev->pCodec[m] = avcodec_find_decoder(sdl2_dev->pCodecCtx[m]->codec_id);
	    if (sdl2_dev->pCodec[m] == NULL)
	    {
	        printf("%s","can't find codec\r\n");
	        exit(-1);
	    }

	    //5.打开解码器
	    if (avcodec_open2(sdl2_dev->pCodecCtx[m],sdl2_dev->pCodec[m],NULL)<0)
	    {
	        printf("%s","can't open codec\r\n");
	        exit(-1);
	    }

		sdl2_dev->imageSize_width[m] = sdl2_dev->pCodecCtx[m]->width;
		sdl2_dev->imageSize_height[m] = sdl2_dev->pCodecCtx[m]->height;
		sdl2_dev->oneLineByteSize[m] = sdl2_dev->pCodecCtx[m]->width * sdl2_dev->components[m];
		
		if((sdl2_dev->pFormatCtx[m]->streams[sdl2_dev->v_stream_idx[m]]->avg_frame_rate.num != 0) && (sdl2_dev->pFormatCtx[m]->streams[sdl2_dev->v_stream_idx[m]]->avg_frame_rate.den != 0))
		{
			sdl2_dev->frame_rate[m] = (double)sdl2_dev->pFormatCtx[m]->streams[sdl2_dev->v_stream_idx[m]]->avg_frame_rate.num/sdl2_dev->pFormatCtx[m]->streams[sdl2_dev->v_stream_idx[m]]->avg_frame_rate.den;
			sdl2_dev->frame_time[m] = (int)(1000000/sdl2_dev->frame_rate[m]);
		}

		//输出视频信息
		printf("[%d]video format：%s\r\n",m,sdl2_dev->pFormatCtx[m]->iformat->name);
		printf("[%d]video time：%d\r\n", m,(int)(sdl2_dev->pFormatCtx[m]->duration)/1000000);
		printf("[%d]video sizeX=%d,sizeY=%d\r\n",m,sdl2_dev->pCodecCtx[m]->width,sdl2_dev->pCodecCtx[m]->height);
		printf("[%d]video fps=%4.2f\r\n",m,sdl2_dev->frame_rate[m]);
		printf("[%d]video codec name：%s\r\n",m,sdl2_dev->pCodec[m]->name);


		//准备读取
	    //AVPacket用于存储一帧一帧的压缩数据(解封装后的数据)
	    //缓冲区，开辟空间
	    sdl2_dev->packet[m] = (AVPacket*)av_malloc(sizeof(AVPacket));

	    //AVFrame用于存储解码后的像素数据(YUV)
	    //内存分配
	    sdl2_dev->pFrame[m] = av_frame_alloc();//用于存储一帧解码后，原始像素格式的数据
	    //RGB888	
	    sdl2_dev->pFrameRGB[m] = av_frame_alloc();//用于存储一帧解码后，由原始像素格式转换成rgb后的数据


		
	    //只有指定了AVFrame的像素格式、画面大小才能真正分配内存
	    //缓冲区分配内存
	    sdl2_dev->out_buffer[m] = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, sdl2_dev->pCodecCtx[m]->width, sdl2_dev->pCodecCtx[m]->height,1));
	    //初始化缓冲区
	    av_image_fill_arrays(sdl2_dev->pFrameRGB[m]->data,sdl2_dev->pFrameRGB[m]->linesize,sdl2_dev->out_buffer[m],AV_PIX_FMT_RGB24,sdl2_dev->pCodecCtx[m]->width, sdl2_dev->pCodecCtx[m]->height,1);
	  
	    //用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
	    sdl2_dev->sws_ctx[m] = sws_getContext(sdl2_dev->pCodecCtx[m]->width,sdl2_dev->pCodecCtx[m]->height,sdl2_dev->pCodecCtx[m]->pix_fmt,
                                sdl2_dev->pCodecCtx[m]->width, sdl2_dev->pCodecCtx[m]->height, AV_PIX_FMT_RGB24,
                                SWS_BICUBIC, NULL, NULL, NULL);
	}
    int got_picture[sdl2_dev->inputSourceNum], ret[sdl2_dev->inputSourceNum];
	
	for(m=0;m<sdl2_dev->outputChNum;m++)
	{
		sdl2_dev->layerInfo[m].inputPort = m;
		sdl2_dev->outLayerAlpha.alpha[m] = 0xFF;
	}
	sdl2_dev->outLayerAlpha.alpha[1] = 0;
	init_sdl2_display(sdl2_dev);

	_pthreadFunc  pthread_decode[MAX_CH_NUM] = {task_decode_0,task_decode_1,task_decode_2,task_decode_3,task_decode_4};
	_pthreadFunc  pthread_display[MAX_CH_NUM] = {task_display_0,task_display_1,task_display_2,task_display_3,task_display_4};
	
	pthread_create(&tid_alpha,NULL,task_alpha,sdl2_dev);//创建一个线程，用以淡入淡出
	pthread_create(&tid_command,NULL,task_command,sdl2_dev);//创建一个线程，用以读取终端信息
	pthread_create(&tid_clear,NULL,task_clear,sdl2_dev);//创建一个线程，用以清屏显示

	for(m=0;m<sdl2_dev->inputSourceNum;m++)
	{
		sem_init(&display_sem[m], 0, 0);
		pthread_create(&tid_decode[m],NULL,pthread_decode[m],sdl2_dev);
	}

	for(m=0;m<sdl2_dev->outputChNum;m++)
	{
		pthread_create(&tid_display[m],NULL,pthread_display[m],sdl2_dev);
	}

	for(m=0;m<sdl2_dev->inputSourceNum;m++)
	{
		pthread_join(tid_decode[m],NULL);
	}

	for(m=0;m<sdl2_dev->outputChNum;m++)
	{
		pthread_join(tid_display[m],NULL);
	}

	for(m=0;m<sdl2_dev->inputSourceNum;m++)
	{
		sem_destroy(&display_sem[m]);
	}

	quit_sdl2_display(sdl2_dev);

	
FREE_MEM:
	for(m=0;m<sdl2_dev->inputSourceNum;m++)
	{
		sws_freeContext(sdl2_dev->sws_ctx[m]);
		av_free(sdl2_dev->out_buffer[m]);
		av_frame_free(&sdl2_dev->pFrameRGB[m]);
		av_frame_free(&sdl2_dev->pFrame[m]);
		av_packet_free(&sdl2_dev->packet[m]);		//这才是释放
		avcodec_close(sdl2_dev->pCodecCtx[m]);
		avcodec_free_context(&sdl2_dev->pCodecCtx[m]);
	    avformat_close_input(&sdl2_dev->pFormatCtx[m]);
	    avformat_free_context(sdl2_dev->pFormatCtx[m]);
	}

	free(sdl2_dev);	
	printf("free mem\r\n");
	return 0;
}









