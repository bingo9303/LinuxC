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

#define ABS(x) ((x)<0?-(x):(x))


#define TEST_ENABLE	1



static const char short_options [] = "i:x:y:";  
  
static const struct option  
long_options [] = {  
        { "input",     	 required_argument,     NULL,          'i' }, 
        { "sizex",     	 no_argument,       	NULL,          'x' }, 
        { "sizey",       no_argument,           NULL,          'y' },  
        { 0, 0, 0, 0 }  
};  
  


int main(int argc, char *argv[])
{
	int m;
    int fd_tty,inputcharNum=0;
	char buf_tty[10];
	double frame_rate[MAX_LAYER_NUM] = {0};
	int frame_time[MAX_LAYER_NUM] = {0},delayTime[MAX_LAYER_NUM]={0};
	struct timeval tv[MAX_LAYER_NUM][2];
	char** inputFile[MAX_LAYER_NUM];
	SDL_Rect scale[MAX_LAYER_NUM];
	SDL_Rect crop[MAX_LAYER_NUM];
	sdl2_display_info_multiple_input sdl2_dev;
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
					inputFile[sdl2_dev.layer] = malloc(100);
					strcpy((char*)inputFile[sdl2_dev.layer],optarg);
					sdl2_dev.layer++;
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

	if(sdl2_dev.layer == 0)
	{
		printf("no input file\r\n");
		exit(-1);
	}

	if((sdl2_dev.windowsSize_width == 0) && (sdl2_dev.windowsSize_height == 0))
	{
		sdl2_dev.windowsSize_width = 960;
		sdl2_dev.windowsSize_height = 540;
	}

	

	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//��ֻ���ͷ������ķ�ʽ���ļ�dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }
	
	sdl2_dev.windowsName = "play mp4";
	
    //1.ע���������
  //  av_register_all();

    //��װ��ʽ�����ģ�ͳ��ȫ�ֵĽṹ�壬��������Ƶ�ļ���װ��ʽ�������Ϣ
    AVFormatContext *pFormatCtx[sdl2_dev.layer];
	AVCodecContext *pCodecCtx[sdl2_dev.layer];
	AVCodec *pCodec[sdl2_dev.layer];
	AVPacket *packet[sdl2_dev.layer];
	AVFrame *pFrame[sdl2_dev.layer];
	AVFrame *pFrameRGB[sdl2_dev.layer];
	uint8_t *out_buffer[sdl2_dev.layer];
	struct SwsContext *sws_ctx[sdl2_dev.layer];
	int v_stream_idx[sdl2_dev.layer];
	memset(v_stream_idx,-1,sizeof(v_stream_idx));
	for(m=0;m<sdl2_dev.layer;m++)
	{
		sdl2_dev.pixformat[m] = SDL_PIXELFORMAT_RGB24;
		sdl2_dev.components[m] = 3;
		pFormatCtx[m] = avformat_alloc_context();
		//2.��������Ƶ�ļ�
	    if (avformat_open_input(&pFormatCtx[m], (char*)inputFile[m], NULL, NULL) != 0)
	    {
	        printf("%s","open input file faild\r\n");
	        return 1;
	    }

		 //3.��ȡ��Ƶ�ļ���Ϣ
	    if (avformat_find_stream_info(pFormatCtx[m],NULL) < 0)
	    {
	        printf("%s","get video info faild\r\n");
	        return 1;
	    }

		//��ȡ��Ƶ��������λ��
	    //�����������͵�������Ƶ������Ƶ������Ļ�������ҵ���Ƶ����ID
	    
	    int i = 0;
	    //number of streams
	    for (; i < pFormatCtx[m]->nb_streams; i++)
	    {
	        //��������
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
		avcodec_parameters_to_context(pCodecCtx[m], pFormatCtx[m]->streams[v_stream_idx[m]]->codecpar);	//�°���Ҫ��ôת��
		
	    //4.���ݱ�����������еı���id���Ҷ�Ӧ�Ľ���
	    pCodec[m] = avcodec_find_decoder(pCodecCtx[m]->codec_id);
	    if (pCodec[m] == NULL)
	    {
	        printf("%s","can't find codec\r\n");
	        return 1;
	    }

	    //5.�򿪽�����
	    if (avcodec_open2(pCodecCtx[m],pCodec[m],NULL)<0)
	    {
	        printf("%s","can't open codec\r\n");
	        return 1;
	    }

		sdl2_dev.imageSize_width[m] = pCodecCtx[m]->width;
		sdl2_dev.imageSize_height[m] = pCodecCtx[m]->height;

		
		if((pFormatCtx[m]->streams[v_stream_idx[m]]->avg_frame_rate.num != 0) && (pFormatCtx[m]->streams[v_stream_idx[m]]->avg_frame_rate.den != 0))
		{
			frame_rate[m] = (double)pFormatCtx[m]->streams[v_stream_idx[m]]->avg_frame_rate.num/pFormatCtx[m]->streams[v_stream_idx[m]]->avg_frame_rate.den;
			frame_time[m] = (int)(1000000/frame_rate[m]);
		}

		//�����Ƶ��Ϣ
		printf("[%d]video format��%s\r\n",m,pFormatCtx[m]->iformat->name);
		printf("[%d]video time��%d\r\n", m,(int)(pFormatCtx[m]->duration)/1000000);
		printf("[%d]video sizeX=%d,sizeY=%d\r\n",m,pCodecCtx[m]->width,pCodecCtx[m]->height);
		printf("[%d]video fps=%4.2f\r\n",m,frame_rate[m]);
		printf("[%d]video codec name��%s\r\n",m,pCodec[m]->name);


		//׼����ȡ
	    //AVPacket���ڴ洢һ֡һ֡��ѹ������(���װ�������)
	    //�����������ٿռ�
	    packet[m] = (AVPacket*)av_malloc(sizeof(AVPacket));

	    //AVFrame���ڴ洢��������������(YUV)
	    //�ڴ����
	    pFrame[m] = av_frame_alloc();//���ڴ洢һ֡�����ԭʼ���ظ�ʽ������
	    //RGB888	
	    pFrameRGB[m] = av_frame_alloc();//���ڴ洢һ֡�������ԭʼ���ظ�ʽת����rgb�������


		
	    //ֻ��ָ����AVFrame�����ظ�ʽ�������С�������������ڴ�
	    //�����������ڴ�
	    out_buffer[m] = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx[m]->width, pCodecCtx[m]->height,1));
	    //��ʼ��������
	    av_image_fill_arrays(pFrameRGB[m]->data,pFrameRGB[m]->linesize,out_buffer[m],AV_PIX_FMT_RGB24,pCodecCtx[m]->width, pCodecCtx[m]->height,1);
	  
	    //����ת�루���ţ��Ĳ�����ת֮ǰ�Ŀ�ߣ�ת֮��Ŀ�ߣ���ʽ��
	    sws_ctx[m] = sws_getContext(pCodecCtx[m]->width,pCodecCtx[m]->height,pCodecCtx[m]->pix_fmt,
                                pCodecCtx[m]->width, pCodecCtx[m]->height, AV_PIX_FMT_RGB24,
                                SWS_BICUBIC, NULL, NULL, NULL);
	}
    int got_picture[sdl2_dev.layer], ret[sdl2_dev.layer];
	

	init_sdl2_display_multiple_input(&sdl2_dev);


	




	quit_sdl2_display_multiple_input(&sdl2_dev);

	for(m=0;m<sdl2_dev.layer;m++)
	{
		sws_freeContext(sws_ctx[m]);
		av_free(out_buffer[m]);
		av_frame_free(&pFrameRGB[m]);
		av_frame_free(&pFrame[m]);
		av_packet_free(&packet[m]);		//������ͷ�
		avcodec_close(pCodecCtx[m]);
		avcodec_free_context(&pCodecCtx[m]);
	    avformat_close_input(&pFormatCtx[m]);
	    avformat_free_context(pFormatCtx[m]);
	}
	
	printf("free mem\r\n");
	return 0;
}









