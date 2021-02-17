#include "define.h"


int init_sdl2_display(struct _uvcCameraDev* dev)
{
	int i;
	if(dev->windowsName == NULL) dev->windowsName = "RgbLink Uvc Camera Video";
		
	if (SDL_Init(SDL_INIT_VIDEO)) {
		  printf("Could not initialize SDL - %s\n", SDL_GetError());
		  exit (EXIT_FAILURE); 
	}

   	dev->screen = SDL_CreateWindow(dev->windowsName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        dev->windowsSize_width, dev->windowsSize_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	if (!dev->screen) {
        printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
        exit (EXIT_FAILURE);
    }

	dev->sdlRenderer = SDL_CreateRenderer(dev->screen, -1, SDL_RENDERER_ACCELERATED);

	for(i=0;i<dev->uvc_ch_num;i++)
	{
		LayerSetting* layer = &dev->layerInfo[i];
		dev->sdlTexture[i] = SDL_CreateTexture(dev->sdlRenderer, dev->pixformat[layer->inputPort], SDL_TEXTUREACCESS_STREAMING, dev->uvc_input_width[layer->inputPort], dev->uvc_input_height[layer->inputPort]);
		SDL_SetTextureBlendMode(dev->sdlTexture[i],SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod(dev->sdlTexture[i],dev->outLayerAlpha.alpha[i]);
	}
	
    return (0);
}


void quit_sdl2_display(struct _uvcCameraDev* dev)
{
	SDL_Quit();
}


void sdl2_display_frame(struct _uvcCameraDev* dev,int layerCh)
{
	LayerSetting* layer = &dev->layerInfo[layerCh];

	if(dev->pixformat[layer->inputPort] == SDL_PIXELFORMAT_IYUV)
	{
		SDL_UpdateYUVTexture(dev->sdlTexture[layerCh], NULL,
                             dev->imageFrameBuffer[layer->inputPort][0], dev->oneLineByteSize[layer->inputPort][0],
                             dev->imageFrameBuffer[layer->inputPort][1], dev->oneLineByteSize[layer->inputPort][1],
                             dev->imageFrameBuffer[layer->inputPort][2], dev->oneLineByteSize[layer->inputPort][2]);
	}
	else
	{
		SDL_UpdateTexture(dev->sdlTexture[layerCh], NULL, dev->imageFrameBuffer[layer->inputPort][0], dev->oneLineByteSize[layer->inputPort][0]);
	}
    SDL_RenderCopy(dev->sdlRenderer, dev->sdlTexture[layerCh], &layer->crop, &layer->scale);
}

void sdl2_clear_frame(struct _uvcCameraDev* dev)
{
	SDL_RenderClear(dev->sdlRenderer);
}

void sdl2_present_frame(struct _uvcCameraDev* dev)
{
	SDL_RenderPresent(dev->sdlRenderer);
}


void sdl2_SetAlpha(struct _uvcCameraDev* dev,int layerCh,int value)
{
	unsigned char temp = value & 0xFF;
	SDL_SetTextureAlphaMod(dev->sdlTexture[layerCh],temp);
}

int sdl2_GetAlpha(struct _uvcCameraDev* dev,int layerCh,int* value)
{
	unsigned char temp;
	int res;
	res = SDL_GetTextureAlphaMod(dev->sdlTexture[layerCh],&temp);
	*value = temp;
	return res;
}

void* task_clear(void* args)
{
	int layerCh,clearFlag=0;
	struct _uvcCameraDev* dev = (struct _uvcCameraDev*)args;
	int delayTime = (dev->alphaTime*1000)/255;
	
	pid_t pid = gettid();
	pthread_t tid = pthread_self();
	printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);
	
	#if ENABLE_MANUAL_SET_CUP
		char cpuIndex;
		int res;
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
	    for (cpuIndex = 4; cpuIndex < 6; cpuIndex++)
	        CPU_SET(cpuIndex, &cpuset);
		
		res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
		if (res != 0)
		{
			printf("pthread_setaffinity_np faild\r\n");
		}
	#endif
	
	while(1)
	{
		//if(stopAllTaskFlag == 1)	break;
		clearFlag = 0;
		for(layerCh=0;layerCh<dev->uvc_ch_num;layerCh++)
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
			sdl2_clear_frame(dev);
			for(layerCh=0;layerCh<dev->uvc_ch_num;layerCh++)	sdl2_display_frame(dev,layerCh);
			sdl2_present_frame(dev);
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
	struct _uvcCameraDev* dev = (struct _uvcCameraDev*)args;
	int delayTime = (dev->alphaTime*1000)/255;
	int layerCh;

	
	pid_t pid = gettid();
	pthread_t tid = pthread_self();
	printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);
	#if ENABLE_MANUAL_SET_CUP
		char cpuIndex;
		int res;
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
	    for (cpuIndex = 4; cpuIndex < 6; cpuIndex++)
	        CPU_SET(cpuIndex, &cpuset);
		
		res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
		if (res != 0)
		{
			printf("pthread_setaffinity_np faild\r\n");
		}
	#endif

	
	while(1)
	{
		//if(stopAllTaskFlag == 1)	break;
		for(layerCh=0;layerCh<dev->uvc_ch_num;layerCh++)
		{
			if(dev->outLayerAlpha.alphaFlag[layerCh] == 1)
			{
				dev->outLayerAlpha.alpha[layerCh]++;
				sdl2_SetAlpha(dev,layerCh,(dev->outLayerAlpha.alpha[layerCh]&0xFF));
				if(dev->outLayerAlpha.alpha[layerCh] >= 0xFF)
				{
					dev->outLayerAlpha.alphaFlag[layerCh] = 0;
					dev->outLayerAlpha.alpha[layerCh] = 0xFF;
				}
			}
			else if(dev->outLayerAlpha.alphaFlag[layerCh] == 2)
			{
				dev->outLayerAlpha.alpha[layerCh]--;
				sdl2_SetAlpha(dev,layerCh,(dev->outLayerAlpha.alpha[layerCh]&0xFF));
				if(dev->outLayerAlpha.alpha[layerCh] <= 0)	
				{
					dev->outLayerAlpha.alphaFlag[layerCh] = 0;
					dev->outLayerAlpha.alpha[layerCh] = 0;
				}					
			}
		}
		usleep(delayTime);
	}
}







void* task_display_0(void* args)  
{
	struct _uvcCameraDev* dev = (struct _uvcCameraDev*)args;
	
	pid_t pid = gettid();
 	pthread_t tid = pthread_self();
	printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);

	#if ENABLE_MANUAL_SET_CUP
		char cpuIndex;
		int res;
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
	    for (cpuIndex = 0; cpuIndex < 4; cpuIndex++)
	        CPU_SET(cpuIndex, &cpuset);
		
		res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
		if (res != 0)
		{
			printf("pthread_setaffinity_np faild\r\n");
		}
	#endif
	
	while(1)
	{
		sem_wait(&display_sem[0]);
		
		
		pthread_mutex_lock(&sdl2_mutex);
		
		//sdl2_clear_frame(sdl2_dev);//要改，不然会闪烁
	#if ENABLE_DEBUG_BINGO
		printf("3.display_pool_read[0] = %d\r\n",display_pool_read[0]);
	#endif
		dev->imageFrameBuffer[0][0] = (char*)display_pool[0][display_pool_read[0]].addr;
		
		dev->layerInfo[0].crop.x = 0;
		dev->layerInfo[0].crop.y = 0;
		dev->layerInfo[0].crop.w = dev->imageSize_width[0];
		dev->layerInfo[0].crop.h = dev->imageSize_height[0];

		dev->layerInfo[0].scale.x = 0;
       	dev->layerInfo[0].scale.y = 0;
        dev->layerInfo[0].scale.w = dev->windowsSize_width;		
        dev->layerInfo[0].scale.h = dev->windowsSize_height;
		
		if((dev->outLayerAlpha.alphaFlag[0] == 0) && (dev->outLayerAlpha.alpha[0] != 0))
		{
		//static struct timeval decode_tv;
		//gettimeofday(&decode_tv,NULL);
		//printf("sdl 11 %ld\r\n",decode_tv.tv_usec);
			sdl2_display_frame(dev,0);	
			sdl2_present_frame(dev);
		//gettimeofday(&decode_tv,NULL);
		//printf("sdl 22 %ld\r\n",decode_tv.tv_usec);
		}
		display_pool[0][display_pool_read[0]].flag = 0;
		display_pool_read[0] = POS_OFFSET_ADD(display_pool_read[0],1,DISPLAY_BUFFER_FRAME_NUM);
		pthread_mutex_unlock(&sdl2_mutex);
		//usleep(1);
	}
}



void* task_display_1(void* args)  
{
#if 1
	struct _uvcCameraDev* dev = (struct _uvcCameraDev*)args;
	pid_t pid = gettid();
 	pthread_t tid = pthread_self();
	printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);
	#if ENABLE_MANUAL_SET_CUP
		char cpuIndex;
		int res;
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
	    for (cpuIndex = 0; cpuIndex < 4; cpuIndex++)
	        CPU_SET(cpuIndex, &cpuset);
		
		res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
		if (res != 0)
		{
			printf("pthread_setaffinity_np faild\r\n");
		}
	#endif

	while(1)
	{
		sem_wait(&display_sem[1]);
		
		
		pthread_mutex_lock(&sdl2_mutex);
		
		//sdl2_clear_frame(sdl2_dev);//要改，不然会闪烁
	#if ENABLE_DEBUG_BINGO
		printf("3.display_pool_read[1] = %d\r\n",display_pool_read[1]);
	#endif
		dev->imageFrameBuffer[1][0] = (char*)display_pool[1][display_pool_read[1]].addr;
		
		dev->layerInfo[1].crop.x = 0;
		dev->layerInfo[1].crop.y = 0;
		dev->layerInfo[1].crop.w = dev->imageSize_width[1];
		dev->layerInfo[1].crop.h = dev->imageSize_height[1];

		dev->layerInfo[1].scale.x = 0;
       	dev->layerInfo[1].scale.y = 0;
        dev->layerInfo[1].scale.w = dev->windowsSize_width;		
        dev->layerInfo[1].scale.h = dev->windowsSize_height;
		
		if((dev->outLayerAlpha.alphaFlag[1] == 0)&& (dev->outLayerAlpha.alpha[1] != 0))
		{
		//static struct timeval decode_tv;
		//gettimeofday(&decode_tv,NULL);
		//printf("sdl 11 %ld\r\n",decode_tv.tv_usec);
			sdl2_display_frame(dev,1);	
			sdl2_present_frame(dev);
		//gettimeofday(&decode_tv,NULL);
		//printf("sdl 22 %ld\r\n",decode_tv.tv_usec);
		}
		display_pool[1][display_pool_read[1]].flag = 0;
		display_pool_read[1] = POS_OFFSET_ADD(display_pool_read[1],1,DISPLAY_BUFFER_FRAME_NUM);
		pthread_mutex_unlock(&sdl2_mutex);
		//usleep(1);
	}
#endif
}





void display_0(struct _uvcCameraDev* dev)
{
	pthread_mutex_lock(&sdl2_mutex);
		
	//sdl2_clear_frame(sdl2_dev);//要改，不然会闪烁
//	printf("3.display_pool_read[0] = %d\r\n",display_pool_read[0]);
	
	dev->imageFrameBuffer[0][0] = (char*)display_pool[0][display_pool_read[0]].addr;
	
	dev->layerInfo[0].crop.x = 0;
	dev->layerInfo[0].crop.y = 0;
	dev->layerInfo[0].crop.w = dev->imageSize_width[0];
	dev->layerInfo[0].crop.h = dev->imageSize_height[0];

	dev->layerInfo[0].scale.x = 0;
   	dev->layerInfo[0].scale.y = 0;
    dev->layerInfo[0].scale.w = dev->windowsSize_width;		
    dev->layerInfo[0].scale.h = dev->windowsSize_height;
	
	if((dev->outLayerAlpha.alphaFlag[0] == 0) && (dev->outLayerAlpha.alpha[0] != 0))
	{
	//static struct timeval decode_tv;
	//gettimeofday(&decode_tv,NULL);
	//printf("sdl 11 %ld\r\n",decode_tv.tv_usec);
		sdl2_display_frame(dev,0);	
		sdl2_present_frame(dev);
	//gettimeofday(&decode_tv,NULL);
	//printf("sdl 22 %ld\r\n",decode_tv.tv_usec);
	}
	display_pool[0][display_pool_read[0]].flag = 0;
	display_pool_read[0] = POS_OFFSET_ADD(display_pool_read[0],1,DISPLAY_BUFFER_FRAME_NUM);
	pthread_mutex_unlock(&sdl2_mutex);
}


void display_1(struct _uvcCameraDev* dev)
{
	pthread_mutex_lock(&sdl2_mutex);
		
	//sdl2_clear_frame(sdl2_dev);//要改，不然会闪烁
//	printf("3.display_pool_read[0] = %d\r\n",display_pool_read[0]);
	
	dev->imageFrameBuffer[1][0] = (char*)display_pool[1][display_pool_read[1]].addr;
	
	dev->layerInfo[1].crop.x = 0;
	dev->layerInfo[1].crop.y = 0;
	dev->layerInfo[1].crop.w = dev->imageSize_width[1];
	dev->layerInfo[1].crop.h = dev->imageSize_height[1];

	dev->layerInfo[1].scale.x = 0;
   	dev->layerInfo[1].scale.y = 0;
    dev->layerInfo[1].scale.w = dev->windowsSize_width;		
    dev->layerInfo[1].scale.h = dev->windowsSize_height;
	
	if((dev->outLayerAlpha.alphaFlag[1] == 0) && (dev->outLayerAlpha.alpha[1] != 0))
	{
	//static struct timeval decode_tv;
	//gettimeofday(&decode_tv,NULL);
	//printf("sdl 11 %ld\r\n",decode_tv.tv_usec);
		sdl2_display_frame(dev,1);	
		sdl2_present_frame(dev);
	//gettimeofday(&decode_tv,NULL);
	//printf("sdl 22 %ld\r\n",decode_tv.tv_usec);
	}
	display_pool[1][display_pool_read[1]].flag = 0;
	display_pool_read[1] = POS_OFFSET_ADD(display_pool_read[1],1,DISPLAY_BUFFER_FRAME_NUM);
	pthread_mutex_unlock(&sdl2_mutex);
}









