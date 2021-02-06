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

	
	
	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//��ֻ���ͷ������ķ�ʽ���ļ�dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }


	sdl2_dev->pixformat[0] = SDL_PIXELFORMAT_RGB24;
	sdl2_dev->components[0] = 3;
	sdl2_dev->windowsName = "play mp4";
	
    //1.ע���������
  //  av_register_all();

    //��װ��ʽ�����ģ�ͳ��ȫ�ֵĽṹ�壬��������Ƶ�ļ���װ��ʽ�������Ϣ
    sdl2_dev->pFormatCtx[0] = avformat_alloc_context();

    //2.��������Ƶ�ļ�
    if (avformat_open_input(&sdl2_dev->pFormatCtx[0], inputFile, NULL, NULL) != 0)
    {
        printf("%s","open input file faild\r\n");
        return 1;
    }

    //3.��ȡ��Ƶ�ļ���Ϣ
    if (avformat_find_stream_info(sdl2_dev->pFormatCtx[0],NULL) < 0)
    {
        printf("%s","get video info faild\r\n");
        return 1;
    }
    //��ȡ��Ƶ��������λ��
    //�����������͵�������Ƶ������Ƶ������Ļ�������ҵ���Ƶ����ID
    sdl2_dev->v_stream_idx[0] = -1;
    int i = 0;
    //number of streams
    for (; i < sdl2_dev->pFormatCtx[0]->nb_streams; i++)
    {
        //��������
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

    //ֻ��֪����Ƶ�ı��뷽ʽ�����ܹ����ݱ��뷽ʽȥ�ҵ�������
    //��ȡ��Ƶ���еı����������
    sdl2_dev->pCodecCtx[0] = avcodec_alloc_context3(NULL);
	if (sdl2_dev->pCodecCtx[0] == NULL) {
		printf("%s","can't alloc pCodecCtx\r\n");
		return 1;
	}
	avcodec_parameters_to_context(sdl2_dev->pCodecCtx[0], sdl2_dev->pFormatCtx[0]->streams[sdl2_dev->v_stream_idx[0]]->codecpar);	//�°���Ҫ��ôת��
	
    //4.���ݱ�����������еı���id���Ҷ�Ӧ�Ľ���
    sdl2_dev->pCodec[0] = avcodec_find_decoder(sdl2_dev->pCodecCtx[0]->codec_id);
    if (sdl2_dev->pCodec[0] == NULL)
    {
        printf("%s","can't find codec\r\n");
        return 1;
    }

    //5.�򿪽�����
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
	

    //�����Ƶ��Ϣ
    printf("video format��%s\r\n",sdl2_dev->pFormatCtx[0]->iformat->name);
    printf("video time��%d\r\n", (int)(sdl2_dev->pFormatCtx[0]->duration)/1000000);
    printf("video sizeX=%d,sizeY=%d\r\n",sdl2_dev->pCodecCtx[0]->width,sdl2_dev->pCodecCtx[0]->height);
	printf("video fps=%4.2f\r\n",sdl2_dev->frame_rate[0]);
    printf("video codec name��%s\r\n",sdl2_dev->pCodec[0]->name);

    //׼����ȡ
    //AVPacket���ڴ洢һ֡һ֡��ѹ������(���װ�������)
    //�����������ٿռ�
    sdl2_dev->packet[0] = (AVPacket*)av_malloc(sizeof(AVPacket));

    //AVFrame���ڴ洢��������������(YUV)
    //�ڴ����
    sdl2_dev->pFrame[0] = av_frame_alloc();//���ڴ洢һ֡�����ԭʼ���ظ�ʽ������
    //RGB888	
    sdl2_dev->pFrameRGB[0] = av_frame_alloc();//���ڴ洢һ֡�������ԭʼ���ظ�ʽת����rgb�������
	
    //ֻ��ָ����AVFrame�����ظ�ʽ�������С�������������ڴ�
    //�����������ڴ�
    sdl2_dev->out_buffer[0] = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, sdl2_dev->pCodecCtx[0]->width, sdl2_dev->pCodecCtx[0]->height,1));
    //��ʼ��������
    av_image_fill_arrays(sdl2_dev->pFrameRGB[0]->data,sdl2_dev->pFrameRGB[0]->linesize,sdl2_dev->out_buffer[0],AV_PIX_FMT_RGB24,sdl2_dev->pCodecCtx[0]->width, sdl2_dev->pCodecCtx[0]->height,1);
  
    //����ת�루���ţ��Ĳ�����ת֮ǰ�Ŀ�ߣ�ת֮��Ŀ�ߣ���ʽ��
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
	pthread_create(&tid_alpha,NULL,task_alpha,sdl2_dev);//����һ���̣߳����Ե��뵭��

    //6.һ֡һ֡�Ķ�ȡѹ������
    while (av_read_frame(sdl2_dev->pFormatCtx[0], sdl2_dev->packet[0]) >= 0)
    {
        //ֻҪ��Ƶѹ�����ݣ�������������λ���жϣ�
        if (sdl2_dev->packet[0]->stream_index == sdl2_dev->v_stream_idx[0])
        {
            //7.����һ֡��Ƶѹ�����ݣ��õ���Ƶ��������
            gettimeofday(&tv[0],NULL);
			ret = avcodec_send_packet(sdl2_dev->pCodecCtx[0], sdl2_dev->packet[0]);
			got_picture = avcodec_receive_frame(sdl2_dev->pCodecCtx[0], sdl2_dev->pFrame[0]); //got_picture = 0 success, a frame was returned
			
            if (ret < 0)
            {
                printf("%s","decode error\r\n");
                break;
            }

            //Ϊ0˵��������ɣ���0���ڽ���
            if (got_picture == 0)
            {
                //AVFrameתΪ���ظ�ʽYUV420�����
                //2 6���롢�������
                //3 7���롢�������һ�е����ݵĴ�С AVFrame ת����һ��һ��ת����
                //4 �������ݵ�һ��Ҫת���λ�� ��0��ʼ
                //5 ���뻭��ĸ߶�
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
		
        av_packet_unref(sdl2_dev->packet[0]);		//av_packet_unref�������ߵ�����,�����ͷ�
    }

	quit_sdl2_display(sdl2_dev);

	sws_freeContext(sdl2_dev->sws_ctx[0]);
	av_free(sdl2_dev->out_buffer[0]);
	av_frame_free(&sdl2_dev->pFrameRGB[0]);
	av_frame_free(&sdl2_dev->pFrame[0]);
	av_packet_free(&sdl2_dev->packet[0]);		//������ͷ�
	avcodec_close(sdl2_dev->pCodecCtx[0]);
	avcodec_free_context(&sdl2_dev->pCodecCtx[0]);
    avformat_close_input(&sdl2_dev->pFormatCtx[0]);
    avformat_free_context(sdl2_dev->pFormatCtx[0]);

	free(sdl2_dev);
	printf("free mem\r\n");
	return 0;
}


