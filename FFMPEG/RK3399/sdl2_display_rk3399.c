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


#include "sdl2_display_rk3399.h"



int init_sdl2_display(sdl2_display_info* sdl2_dev)
{
	int i;
	if(sdl2_dev->windowsName == NULL) sdl2_dev->windowsName = "Simplest Video Play SDL2";
		
	if (SDL_Init(SDL_INIT_VIDEO)) {
		  printf("Could not initialize SDL - %s\n", SDL_GetError());
		  return -1;
	}

    //SDL 2.0 Support for multiple windows
   	sdl2_dev->screen = SDL_CreateWindow(sdl2_dev->windowsName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        sdl2_dev->windowsSize_width, sdl2_dev->windowsSize_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	if (!sdl2_dev->screen) {
        printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
        return -1;
    }

	sdl2_dev->sdlRenderer = SDL_CreateRenderer(sdl2_dev->screen, -1, 0);

	for(i=0;i<sdl2_dev->outputChNum;i++)
	{
		LayerSetting* layer = &sdl2_dev->layerInfo[i];
		sdl2_dev->sdlTexture[i] = SDL_CreateTexture(sdl2_dev->sdlRenderer, sdl2_dev->pixformat[layer->inputPort], SDL_TEXTUREACCESS_STREAMING, sdl2_dev->imageSize_width[layer->inputPort], sdl2_dev->imageSize_height[layer->inputPort]);
		SDL_SetTextureBlendMode(sdl2_dev->sdlTexture[i],SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod(sdl2_dev->sdlTexture[i],sdl2_dev->outLayerAlpha.alpha[i]);
	}
	
    return (0);
}


void quit_sdl2_display(sdl2_display_info* sdl2_dev)
{
	SDL_Quit();
}


void sdl2_display_frame(sdl2_display_info* sdl2_dev,int layerCh)
{
	LayerSetting* layer = &sdl2_dev->layerInfo[layerCh];

	if(sdl2_dev->pixformat[layer->inputPort] == SDL_PIXELFORMAT_IYUV)
	{
		SDL_UpdateYUVTexture(sdl2_dev->sdlTexture[layerCh], NULL,
                             sdl2_dev->imageFrameBuffer[layer->inputPort][0], sdl2_dev->oneLineByteSize[layer->inputPort][0],
                             sdl2_dev->imageFrameBuffer[layer->inputPort][1], sdl2_dev->oneLineByteSize[layer->inputPort][1],
                             sdl2_dev->imageFrameBuffer[layer->inputPort][2], sdl2_dev->oneLineByteSize[layer->inputPort][2]);
	}
	else
	{
		SDL_UpdateTexture(sdl2_dev->sdlTexture[layerCh], NULL, sdl2_dev->imageFrameBuffer[layer->inputPort][0], sdl2_dev->oneLineByteSize[layer->inputPort][0]);
	}
	//SDL_RenderClear(sdl2_dev->sdlRenderer);
    SDL_RenderCopy(sdl2_dev->sdlRenderer, sdl2_dev->sdlTexture[layerCh], &layer->crop, &layer->scale);	//crop_Rect如果为NULL，则默认整个画面的crop
   // SDL_RenderPresent(sdl2_dev->sdlRenderer);
}

void sdl2_clear_frame(sdl2_display_info* sdl2_dev)
{
	SDL_RenderClear(sdl2_dev->sdlRenderer);
}

void sdl2_present_frame(sdl2_display_info* sdl2_dev)
{
	SDL_RenderPresent(sdl2_dev->sdlRenderer);
}


void sdl2_SetAlpha(sdl2_display_info* sdl2_dev,int layerCh,unsigned char value)
{
	SDL_SetTextureAlphaMod(sdl2_dev->sdlTexture[layerCh],value);
}

int sdl2_GetAlpha(sdl2_display_info* sdl2_dev,int layerCh,unsigned char* value)
{
	return SDL_GetTextureAlphaMod(sdl2_dev->sdlTexture[layerCh],value);
}


