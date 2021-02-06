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



static const char short_options [] = "i:x:y:l:a:";  
  
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



int main(int argc, char *argv[])
{
    int fd_tty,inputcharNum=0;
	char buf_tty[10];
	struct timeval tv[2];
	int delayTime;
	char inputFile[100] = {0};

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
					strcpy(inputFile,optarg);
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
	                exit (EXIT_FAILURE);  
	    }  
    }  


	if(inputFile[0] == 0)
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
		sdl2_dev->outputChNum = 4;
	}

	if(sdl2_dev->alphaTime == 0)
	{
		sdl2_dev->alphaTime = 1000;
	}

	sdl2_dev->inputSourceNum = 1;

	
	
	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//用只读和非阻塞的方式打开文件dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }


	sdl2_dev->pixformat[0] = SDL_PIXELFORMAT_RGB24;
	sdl2_dev->components[0] = 3;
	sdl2_dev->windowsName = "play mp4";
	
    //1.注册所有组件
  //  av_register_all();

    //封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
    sdl2_dev->pFormatCtx[0] = avformat_alloc_context();

    //2.打开输入视频文件
    if (avformat_open_input(&sdl2_dev->pFormatCtx[0], inputFile, NULL, NULL) != 0)
    {
        printf("%s","open input file faild\r\n");
        return 1;
    }

    //3.获取视频文件信息
    if (avformat_find_stream_info(sdl2_dev->pFormatCtx[0],NULL) < 0)
    {
        printf("%s","get video info faild\r\n");
        return 1;
    }
    //获取视频流的索引位置
    //遍历所有类型的流（音频流、视频流、字幕流），找到视频流的ID
    sdl2_dev->v_stream_idx[0] = -1;
    int i = 0;
    //number of streams
    for (; i < sdl2_dev->pFormatCtx[0]->nb_streams; i++)
    {
        //流的类型
        if (sdl2_dev->pFormatCtx[0]->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            sdl2_dev->v_stream_idx[0] = i;
            break;
        }
    }

    if (sdl2_dev->v_stream_idx[0] == -1)
    {
        printf("%s","can't find video stream\r\n");
        return 1;
    }

    //只有知道视频的编码方式，才能够根据编码方式去找到解码器
    //获取视频流中的编解码上下文
    sdl2_dev->pCodecCtx[0] = avcodec_alloc_context3(NULL);
	if (sdl2_dev->pCodecCtx[0] == NULL) {
		printf("%s","can't alloc pCodecCtx\r\n");
		return 1;
	}
	avcodec_parameters_to_context(sdl2_dev->pCodecCtx[0], sdl2_dev->pFormatCtx[0]->streams[sdl2_dev->v_stream_idx[0]]->codecpar);	//新版需要这么转化
	
    //4.根据编解码上下文中的编码id查找对应的解码
    sdl2_dev->pCodec[0] = avcodec_find_decoder(sdl2_dev->pCodecCtx[0]->codec_id);
    if (sdl2_dev->pCodec[0] == NULL)
    {
        printf("%s","can't find codec\r\n");
        return 1;
    }

    //5.打开解码器
    if (avcodec_open2(sdl2_dev->pCodecCtx[0],sdl2_dev->pCodec[0],NULL)<0)
    {
        printf("%s","can't open codec\r\n");
        return 1;
    }

	sdl2_dev->imageSize_width[0] = sdl2_dev->pCodecCtx[0]->width;
	sdl2_dev->imageSize_height[0] = sdl2_dev->pCodecCtx[0]->height;
	sdl2_dev->oneLineByteSize[0] = sdl2_dev->pCodecCtx[0]->width * sdl2_dev->components[0];
	if((sdl2_dev->pFormatCtx[0]->streams[sdl2_dev->v_stream_idx[0]]->avg_frame_rate.num != 0) && (sdl2_dev->pFormatCtx[0]->streams[sdl2_dev->v_stream_idx[0]]->avg_frame_rate.den != 0))
	{
		sdl2_dev->frame_rate[0] = (double)sdl2_dev->pFormatCtx[0]->streams[sdl2_dev->v_stream_idx[0]]->avg_frame_rate.num/sdl2_dev->pFormatCtx[0]->streams[sdl2_dev->v_stream_idx[0]]->avg_frame_rate.den;
		sdl2_dev->frame_time[0] = (int)(1000000/sdl2_dev->frame_rate[0]);
	}
	

    //输出视频信息
    printf("video format：%s\r\n",sdl2_dev->pFormatCtx[0]->iformat->name);
    printf("video time：%d\r\n", (int)(sdl2_dev->pFormatCtx[0]->duration)/1000000);
    printf("video sizeX=%d,sizeY=%d\r\n",sdl2_dev->pCodecCtx[0]->width,sdl2_dev->pCodecCtx[0]->height);
	printf("video fps=%4.2f\r\n",sdl2_dev->frame_rate[0]);
    printf("video codec name：%s\r\n",sdl2_dev->pCodec[0]->name);

    //准备读取
    //AVPacket用于存储一帧一帧的压缩数据(解封装后的数据)
    //缓冲区，开辟空间
    sdl2_dev->packet[0] = (AVPacket*)av_malloc(sizeof(AVPacket));

    //AVFrame用于存储解码后的像素数据(YUV)
    //内存分配
    sdl2_dev->pFrame[0] = av_frame_alloc();//用于存储一帧解码后，原始像素格式的数据
    //RGB888	
    sdl2_dev->pFrameRGB[0] = av_frame_alloc();//用于存储一帧解码后，由原始像素格式转换成rgb后的数据
	
    //只有指定了AVFrame的像素格式、画面大小才能真正分配内存
    //缓冲区分配内存
    sdl2_dev->out_buffer[0] = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, sdl2_dev->pCodecCtx[0]->width, sdl2_dev->pCodecCtx[0]->height,1));
    //初始化缓冲区
    av_image_fill_arrays(sdl2_dev->pFrameRGB[0]->data,sdl2_dev->pFrameRGB[0]->linesize,sdl2_dev->out_buffer[0],AV_PIX_FMT_RGB24,sdl2_dev->pCodecCtx[0]->width, sdl2_dev->pCodecCtx[0]->height,1);
  
    //用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
    sdl2_dev->sws_ctx[0] = sws_getContext(sdl2_dev->pCodecCtx[0]->width,sdl2_dev->pCodecCtx[0]->height,sdl2_dev->pCodecCtx[0]->pix_fmt,
                                                sdl2_dev->pCodecCtx[0]->width, sdl2_dev->pCodecCtx[0]->height, AV_PIX_FMT_RGB24,
                                                SWS_BICUBIC, NULL, NULL, NULL);
    int got_picture, ret;
	
    int frame_count = 0;

	for(i=0;i<sdl2_dev->outputChNum;i++)
	{
		sdl2_dev->layerInfo[i].inputPort = 0;
		sdl2_dev->outLayerAlpha.alpha[i] = 0xFF;
	}

	init_sdl2_display(sdl2_dev);

	pthread_t tid_alpha;
	pthread_create(&tid_alpha,NULL,task_alpha,sdl2_dev);//创建一个线程，用以淡入淡出

    //6.一帧一帧的读取压缩数据
    while (av_read_frame(sdl2_dev->pFormatCtx[0], sdl2_dev->packet[0]) >= 0)
    {
        //只要视频压缩数据（根据流的索引位置判断）
        if (sdl2_dev->packet[0]->stream_index == sdl2_dev->v_stream_idx[0])
        {
            //7.解码一帧视频压缩数据，得到视频像素数据
            gettimeofday(&tv[0],NULL);
			ret = avcodec_send_packet(sdl2_dev->pCodecCtx[0], sdl2_dev->packet[0]);
			got_picture = avcodec_receive_frame(sdl2_dev->pCodecCtx[0], sdl2_dev->pFrame[0]); //got_picture = 0 success, a frame was returned
			
            if (ret < 0)
            {
                printf("%s","decode error\r\n");
                break;
            }

            //为0说明解码完成，非0正在解码
            if (got_picture == 0)
            {
                //AVFrame转为像素格式YUV420，宽高
                //2 6输入、输出数据
                //3 7输入、输出画面一行的数据的大小 AVFrame 转换是一行一行转换的
                //4 输入数据第一列要转码的位置 从0开始
                //5 输入画面的高度
                int currentLayer;
                sws_scale(sdl2_dev->sws_ctx[0],(const uint8_t **) sdl2_dev->pFrame[0]->data, sdl2_dev->pFrame[0]->linesize, 0, sdl2_dev->pCodecCtx[0]->height,
                          sdl2_dev->pFrameRGB[0]->data, sdl2_dev->pFrameRGB[0]->linesize);
				sdl2_clear_frame(sdl2_dev);
				for(currentLayer=0;currentLayer<sdl2_dev->outputChNum;currentLayer++)
				{
					sdl2_dev->imageFrameBuffer[0] = sdl2_dev->pFrameRGB[0]->data[0];
					
					if(currentLayer == 0)
					{
						sdl2_dev->layerInfo[currentLayer].crop.x = 0;
						sdl2_dev->layerInfo[currentLayer].crop.y = 0;
						sdl2_dev->layerInfo[currentLayer].crop.w = sdl2_dev->imageSize_width[0];
						sdl2_dev->layerInfo[currentLayer].crop.h = sdl2_dev->imageSize_height[0];

						sdl2_dev->layerInfo[currentLayer].scale.x = 0;
				       	sdl2_dev->layerInfo[currentLayer].scale.y = 0;
				        sdl2_dev->layerInfo[currentLayer].scale.w = sdl2_dev->windowsSize_width/2;		
				        sdl2_dev->layerInfo[currentLayer].scale.h = sdl2_dev->windowsSize_height/2;
					}
					else if(currentLayer == 1)
					{
						sdl2_dev->layerInfo[currentLayer].crop.x = 0;
						sdl2_dev->layerInfo[currentLayer].crop.y = 0;
						sdl2_dev->layerInfo[currentLayer].crop.w = sdl2_dev->imageSize_width[0];
						sdl2_dev->layerInfo[currentLayer].crop.h = sdl2_dev->imageSize_height[0];

						sdl2_dev->layerInfo[currentLayer].scale.x = sdl2_dev->windowsSize_width/2;
				        sdl2_dev->layerInfo[currentLayer].scale.y = 0;
				        sdl2_dev->layerInfo[currentLayer].scale.w = sdl2_dev->windowsSize_width/2;		
				        sdl2_dev->layerInfo[currentLayer].scale.h = sdl2_dev->windowsSize_height/2;
					}
					else if(currentLayer == 2)
					{
						sdl2_dev->layerInfo[currentLayer].crop.x = 0;
						sdl2_dev->layerInfo[currentLayer].crop.y = 0;
						sdl2_dev->layerInfo[currentLayer].crop.w = sdl2_dev->imageSize_width[0];
						sdl2_dev->layerInfo[currentLayer].crop.h = sdl2_dev->imageSize_height[0];

						sdl2_dev->layerInfo[currentLayer].scale.x = 0;
				        sdl2_dev->layerInfo[currentLayer].scale.y = sdl2_dev->windowsSize_height/2;
				        sdl2_dev->layerInfo[currentLayer].scale.w = sdl2_dev->windowsSize_width/2;		
				        sdl2_dev->layerInfo[currentLayer].scale.h = sdl2_dev->windowsSize_height/2;
					}
					else if(currentLayer == 3)
					{
						sdl2_dev->layerInfo[currentLayer].crop.x = 0;
						sdl2_dev->layerInfo[currentLayer].crop.y = 0;
						sdl2_dev->layerInfo[currentLayer].crop.w = sdl2_dev->imageSize_width[0];
						sdl2_dev->layerInfo[currentLayer].crop.h = sdl2_dev->imageSize_height[0];

						sdl2_dev->layerInfo[currentLayer].scale.x = sdl2_dev->windowsSize_width/2;
				        sdl2_dev->layerInfo[currentLayer].scale.y = sdl2_dev->windowsSize_height/2;
				        sdl2_dev->layerInfo[currentLayer].scale.w = sdl2_dev->windowsSize_width/2;		
				        sdl2_dev->layerInfo[currentLayer].scale.h = sdl2_dev->windowsSize_height/2;
					}			
					else if(currentLayer == 4)
					{
						sdl2_dev->layerInfo[currentLayer].crop.x = 0;
						sdl2_dev->layerInfo[currentLayer].crop.y = 0;
						sdl2_dev->layerInfo[currentLayer].crop.w = sdl2_dev->imageSize_width[0]/2;
						sdl2_dev->layerInfo[currentLayer].crop.h = sdl2_dev->imageSize_height[0]/2;

						sdl2_dev->layerInfo[currentLayer].scale.x = sdl2_dev->windowsSize_width/4;
				        sdl2_dev->layerInfo[currentLayer].scale.y = sdl2_dev->windowsSize_height/4;
				        sdl2_dev->layerInfo[currentLayer].scale.w = sdl2_dev->windowsSize_width/2;		
				        sdl2_dev->layerInfo[currentLayer].scale.h = sdl2_dev->windowsSize_height/2;
					}	
					
					sdl2_display_frame(sdl2_dev,currentLayer);	
				}
				sdl2_present_frame(sdl2_dev);
				gettimeofday(&tv[1],NULL);
				delayTime = sdl2_dev->frame_time[0] - ABS(tv[1].tv_usec - tv[0].tv_usec);
				if(delayTime > 0)
				{
					usleep(delayTime);
				}	
            }
        }
		inputcharNum=read(fd_tty,buf_tty,10);
        if(inputcharNum<0)
        {
        	//printf("no inputs\n");
        }
		else
		{
			if(buf_tty[0] == 'q')	break;
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
		
        av_packet_unref(sdl2_dev->packet[0]);		//av_packet_unref是清空里边的数据,不是释放
    }

	quit_sdl2_display(sdl2_dev);

	sws_freeContext(sdl2_dev->sws_ctx[0]);
	av_free(sdl2_dev->out_buffer[0]);
	av_frame_free(&sdl2_dev->pFrameRGB[0]);
	av_frame_free(&sdl2_dev->pFrame[0]);
	av_packet_free(&sdl2_dev->packet[0]);		//这才是释放
	avcodec_close(sdl2_dev->pCodecCtx[0]);
	avcodec_free_context(&sdl2_dev->pCodecCtx[0]);
    avformat_close_input(&sdl2_dev->pFormatCtx[0]);
    avformat_free_context(sdl2_dev->pFormatCtx[0]);

	free(sdl2_dev);
	printf("free mem\r\n");
	return 0;
}


