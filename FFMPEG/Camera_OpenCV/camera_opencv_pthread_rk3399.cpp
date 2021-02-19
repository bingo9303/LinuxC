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
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <cstdlib>


extern "C" { 
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h> 
}

using namespace std;
using namespace cv;


#define MAX_CH_NUM		2
typedef struct SDL_Rect
{
    int x, y;
    int w, h;
} SDL_Rect;

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
	char alphaFlag[MAX_CH_NUM];				//1-����  2-����0-��������
} OutLayerSetting;


typedef struct _sdl2_display_info_multiple_input
{
/* ������� */
	int outputChNum;
	char* windowsName;
	int windowsSize_width;
	int windowsSize_height;

/* ������� */
	char** inputFile[MAX_CH_NUM];
	int inputSourceNum;	
	int imageSize_width[MAX_CH_NUM];
	int imageSize_height[MAX_CH_NUM];
	unsigned char* imageFrameBuffer[MAX_CH_NUM][3];		//һ֡ͼ���buff	
	int pixformat[MAX_CH_NUM];
	int components[MAX_CH_NUM];
	unsigned int oneLineByteSize[MAX_CH_NUM][3];
	int frame_time[MAX_CH_NUM];
	double frame_rate[MAX_CH_NUM];
/* ͼ����� */
	LayerSetting layerInfo[MAX_CH_NUM];
	OutLayerSetting outLayerAlpha;
	int alphaTime;

/* FFMEPH���� */
	AVFormatContext *pFormatCtx[MAX_CH_NUM];
	AVCodecContext *pCodecCtx[MAX_CH_NUM];
	AVCodec *pCodec[MAX_CH_NUM];
	AVPacket *packet[MAX_CH_NUM];
	AVFrame *pFrame[MAX_CH_NUM];
	AVFrame *pFrameDecode[MAX_CH_NUM];
	uint8_t *out_buffer[MAX_CH_NUM];
	struct SwsContext *sws_ctx[MAX_CH_NUM];
	int v_stream_idx[MAX_CH_NUM];
	
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

/* ʱ��� */
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


std::unique_ptr<uint8_t[]> data(new uint8_t[1920 * 1080 * 3]);
cv::Mat mat(1080, 1920, CV_8UC3);
//	int width_new = 320, height_new = 240;
int width_new = 1920, height_new = 1080;
cv::Mat dst(height_new, width_new, CV_8UC3);
const char* winname = "usb video1";


void* task_command(void* args)
{
	int inputcharNum;
	char buf_tty[10];
	camera_info* dev = (camera_info*)args;
	while(1)
	{/*
		//��ʱ��Ҫ�����Ż�һ�������е��������
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


void* task_decode_0(void* args)
{
	camera_info* dev = (camera_info*)args;
	int got_picture, ret;
	int delayTime; 
	while (av_read_frame(dev->pFormatCtx[0], dev->packet[0]) >= 0)
    {
    	if(stopAllTaskFlag == 1)	break;
		
        if (dev->packet[0]->stream_index == dev->v_stream_idx[0])
        {
        	gettimeofday(&decode_tv[0][0],NULL);
			{
				ret = avcodec_send_packet(dev->pCodecCtx[0], dev->packet[0]);
				got_picture = avcodec_receive_frame(dev->pCodecCtx[0], dev->pFrame[0]); //got_picture = 0 success, a frame was returned
				
	            if (ret < 0)
	            {
	                printf("%s","decode error\r\n");
	                continue;
	            }

	            //Ϊ0˵��������ɣ���0���ڽ���
	            if (got_picture == 0)
	            {
					uint8_t* p[1] = { data.get() };
					int dst_stride[1] = { dev->pFrame[0]->width * 3 };
				//	sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height, p, dst_stride);
				//	mat.data = data.get();
				//	cv::resize(mat, dst, cv::Size(width_new, height_new));
				//	cv::imshow(winname, dst);

				
	                sws_scale(dev->sws_ctx[0],(const uint8_t **) dev->pFrame[0]->data, dev->pFrame[0]->linesize, 0, dev->pCodecCtx[0]->height,
	                          p, dst_stride);
					mat.data = data.get();
					
					gettimeofday(&decode_tv[0][1],NULL);
					delayTime = dev->frame_time[0]
								- ABS(decode_tv[0][1].tv_usec - decode_tv[0][0].tv_usec)
								- ABS(display_tv[0][1].tv_usec - display_tv[0][0].tv_usec);

								
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
        av_packet_unref(dev->packet[0]);		//av_packet_unref�������ߵ�����,�����ͷ�
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

	            //Ϊ0˵��������ɣ���0���ڽ���
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
        av_packet_unref(dev->packet[1]);		//av_packet_unref�������ߵ�����,�����ͷ�
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
	while(1)
	{
		sem_wait(&display_sem[0]);
		if((pthread_kill(tid_decode[0],0) != 0) || (stopAllTaskFlag == 1))	break;
		pthread_mutex_lock(&sdl2_mutex);
		//sdl2_clear_frame(sdl2_dev);//Ҫ�ģ���Ȼ����˸
		dev->imageFrameBuffer[0][0] = dev->pFrameDecode[0]->data[0];
		dev->imageFrameBuffer[0][1] = dev->pFrameDecode[0]->data[1];
		dev->imageFrameBuffer[0][2] = dev->pFrameDecode[0]->data[2];
		dev->oneLineByteSize[0][0] = dev->pFrameDecode[0]->linesize[0];
		dev->oneLineByteSize[0][1] = dev->pFrameDecode[0]->linesize[1];
		dev->oneLineByteSize[0][2] = dev->pFrameDecode[0]->linesize[2];

		dev->layerInfo[0].crop.x = 0;
		dev->layerInfo[0].crop.y = 0;
		dev->layerInfo[0].crop.w = dev->imageSize_width[0];
		dev->layerInfo[0].crop.h = dev->imageSize_height[0];

		dev->layerInfo[0].scale.x = 0;
       	dev->layerInfo[0].scale.y = 0;
        dev->layerInfo[0].scale.w = dev->windowsSize_width;		
        dev->layerInfo[0].scale.h = dev->windowsSize_height;


		//uint8_t* p[1] = { data.get() };
		//int dst_stride[1] = { frame->width * 3 };
		//sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height, p, dst_stride);
		


		
		if(dev->outLayerAlpha.alphaFlag[0] == 0)
		{
			gettimeofday(&display_tv[0][0],NULL);
			/*
			mat.data = data.get();
			cv::resize(mat, dst, cv::Size(width_new, height_new));
			cv::imshow(winname, dst);*/
			cv::imshow(winname, mat);
			
			
			gettimeofday(&display_tv[0][1],NULL);
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
		//	sdl2_display_frame(dev,1); 
		//	sdl2_present_frame(dev);	
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
	for(m=0;m<dev->inputSourceNum;m++)
	{
		dev->components[m] = 3;
		dev->v_stream_idx[m] = -1;

		const char* url = "/dev/video10";
	 
		AVInputFormat* input_fmt = av_find_input_format("video4linux2");

		if(input_fmt == NULL) printf("222222222\r\n");
		
		dev->pFormatCtx[m] = avformat_alloc_context();
		
		//2.��������Ƶ�ļ�
	    if (avformat_open_input(&dev->pFormatCtx[m], url, input_fmt, NULL) != 0)
	    {
	        printf("%s","open input file faild\r\n");
	        return 1;
	    }

		 //3.��ȡ��Ƶ�ļ���Ϣ
	    if (avformat_find_stream_info(dev->pFormatCtx[m],NULL) < 0)
	    {
	        printf("%s","get video info faild\r\n");
	        return 1;
	    }

		//��ȡ��Ƶ��������λ��
	    //�����������͵�������Ƶ������Ƶ������Ļ�������ҵ���Ƶ����ID
	    
	    int i = 0;
	    //number of streams
	    for (; i < dev->pFormatCtx[m]->nb_streams; i++)
	    {
	        //��������
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
		avcodec_parameters_to_context(dev->pCodecCtx[m], dev->pFormatCtx[m]->streams[dev->v_stream_idx[m]]->codecpar);	//�°���Ҫ��ôת��
		
	    //4.���ݱ�����������еı���id���Ҷ�Ӧ�Ľ���
	    
	    dev->pCodec[m] = avcodec_find_decoder(dev->pCodecCtx[m]->codec_id);
	    if (dev->pCodec[m] == NULL)
	    {
	        printf("%s","can't find codec\r\n");
	        return 1;
	    }

	    //5.�򿪽�����
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

		//�����Ƶ��Ϣ
		printf("[%d]video codec id: %d\r\n",m,dev->pCodecCtx[m]->codec_id);
		printf("[%d]video format��%s\r\n",m,dev->pFormatCtx[m]->iformat->name);
		printf("[%d]video time��%d\r\n", m,(int)(dev->pFormatCtx[m]->duration)/1000000);
		printf("[%d]video sizeX=%d,sizeY=%d\r\n",m,dev->pCodecCtx[m]->width,dev->pCodecCtx[m]->height);
		printf("[%d]video fps=%4.2f\r\n",m,dev->frame_rate[m]);
		printf("[%d]video codec name��%s\r\n",m,dev->pCodec[m]->name);


		//׼����ȡ
	    //AVPacket���ڴ洢һ֡һ֡��ѹ������(���װ�������)
	    //�����������ٿռ�
	    dev->packet[m] = (AVPacket*)av_malloc(sizeof(AVPacket));

	    //AVFrame���ڴ洢��������������(YUV)
	    //�ڴ����
	    dev->pFrame[m] = av_frame_alloc();//���ڴ洢һ֡�����ԭʼ���ظ�ʽ������
	    //RGB888	
	    dev->pFrameDecode[m] = av_frame_alloc();//���ڴ洢һ֡�������ԭʼ���ظ�ʽת����rgb�������


		
	    //ֻ��ָ����AVFrame�����ظ�ʽ�������С�������������ڴ�
	    //�����������ڴ�
	    dev->out_buffer[m] = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_BGR24, dev->pCodecCtx[m]->width, dev->pCodecCtx[m]->height,1));
	    //��ʼ��������
	    av_image_fill_arrays(dev->pFrameDecode[m]->data,dev->pFrameDecode[m]->linesize,dev->out_buffer[m],AV_PIX_FMT_BGR24,dev->pCodecCtx[m]->width, dev->pCodecCtx[m]->height,1);
	  
	    //����ת�루���ţ��Ĳ�����ת֮ǰ�Ŀ�ߣ�ת֮��Ŀ�ߣ���ʽ��
	    dev->sws_ctx[m] = sws_getContext(dev->pCodecCtx[m]->width,dev->pCodecCtx[m]->height,dev->pCodecCtx[m]->pix_fmt,
                                dev->pCodecCtx[m]->width, dev->pCodecCtx[m]->height, AV_PIX_FMT_BGR24,
                                SWS_BICUBIC, NULL, NULL, NULL);
	}


	
	cv::namedWindow(winname);

	
	return 0;
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

	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//��ֻ���ͷ������ķ�ʽ���ļ�dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }
	
	dev->windowsName = "play camera";

	if(init_device(dev))
	{
		printf("init sdl2 display faild\r\n");
		exit(-1);
	}

	

	for(m=0;m<dev->outputChNum;m++)
	{
		dev->layerInfo[m].inputPort = m;
		dev->outLayerAlpha.alpha[m] = 0xFF;
	}
	dev->outLayerAlpha.alpha[1] = 0;
	

	_pthreadFunc  pthread_decode[MAX_CH_NUM] = {task_decode_0,task_decode_1};
	_pthreadFunc  pthread_display[MAX_CH_NUM] = {task_display_0,task_display_1};
	
	pthread_create(&tid_alpha,NULL,task_alpha,dev);//����һ���̣߳����Ե��뵭��
	pthread_create(&tid_command,NULL,task_command,dev);//����һ���̣߳����Զ�ȡ�ն���Ϣ
	pthread_create(&tid_clear,NULL,task_clear,dev);//����һ���̣߳�����������ʾ

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
		av_packet_free(&dev->packet[m]);		//������ͷ�
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

