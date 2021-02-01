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


#include "sdl2_display.h"

void usage(char *msg)
{
    fprintf(stderr, "%s/n", msg);
    printf("Usage: fv some-jpeg-file.jpg/n");
}


int main(int argc, char *argv[])
{
    //获取输入输出文件名
    int fd_tty,inputcharNum=0;
	char buf_tty[10];
	sdl2_display_info sdl2_dev;

	if (argc < 2) 
    {
        usage("insuffient auguments");
        exit(-1);
    }

	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//用只读和非阻塞的方式打开文件dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }
	

	sdl2_dev.pixformat = SDL_PIXELFORMAT_RGB24;
	sdl2_dev.windowsName = "play mp4";
	sdl2_dev.windowsSize_width = 960;
	sdl2_dev.windowsSize_height = 540;

	
    //1.注册所有组件
    av_register_all();

    //封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
    AVFormatContext *pFormatCtx = avformat_alloc_context();

    //2.打开输入视频文件
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
    {
        printf("%s","open input file faild\r\n");
        return;
    }

    //3.获取视频文件信息
    if (avformat_find_stream_info(pFormatCtx,NULL) < 0)
    {
        printf("%s","get video info faild\r\n");
        return;
    }

    //获取视频流的索引位置
    //遍历所有类型的流（音频流、视频流、字幕流），找到视频流的ID
    int v_stream_idx = -1;
    int i = 0;
    //number of streams
    for (; i < pFormatCtx->nb_streams; i++)
    {
        //流的类型
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            v_stream_idx = i;
            break;
        }
    }

    if (v_stream_idx == -1)
    {
        printf("%s","can't find video stream\r\n");
        return;
    }

    //只有知道视频的编码方式，才能够根据编码方式去找到解码器
    //获取视频流中的编解码上下文
    AVCodecContext *pCodecCtx = pFormatCtx->streams[v_stream_idx]->codec;
	
    //4.根据编解码上下文中的编码id查找对应的解码
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL)
    {
        printf("%s","can't find codec\r\n");
        return;
    }

    //5.打开解码器
    if (avcodec_open2(pCodecCtx,pCodec,NULL)<0)
    {
        printf("%s","can't open codec\r\n");
        return;
    }

	sdl2_dev.imageSize_width = pCodecCtx->width;
	sdl2_dev.imageSize_height = pCodecCtx->height;

    //输出视频信息
    printf("video format：%s",pFormatCtx->iformat->name);
    printf("video time：%d", (pFormatCtx->duration)/1000000);
    printf("video sizeX=%d,sizeY=%d",pCodecCtx->width,pCodecCtx->height);
    printf("video codec name：%s",pCodec->name);

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
    uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height));
    //初始化缓冲区
    avpicture_fill((AVPicture *)pFrameRGB, out_buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

    //用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,
                                                pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24,
                                                SWS_BICUBIC, NULL, NULL, NULL);
    int got_picture, ret;
	
    int frame_count = 0;

	init_sdl2_display_one_layer(&sdl2_dev);

    //6.一帧一帧的读取压缩数据
    while (av_read_frame(pFormatCtx, packet) >= 0)
    {
        //只要视频压缩数据（根据流的索引位置判断）
        if (packet->stream_index == v_stream_idx)
        {
            //7.解码一帧视频压缩数据，得到视频像素数据
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if (ret < 0)
            {
                printf("%s","decode error\r\n");
                return;
            }

            //为0说明解码完成，非0正在解码
            if (got_picture)
            {
                //AVFrame转为像素格式YUV420，宽高
                //2 6输入、输出数据
                //3 7输入、输出画面一行的数据的大小 AVFrame 转换是一行一行转换的
                //4 输入数据第一列要转码的位置 从0开始
                //5 输入画面的高度
                sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
                          pFrameRGB->data, pFrameRGB->linesize);
				sdl2_dev.imageFrameBuffer = pFrameRGB->data[0];
				sdl2_display_frame(&sdl2_dev,NULL,NULL);
				usleep(10000);
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

        av_free_packet(packet);		//av_free_packet是清空里边的数据,不是释放
    }

	quit_sdl2_display_one_layer();

	sws_freeContext(sws_ctx);
	av_free(out_buffer);
	av_frame_free(&pFrameRGB);
	av_frame_free(&pFrame);
	av_packet_free(packet);		//这才是释放
	avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);

}


