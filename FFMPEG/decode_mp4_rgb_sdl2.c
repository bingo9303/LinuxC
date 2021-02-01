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
    //��ȡ��������ļ���
    int fd_tty,inputcharNum=0;
	char buf_tty[10];
	sdl2_display_info sdl2_dev;

	if (argc < 2) 
    {
        usage("insuffient auguments");
        exit(-1);
    }

	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//��ֻ���ͷ������ķ�ʽ���ļ�dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }
	

	sdl2_dev.pixformat = SDL_PIXELFORMAT_RGB24;
	sdl2_dev.windowsName = "play mp4";
	sdl2_dev.windowsSize_width = 960;
	sdl2_dev.windowsSize_height = 540;

	
    //1.ע���������
    av_register_all();

    //��װ��ʽ�����ģ�ͳ��ȫ�ֵĽṹ�壬��������Ƶ�ļ���װ��ʽ�������Ϣ
    AVFormatContext *pFormatCtx = avformat_alloc_context();

    //2.��������Ƶ�ļ�
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
    {
        printf("%s","open input file faild\r\n");
        return;
    }

    //3.��ȡ��Ƶ�ļ���Ϣ
    if (avformat_find_stream_info(pFormatCtx,NULL) < 0)
    {
        printf("%s","get video info faild\r\n");
        return;
    }

    //��ȡ��Ƶ��������λ��
    //�����������͵�������Ƶ������Ƶ������Ļ�������ҵ���Ƶ����ID
    int v_stream_idx = -1;
    int i = 0;
    //number of streams
    for (; i < pFormatCtx->nb_streams; i++)
    {
        //��������
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

    //ֻ��֪����Ƶ�ı��뷽ʽ�����ܹ����ݱ��뷽ʽȥ�ҵ�������
    //��ȡ��Ƶ���еı����������
    AVCodecContext *pCodecCtx = pFormatCtx->streams[v_stream_idx]->codec;
	
    //4.���ݱ�����������еı���id���Ҷ�Ӧ�Ľ���
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL)
    {
        printf("%s","can't find codec\r\n");
        return;
    }

    //5.�򿪽�����
    if (avcodec_open2(pCodecCtx,pCodec,NULL)<0)
    {
        printf("%s","can't open codec\r\n");
        return;
    }

	sdl2_dev.imageSize_width = pCodecCtx->width;
	sdl2_dev.imageSize_height = pCodecCtx->height;

    //�����Ƶ��Ϣ
    printf("video format��%s",pFormatCtx->iformat->name);
    printf("video time��%d", (pFormatCtx->duration)/1000000);
    printf("video sizeX=%d,sizeY=%d",pCodecCtx->width,pCodecCtx->height);
    printf("video codec name��%s",pCodec->name);

    //׼����ȡ
    //AVPacket���ڴ洢һ֡һ֡��ѹ������(���װ�������)
    //�����������ٿռ�
    AVPacket *packet = (AVPacket*)av_malloc(sizeof(AVPacket));

    //AVFrame���ڴ洢��������������(YUV)
    //�ڴ����
    AVFrame *pFrame = av_frame_alloc();//���ڴ洢һ֡�����ԭʼ���ظ�ʽ������
	
    //RGB888	
    AVFrame *pFrameRGB = av_frame_alloc();//���ڴ洢һ֡�������ԭʼ���ظ�ʽת����rgb�������
	
	
    //ֻ��ָ����AVFrame�����ظ�ʽ�������С�������������ڴ�
    //�����������ڴ�
    uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height));
    //��ʼ��������
    avpicture_fill((AVPicture *)pFrameRGB, out_buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

    //����ת�루���ţ��Ĳ�����ת֮ǰ�Ŀ�ߣ�ת֮��Ŀ�ߣ���ʽ��
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,
                                                pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24,
                                                SWS_BICUBIC, NULL, NULL, NULL);
    int got_picture, ret;
	
    int frame_count = 0;

	init_sdl2_display_one_layer(&sdl2_dev);

    //6.һ֡һ֡�Ķ�ȡѹ������
    while (av_read_frame(pFormatCtx, packet) >= 0)
    {
        //ֻҪ��Ƶѹ�����ݣ�������������λ���жϣ�
        if (packet->stream_index == v_stream_idx)
        {
            //7.����һ֡��Ƶѹ�����ݣ��õ���Ƶ��������
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if (ret < 0)
            {
                printf("%s","decode error\r\n");
                return;
            }

            //Ϊ0˵��������ɣ���0���ڽ���
            if (got_picture)
            {
                //AVFrameתΪ���ظ�ʽYUV420�����
                //2 6���롢�������
                //3 7���롢�������һ�е����ݵĴ�С AVFrame ת����һ��һ��ת����
                //4 �������ݵ�һ��Ҫת���λ�� ��0��ʼ
                //5 ���뻭��ĸ߶�
                sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
                          pFrameRGB->data, pFrameRGB->linesize);
				sdl2_dev.imageFrameBuffer = pFrameRGB->data[0];
				sdl2_display_frame(&sdl2_dev,NULL,NULL);
				usleep(10000);
                frame_count++;
              //  printf("�����%d֡\n",frame_count);
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

        av_free_packet(packet);		//av_free_packet�������ߵ�����,�����ͷ�
    }

	quit_sdl2_display_one_layer();

	sws_freeContext(sws_ctx);
	av_free(out_buffer);
	av_frame_free(&pFrameRGB);
	av_frame_free(&pFrame);
	av_packet_free(packet);		//������ͷ�
	avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);

}


