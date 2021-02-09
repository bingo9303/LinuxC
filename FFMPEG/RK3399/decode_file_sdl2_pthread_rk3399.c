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
#include "sdl2_display_rk3399.h"
#include <getopt.h> 
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>


extern int decode_simple(MpiDecLoopData *data, AVPacket *av_packet,sdl2_display_info* sdl2_dev,int inputCh);


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

/*
static int decode_packet(AVCodecContext *dec, const AVPacket *pkt)
{
    int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }

    // get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                return 0;

            fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
            return ret;
        }

        // write the frame data to output file
        if (dec->codec->type == AVMEDIA_TYPE_VIDEO)
            ret = output_video_frame(frame);
        else
            ret = output_audio_frame(frame);

        av_frame_unref(frame);
        if (ret < 0)
            return ret;
    }

    return 0;
}*/


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
	int delayTime; 
	while (av_read_frame(sdl2_dev->pFormatCtx[0], sdl2_dev->packet[0]) >= 0)
    {
    	if(stopAllTaskFlag == 1)	break;
		
        if (sdl2_dev->packet[0]->stream_index == sdl2_dev->v_stream_idx[0])
        {
        	gettimeofday(&decode_tv[0][0],NULL);
#if ENABLE_MMP_DECODE
			if(sdl2_dev->pCodecCtx[0]->codec_id == AV_CODEC_ID_H264)
			{		
				decode_simple(&sdl2_dev->mmp_data[0], sdl2_dev->packet[0],sdl2_dev,0);
				gettimeofday(&decode_tv[0][1],NULL);
				delayTime = sdl2_dev->frame_time[0]
							- ABS(decode_tv[0][1].tv_usec - decode_tv[0][0].tv_usec)
							- ABS(display_tv[0][1].tv_usec - display_tv[0][0].tv_usec);
				if(delayTime > 0)
				{
					//printf("0.delayTime = %d\r\n",delayTime);
					usleep(delayTime);
				}	
				if(sdl2_dev->outLayerAlpha.alpha[0] != 0)
				{
					sem_post(&display_sem[0]);
				}	
			}
			else
#endif
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
	                          sdl2_dev->pFrameDecode[0]->data, sdl2_dev->pFrameDecode[0]->linesize);
					gettimeofday(&decode_tv[0][1],NULL);
					delayTime = sdl2_dev->frame_time[0]
								- ABS(decode_tv[0][1].tv_usec - decode_tv[0][0].tv_usec)
								- ABS(display_tv[0][1].tv_usec - display_tv[0][0].tv_usec);
					if(delayTime > 0)
					{
						//printf("0.delayTime = %d\r\n",delayTime);
						usleep(delayTime);
					}	
					if(sdl2_dev->outLayerAlpha.alpha[0] != 0)
					{
						sem_post(&display_sem[0]);
					}	
	            }
			}
        }		
        av_packet_unref(sdl2_dev->packet[0]);		//av_packet_unref是清空里边的数据,不是释放
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
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	int got_picture, ret;
	int delayTime;
	
	while (av_read_frame(sdl2_dev->pFormatCtx[1], sdl2_dev->packet[1]) >= 0)
    {
    	if(stopAllTaskFlag == 1)	break;
		
        if (sdl2_dev->packet[1]->stream_index == sdl2_dev->v_stream_idx[1])
        {
        	gettimeofday(&decode_tv[1][0],NULL);
#if ENABLE_MMP_DECODE
			if(sdl2_dev->pCodecCtx[1]->codec_id == AV_CODEC_ID_H264)
			{		
				decode_simple(&sdl2_dev->mmp_data[1], sdl2_dev->packet[1],sdl2_dev,1);
				gettimeofday(&decode_tv[1][1],NULL);
				delayTime = sdl2_dev->frame_time[1]
							- ABS(decode_tv[1][1].tv_usec - decode_tv[1][0].tv_usec)
							- ABS(display_tv[1][1].tv_usec - display_tv[1][0].tv_usec);
				if(delayTime > 0)
				{
					//printf("0.delayTime = %d\r\n",delayTime);
					usleep(delayTime);
				}	
				if(sdl2_dev->outLayerAlpha.alpha[1] != 0)
				{
					sem_post(&display_sem[1]);
				}	
			}
			else
#endif
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
	                          sdl2_dev->pFrameDecode[1]->data, sdl2_dev->pFrameDecode[1]->linesize);
					gettimeofday(&decode_tv[1][1],NULL);
					delayTime = sdl2_dev->frame_time[1]
								- ABS(decode_tv[1][1].tv_usec - decode_tv[1][0].tv_usec)
								- ABS(display_tv[1][1].tv_usec - display_tv[1][0].tv_usec);
					if(delayTime > 0)
					{
						//printf("1.delayTime = %d\r\n",delayTime);
						usleep(delayTime);
					}	
					if(sdl2_dev->outLayerAlpha.alpha[1] != 0)
					{
						sem_post(&display_sem[1]);
					}
	            }
			}
        }		
        av_packet_unref(sdl2_dev->packet[1]);		//av_packet_unref是清空里边的数据,不是释放
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
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	while(1)
	{
		sem_wait(&display_sem[0]);
		if((pthread_kill(tid_decode[0],0) != 0) || (stopAllTaskFlag == 1))	break;
		pthread_mutex_lock(&sdl2_mutex);
		//sdl2_clear_frame(sdl2_dev);//要改，不然会闪烁
		sdl2_dev->imageFrameBuffer[0][0] = sdl2_dev->pFrameDecode[0]->data[0];
		sdl2_dev->imageFrameBuffer[0][1] = sdl2_dev->pFrameDecode[0]->data[1];
		sdl2_dev->imageFrameBuffer[0][2] = sdl2_dev->pFrameDecode[0]->data[2];
		sdl2_dev->oneLineByteSize[0][0] = sdl2_dev->pFrameDecode[0]->linesize[0];
		sdl2_dev->oneLineByteSize[0][1] = sdl2_dev->pFrameDecode[0]->linesize[1];
		sdl2_dev->oneLineByteSize[0][2] = sdl2_dev->pFrameDecode[0]->linesize[2];

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
			gettimeofday(&display_tv[0][0],NULL);
			sdl2_display_frame(sdl2_dev,0);	
			sdl2_present_frame(sdl2_dev);
			gettimeofday(&display_tv[0][1],NULL);
		}
		
		pthread_mutex_unlock(&sdl2_mutex);
	}
	printf("display 0 end\r\n");
	return (void*)0;
}

void* task_display_1(void* args)
{
	sdl2_display_info* sdl2_dev = (sdl2_display_info*)args;
	while(1)
	{
		sem_wait(&display_sem[1]);
		if((pthread_kill(tid_decode[1],0) != 0) || (stopAllTaskFlag == 1))	break;
		pthread_mutex_lock(&sdl2_mutex);
		//sdl2_clear_frame(sdl2_dev);
		sdl2_dev->imageFrameBuffer[1][0] = sdl2_dev->pFrameDecode[1]->data[0];
		sdl2_dev->imageFrameBuffer[1][1] = sdl2_dev->pFrameDecode[1]->data[1];
		sdl2_dev->imageFrameBuffer[1][2] = sdl2_dev->pFrameDecode[1]->data[2];
		sdl2_dev->oneLineByteSize[1][0] = sdl2_dev->pFrameDecode[1]->linesize[0];
		sdl2_dev->oneLineByteSize[1][1] = sdl2_dev->pFrameDecode[1]->linesize[1];
		sdl2_dev->oneLineByteSize[1][2] = sdl2_dev->pFrameDecode[1]->linesize[2];
		
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
			gettimeofday(&display_tv[1][0],NULL);
			sdl2_display_frame(sdl2_dev,1); 
			sdl2_present_frame(sdl2_dev);	
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
#if ENABLE_MMP_DECODE
static void deInit(MppPacket *packet, MppFrame *frame, MppCtx ctx, char *buf, MpiDecLoopData data )
{
    if (packet) {
        mpp_packet_deinit(packet);
        packet = NULL;
    }

    if (frame) {
        mpp_frame_deinit(frame);
        frame = NULL;
    }

    if (ctx) {
        mpp_destroy(ctx);
        ctx = NULL;
    }

/*
    if (buf) {
        mpp_free(buf);
        buf = NULL;
    }*/


    if (data.pkt_grp) {
        mpp_buffer_group_put(data.pkt_grp);
        data.pkt_grp = NULL;
    }

    if (data.frm_grp) {
        mpp_buffer_group_put(data.frm_grp);
        data.frm_grp = NULL;
    }
}

static int init_mmp(sdl2_display_info* sdl2_dev,int inputCh)
{
	int ret = MPP_OK;

	printf("[%d]init mmp decode\r\n",inputCh);
//	sdl2_dev->pixformat[inputCh] = SDL_PIXELFORMAT_NV12;

	sdl2_dev->mmp_ctx[inputCh] = NULL;
	sdl2_dev->mmp_mpi[inputCh] = NULL;

	sdl2_dev->mmp_packet[inputCh] = NULL;
	sdl2_dev->mmp_frame[inputCh]  = NULL;

	sdl2_dev->mmp_mpi_cmd[inputCh] = MPP_CMD_BASE;
	sdl2_dev->mmp_param[inputCh]	= NULL;
	sdl2_dev->mmp_need_split[inputCh] = 1;

	//sdl2_dev->mmp_width[ch] = 2560;
	//sdl2_dev->mmp_height[ch] = 1440;
	sdl2_dev->mmp_width[inputCh] = sdl2_dev->pCodecCtx[inputCh]->width;
	sdl2_dev->mmp_height[inputCh] = sdl2_dev->pCodecCtx[inputCh]->height;
	sdl2_dev->mmp_type[inputCh] = MPP_VIDEO_CodingAVC;

	// resources
	sdl2_dev->mmp_buf[inputCh] = NULL;
	sdl2_dev->mmp_packet_size[inputCh] = 8*1024;
	sdl2_dev->mmp_pkt_buf[inputCh] = NULL;
	sdl2_dev->mmp_frm_buf[inputCh] = NULL;

	sdl2_dev->mmp_data[inputCh];  

	
	memset(&sdl2_dev->mmp_data[inputCh], 0, sizeof(MpiDecLoopData));

	// decoder demo
	ret = mpp_create(&sdl2_dev->mmp_ctx[inputCh], &sdl2_dev->mmp_mpi[inputCh]);

	if (MPP_OK != ret) {
		printf("mpp_create failed\n");
		deInit(	&sdl2_dev->mmp_packet[inputCh], 
				&sdl2_dev->mmp_frame[inputCh],
				sdl2_dev->mmp_ctx[inputCh],
				sdl2_dev->mmp_buf[inputCh],
				sdl2_dev->mmp_data[inputCh]);
		return 1;
	}

	// NOTE: decoder split mode need to be set before init
	sdl2_dev->mmp_mpi_cmd[inputCh] = MPP_DEC_SET_PARSER_SPLIT_MODE;
	sdl2_dev->mmp_param[inputCh] = &sdl2_dev->mmp_need_split[inputCh];
	
	ret = sdl2_dev->mmp_mpi[inputCh]->control(  sdl2_dev->mmp_ctx[inputCh], 
												sdl2_dev->mmp_mpi_cmd[inputCh],
												sdl2_dev->mmp_param[inputCh]);
	if (MPP_OK != ret) 
	{
		printf("mpi->control failed\n");
		deInit(	&sdl2_dev->mmp_packet[inputCh], 
				&sdl2_dev->mmp_frame[inputCh],
				sdl2_dev->mmp_ctx[inputCh],
				sdl2_dev->mmp_buf[inputCh],
				sdl2_dev->mmp_data[inputCh]);
		return 1;
	}

	sdl2_dev->mmp_mpi_cmd[inputCh] = MPP_SET_INPUT_BLOCK;
	sdl2_dev->mmp_param[inputCh] = &sdl2_dev->mmp_need_split[inputCh];
	ret = sdl2_dev->mmp_mpi[inputCh]->control(	sdl2_dev->mmp_ctx[inputCh], 
												sdl2_dev->mmp_mpi_cmd[inputCh], 
												sdl2_dev->mmp_param[inputCh]);
	if (MPP_OK != ret) 
	{
		printf("mpi->control failed\n");
		deInit(	&sdl2_dev->mmp_packet[inputCh], 
				&sdl2_dev->mmp_frame[inputCh],
				sdl2_dev->mmp_ctx[inputCh],
				sdl2_dev->mmp_buf[inputCh],
				sdl2_dev->mmp_data[inputCh]);
		return 1;
	}

	ret = mpp_init(sdl2_dev->mmp_ctx[inputCh], MPP_CTX_DEC, sdl2_dev->mmp_type[inputCh]);
	if (MPP_OK != ret) 
	{
		printf("mpp_init failed\n");
		deInit(	&sdl2_dev->mmp_packet[inputCh], 
				&sdl2_dev->mmp_frame[inputCh],
				sdl2_dev->mmp_ctx[inputCh],
				sdl2_dev->mmp_buf[inputCh],
				sdl2_dev->mmp_data[inputCh]);
		return 1;
	}


	sdl2_dev->mmp_data[inputCh].ctx            = sdl2_dev->mmp_ctx[inputCh];
    sdl2_dev->mmp_data[inputCh].mpi            = sdl2_dev->mmp_mpi[inputCh];
    sdl2_dev->mmp_data[inputCh].eos            = 0;
    sdl2_dev->mmp_data[inputCh].packet_size    = sdl2_dev->mmp_packet_size[inputCh];
    sdl2_dev->mmp_data[inputCh].frame          = sdl2_dev->mmp_frame[inputCh];
    sdl2_dev->mmp_data[inputCh].frame_count    = 0;

	return 0;

}


static char mpp_pool_u[3000*2000];
static char mpp_pool_v[3000*2000];


void dump_mpp_frame_to_file(MppFrame frame, sdl2_display_info* sdl2_dev,int inputCh)
{
    RK_U32 width    = 0;
    RK_U32 height   = 0;
    RK_U32 h_stride = 0;
    RK_U32 v_stride = 0;
	
    MppBuffer buffer    = NULL;
    RK_U8 *base = NULL;
 
    width    = mpp_frame_get_width(frame);
    height   = mpp_frame_get_height(frame);
    h_stride = mpp_frame_get_hor_stride(frame);
    v_stride = mpp_frame_get_ver_stride(frame);
    buffer   = mpp_frame_get_buffer(frame);
	
    base = (RK_U8 *)mpp_buffer_get_ptr(buffer);
    RK_U32 buf_size = mpp_frame_get_buf_size(frame);
    size_t base_length = mpp_buffer_get_size(buffer);
  /*  printf("base_length = %ld\n",base_length);
	printf("width = %d\n",width);
	printf("height = %d\n",height);
	printf("h_stride = %d\n",h_stride);
	printf("v_stride = %d\n",v_stride);
	printf("fmt = %d\r\n",mpp_frame_get_fmt(frame));*/
 
    RK_U32 i,j;
    RK_U8 *base_y = base;
    RK_U8 *base_c = base + h_stride * v_stride;
	int idx = 0;
	
	//保存为YUV420p格式

	sdl2_dev->pFrameDecode[inputCh]->linesize[0] = width;
	sdl2_dev->pFrameDecode[inputCh]->linesize[1] = width/2;
	sdl2_dev->pFrameDecode[inputCh]->linesize[2] = width/2;
#if 0
    for(i = 0; i < height; i++, base_y += h_stride)
    {
       // fwrite(base_y, 1, width, fp);
    }
#endif

	sdl2_dev->pFrameDecode[inputCh]->data[0] = base_y;
	//到时候如果真的要启动mpp解码，这部分还需要更改，要做多个内存池进行拷贝
	//否则当前这个内存池被显示线程使用的时候，可能下一帧解码又更改了内存池的数据
	//上锁会影响效率，需要多个内存池
    for(idx = 0,i = 0,j = 1; i < height * width / 2; i+=2,j+=2,idx++)
	{
	    //fwrite((base_c + i), 1, 1, fp);
		mpp_pool_u[idx] = *(base_c + i);
		mpp_pool_v[idx] = *(base_c + j);
    }
	sdl2_dev->pFrameDecode[inputCh]->data[1]  = mpp_pool_u;
	sdl2_dev->pFrameDecode[inputCh]->data[2]  = mpp_pool_v;
}





int decode_simple(MpiDecLoopData *data, AVPacket *av_packet,sdl2_display_info* sdl2_dev,int inputCh)
{
    RK_U32 pkt_done = 0;
    RK_U32 pkt_eos  = 0;
    RK_U32 err_info = 0;
    MPP_RET ret = MPP_OK;
    MppCtx ctx  = data->ctx;
    MppApi *mpi = data->mpi;
    // char   *buf = data->buf;
    MppPacket packet = NULL;
    MppFrame  frame  = NULL;
    size_t read_size = 0;
    size_t packet_size = data->packet_size;


    ret = mpp_packet_init(&packet, av_packet->data, av_packet->size);
    mpp_packet_set_pts(packet, av_packet->pts);



    do {
        RK_S32 times = 5;
        // send the packet first if packet is not done
        if (!pkt_done) {
            ret = mpi->decode_put_packet(ctx, packet);
            if (MPP_OK == ret)
                pkt_done = 1;
        }

        // then get all available frame and release
        do {
            RK_S32 get_frm = 0;
            RK_U32 frm_eos = 0;

            try_again:
            ret = mpi->decode_get_frame(ctx, &frame);
            if (MPP_ERR_TIMEOUT == ret) {
                if (times > 0) {
                    times--;
                    usleep(2000);
                    goto try_again;
                }
                printf("decode_get_frame failed too much time\n");
            }
            if (MPP_OK != ret) {
                printf("decode_get_frame failed ret %d\n", ret);
                break;
            }
		
            if (frame) {
                if (mpp_frame_get_info_change(frame)) {
                    RK_U32 width = mpp_frame_get_width(frame);
                    RK_U32 height = mpp_frame_get_height(frame);
                    RK_U32 hor_stride = mpp_frame_get_hor_stride(frame);
                    RK_U32 ver_stride = mpp_frame_get_ver_stride(frame);
                    RK_U32 buf_size = mpp_frame_get_buf_size(frame);

                    printf("decode_get_frame get info changed found\n");
                    printf("decoder require buffer w:h [%d:%d] stride [%d:%d] buf_size %d",
                            width, height, hor_stride, ver_stride, buf_size);

                    ret = mpp_buffer_group_get_internal(&data->frm_grp, MPP_BUFFER_TYPE_ION);
                    if (ret) {
                        printf("get mpp buffer group  failed ret %d\n", ret);
                        break;
                    }
                    mpi->control(ctx, MPP_DEC_SET_EXT_BUF_GROUP, data->frm_grp);

                    mpi->control(ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
                } else {
                    err_info = mpp_frame_get_errinfo(frame) | mpp_frame_get_discard(frame);
                    if (err_info) {
                        printf("decoder_get_frame get err info:%d discard:%d.\n",
                                mpp_frame_get_errinfo(frame), mpp_frame_get_discard(frame));
                    }
                    data->frame_count++;
                    
                   if (!err_info){
				   		printf("decode_get_frame get frame %d\n", data->frame_count);
						dump_mpp_frame_to_file(frame, sdl2_dev,inputCh);
                   }
                }
                frm_eos = mpp_frame_get_eos(frame);
                mpp_frame_deinit(&frame);

                frame = NULL;
                get_frm = 1;
            }

            // try get runtime frame memory usage
            if (data->frm_grp) {
                size_t usage = mpp_buffer_group_usage(data->frm_grp);
                if (usage > data->max_usage)
                    data->max_usage = usage;
            }

            // if last packet is send but last frame is not found continue
            if (pkt_eos && pkt_done && !frm_eos) {
                usleep(10000);
                continue;
            }

            if (frm_eos) {
                printf("found last frame\n");
                break;
            }

            if (data->frame_num > 0 && data->frame_count >= data->frame_num) {
                data->eos = 1;
                break;
            }

            if (get_frm)
                continue;
            break;
        } while (1);

        if (data->frame_num > 0 && data->frame_count >= data->frame_num) {
            data->eos = 1;
            printf("reach max frame number %d\n", data->frame_count);
            break;
        }

        if (pkt_done)
            break;

        /*
         * why sleep here:
         * mpi->decode_put_packet will failed when packet in internal queue is
         * full,waiting the package is consumed .Usually hardware decode one
         * frame which resolution is 1080p needs 2 ms,so here we sleep 3ms
         * * is enough.
         */
        usleep(3000);
    } while (1);
    mpp_packet_deinit(&packet);

    return ret;
}
#endif

static int init_device(sdl2_display_info* sdl2_dev)
{
	int m;
	for(m=0;m<sdl2_dev->inputSourceNum;m++)
	{
		sdl2_dev->pixformat[m] = SDL_PIXELFORMAT_IYUV;
		sdl2_dev->components[m] = 3;
		sdl2_dev->v_stream_idx[m] = -1;
		sdl2_dev->pFormatCtx[m] = avformat_alloc_context();
		//2.打开输入视频文件
	    if (avformat_open_input(&sdl2_dev->pFormatCtx[m], (char*)sdl2_dev->inputFile[m], NULL, NULL) != 0)
	    {
	        printf("%s","open input file faild\r\n");
	        return 1;
	    }

		 //3.获取视频文件信息
	    if (avformat_find_stream_info(sdl2_dev->pFormatCtx[m],NULL) < 0)
	    {
	        printf("%s","get video info faild\r\n");
	        return 1;
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
	        return 1;
	    }



		
	    sdl2_dev->pCodecCtx[m] = avcodec_alloc_context3(NULL);
		if (sdl2_dev->pCodecCtx[m] == NULL) {
			printf("%s","can't alloc pCodecCtx\r\n");
			return 1;
		}
		avcodec_parameters_to_context(sdl2_dev->pCodecCtx[m], sdl2_dev->pFormatCtx[m]->streams[sdl2_dev->v_stream_idx[m]]->codecpar);	//新版需要这么转化
		
	    //4.根据编解码上下文中的编码id查找对应的解码
	    
	    sdl2_dev->pCodec[m] = avcodec_find_decoder(sdl2_dev->pCodecCtx[m]->codec_id);
	    if (sdl2_dev->pCodec[m] == NULL)
	    {
	        printf("%s","can't find codec\r\n");
	        return 1;
	    }

	    //5.打开解码器
	    if (avcodec_open2(sdl2_dev->pCodecCtx[m],sdl2_dev->pCodec[m],NULL)<0)
	    {
	        printf("%s","can't open codec\r\n");
	        return 1;
	    }

		sdl2_dev->imageSize_width[m] = sdl2_dev->pCodecCtx[m]->width;
		sdl2_dev->imageSize_height[m] = sdl2_dev->pCodecCtx[m]->height;
		//sdl2_dev->oneLineByteSize[m] = sdl2_dev->pCodecCtx[m]->width * sdl2_dev->components[m];
		
		if((sdl2_dev->pFormatCtx[m]->streams[sdl2_dev->v_stream_idx[m]]->avg_frame_rate.num != 0) && (sdl2_dev->pFormatCtx[m]->streams[sdl2_dev->v_stream_idx[m]]->avg_frame_rate.den != 0))
		{
			sdl2_dev->frame_rate[m] = (double)sdl2_dev->pFormatCtx[m]->streams[sdl2_dev->v_stream_idx[m]]->avg_frame_rate.num/sdl2_dev->pFormatCtx[m]->streams[sdl2_dev->v_stream_idx[m]]->avg_frame_rate.den;
			sdl2_dev->frame_time[m] = (int)(1000000/sdl2_dev->frame_rate[m]);
		}

		//输出视频信息
		printf("[%d]video codec id: %d\r\n",m,sdl2_dev->pCodecCtx[m]->codec_id);
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
	    sdl2_dev->pFrameDecode[m] = av_frame_alloc();//用于存储一帧解码后，由原始像素格式转换成rgb后的数据


		
	    //只有指定了AVFrame的像素格式、画面大小才能真正分配内存
	    //缓冲区分配内存
	    sdl2_dev->out_buffer[m] = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, sdl2_dev->pCodecCtx[m]->width, sdl2_dev->pCodecCtx[m]->height,1));
	    //初始化缓冲区
	    av_image_fill_arrays(sdl2_dev->pFrameDecode[m]->data,sdl2_dev->pFrameDecode[m]->linesize,sdl2_dev->out_buffer[m],AV_PIX_FMT_YUV420P,sdl2_dev->pCodecCtx[m]->width, sdl2_dev->pCodecCtx[m]->height,1);
	  
	    //用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
	    sdl2_dev->sws_ctx[m] = sws_getContext(sdl2_dev->pCodecCtx[m]->width,sdl2_dev->pCodecCtx[m]->height,sdl2_dev->pCodecCtx[m]->pix_fmt,
                                sdl2_dev->pCodecCtx[m]->width, sdl2_dev->pCodecCtx[m]->height, AV_PIX_FMT_YUV420P,
                                SWS_BICUBIC, NULL, NULL, NULL);

		#if ENABLE_MMP_DECODE

		if(sdl2_dev->pCodecCtx[m]->codec_id == AV_CODEC_ID_H264)
		{
		   if(init_mmp(sdl2_dev,m))
		   {
				printf("init mmp faild\r\n");
				exit(-1);
		   }
		}
		#endif	
								
	}
	return 0;
}



int main(int argc, char *argv[])
{
	int m;
	
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
					sdl2_dev->inputFile[sdl2_dev->inputSourceNum] = malloc(100);
					strcpy((char*)sdl2_dev->inputFile[sdl2_dev->inputSourceNum],optarg);
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

	if(init_device(sdl2_dev))
	{
		printf("init sdl2 display faild\r\n");
		exit(-1);
	}

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
		#if ENABLE_MMP_DECODE
			if(sdl2_dev->pCodecCtx[m]->codec_id == AV_CODEC_ID_H264)	
			{
				deInit(	&sdl2_dev->mmp_packet[m], 
						&sdl2_dev->mmp_frame[m],
						sdl2_dev->mmp_ctx[m],
						sdl2_dev->mmp_buf[m],
						sdl2_dev->mmp_data[m]);
			}
		#endif
		sws_freeContext(sdl2_dev->sws_ctx[m]);
		av_free(sdl2_dev->out_buffer[m]);
		av_frame_free(&sdl2_dev->pFrameDecode[m]);
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









