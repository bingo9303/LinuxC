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
#include "sdl2_display.h"
#define ABS(x) ((x)<0?-(x):(x))



static const char short_options [] = "i:x:y:";  
  
static const struct option  
long_options [] = {  
        { "input",     	 required_argument,     NULL,          'i' }, 
        { "sizex",     	 no_argument,       	NULL,          'x' }, 
        { "sizey",       no_argument,           NULL,          'y' },  
        { "layer",       no_argument,           NULL,          'l' }, 
        { 0, 0, 0, 0 }  
};  


int main(int argc, char *argv[])
{
    //获取输入输出文件名
    int fd_tty,inputcharNum=0;
	char buf_tty[10];
	sdl2_display_info sdl2_dev;
	double frame_rate = 0;
	char inputFile[100] = {0};
	int frame_time = 0,delayTime;
	struct timeval tv[2];
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
					strcpy(inputFile,optarg);
	                break;  

	        case 'x':  
					sdl2_dev.windowsSize_width = atoi(optarg);
					break;
	        case 'y':  
	                sdl2_dev.windowsSize_height = atoi(optarg);
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

	if((sdl2_dev.windowsSize_width == 0) && (sdl2_dev.windowsSize_height == 0))
	{
		sdl2_dev.windowsSize_width = 960;
		sdl2_dev.windowsSize_height = 540;
	}

	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//用只读和非阻塞的方式打开文件dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }

	

	sdl2_dev.pixformat = SDL_PIXELFORMAT_RGB24;
	sdl2_dev.components = 3;
	sdl2_dev.windowsName = "play mp4";
	//sdl2_dev.windowsSize_width = 960;
	//sdl2_dev.windowsSize_height = 540;
	sdl2_dev.layer = 1;
	
    //1.注册所有组件
  //  av_register_all();

    //封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
    AVFormatContext *pFormatCtx = avformat_alloc_context();

    //2.打开输入视频文件
    if (avformat_open_input(&pFormatCtx, inputFile, NULL, NULL) != 0)
    {
        printf("%s","open input file faild\r\n");
        return 1;
    }

    //3.获取视频文件信息
    if (avformat_find_stream_info(pFormatCtx,NULL) < 0)
    {
        printf("%s","get video info faild\r\n");
        return 1;
    }
    //获取视频流的索引位置
    //遍历所有类型的流（音频流、视频流、字幕流），找到视频流的ID
    int v_stream_idx = -1;
    int i = 0;
    //number of streams
    for (; i < pFormatCtx->nb_streams; i++)
    {
        //流的类型
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            v_stream_idx = i;
            break;
        }
    }

    if (v_stream_idx == -1)
    {
        printf("%s","can't find video stream\r\n");
        return 1;
    }

    //只有知道视频的编码方式，才能够根据编码方式去找到解码器
    //获取视频流中的编解码上下文
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(NULL);
	if (pCodecCtx == NULL) {
		printf("%s","can't alloc pCodecCtx\r\n");
		return 1;
	}
	avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[v_stream_idx]->codecpar);	//新版需要这么转化
	
    //4.根据编解码上下文中的编码id查找对应的解码
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL)
    {
        printf("%s","can't find codec\r\n");
        return 1;
    }

    //5.打开解码器
    if (avcodec_open2(pCodecCtx,pCodec,NULL)<0)
    {
        printf("%s","can't open codec\r\n");
        return 1;
    }

	sdl2_dev.imageSize_width = pCodecCtx->width;
	sdl2_dev.imageSize_height = pCodecCtx->height;
	if((pFormatCtx->streams[v_stream_idx]->avg_frame_rate.num != 0) && (pFormatCtx->streams[v_stream_idx]->avg_frame_rate.den != 0))
	{
		frame_rate = (double)pFormatCtx->streams[v_stream_idx]->avg_frame_rate.num/pFormatCtx->streams[v_stream_idx]->avg_frame_rate.den;
		frame_time = (int)(1000000/frame_rate);
	}
	

    //输出视频信息
    printf("video format：%s\r\n",pFormatCtx->iformat->name);
    printf("video time：%d\r\n", (int)(pFormatCtx->duration)/1000000);
    printf("video sizeX=%d,sizeY=%d\r\n",pCodecCtx->width,pCodecCtx->height);
	printf("video fps=%4.2f\r\n",frame_rate);
    printf("video codec name：%s\r\n",pCodec->name);

    //准备读取
    //AVPacket用于存储一帧一帧的压缩数据(解封装后的数据)
    //缓冲区，开辟空间
    AVPacket *packet = (AVPacket*)av_malloc(sizeof(AVPacket));

    //AVFrame用于存储解码后的像素数据(YUV)
    //内存分配
    AVFrame *pFrame = av_frame_alloc();//用于存储一帧解码后，原始像素格式的数据
    //RGB888	
    AVFrame *pFrameRGB = av_frame_alloc();//用于存储一帧解码后，由原始像素格式转换成rgb后的数据
	
    //只有指定了AVFrame的像素格式、画面大小才能真正分配内存
    //缓冲区分配内存
    uint8_t *out_buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height,1));
    //初始化缓冲区
    av_image_fill_arrays(pFrameRGB->data,pFrameRGB->linesize,out_buffer,AV_PIX_FMT_RGB24,pCodecCtx->width, pCodecCtx->height,1);
  
    //用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,
                                                pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24,
                                                SWS_BICUBIC, NULL, NULL, NULL);
    int got_picture, ret;
	
    int frame_count = 0;

	init_sdl2_display_one_input(&sdl2_dev);

    //6.一帧一帧的读取压缩数据
    while (av_read_frame(pFormatCtx, packet) >= 0)
    {
        //只要视频压缩数据（根据流的索引位置判断）
        if (packet->stream_index == v_stream_idx)
        {
            //7.解码一帧视频压缩数据，得到视频像素数据
            gettimeofday(&tv[0],NULL);
			ret = avcodec_send_packet(pCodecCtx, packet);
			got_picture = avcodec_receive_frame(pCodecCtx, pFrame); //got_picture = 0 success, a frame was returned
			
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
                sws_scale(sws_ctx,(const uint8_t **) pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
                          pFrameRGB->data, pFrameRGB->linesize);
				sdl2_dev.imageFrameBuffer = pFrameRGB->data[0];
				sdl2_dev.currentLayer = 0;
				sdl2_clear_frame(&sdl2_dev);
				sdl2_display_frame(&sdl2_dev,NULL,NULL);
				sdl2_present_frame(&sdl2_dev);
				gettimeofday(&tv[1],NULL);
				delayTime = frame_time - ABS(tv[1].tv_usec - tv[0].tv_usec);
				if(delayTime > 0)
				{
					usleep(delayTime);
				}				
				
				
                frame_count++;
              //  printf("解码第%d帧\n",frame_count);
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
		}

        av_packet_unref(packet);		//av_packet_unref是清空里边的数据,不是释放
    }

	quit_sdl2_display_one_input(&sdl2_dev);

	sws_freeContext(sws_ctx);
	av_free(out_buffer);
	av_frame_free(&pFrameRGB);
	av_frame_free(&pFrame);
	av_packet_free(&packet);		//这才是释放
	avcodec_close(pCodecCtx);
	avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);
	printf("free mem\r\n");
	return 0;
}


