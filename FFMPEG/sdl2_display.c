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



int init_sdl2_display_one_input(sdl2_display_info* sdl2_dev)
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

	
	sdl2_dev->sdlTexture = malloc(sizeof(SDL_Texture*)*sdl2_dev->layer);
	
	for(i=0;i<sdl2_dev->layer;i++)
	{
		sdl2_dev->sdlTexture[i] = SDL_CreateTexture(sdl2_dev->sdlRenderer, sdl2_dev->pixformat, SDL_TEXTUREACCESS_STREAMING, sdl2_dev->imageSize_width, sdl2_dev->imageSize_height);
	}
	
    return (0);
}


void quit_sdl2_display_one_input(sdl2_display_info* sdl2_dev)
{
	SDL_Quit();
	free(sdl2_dev->sdlTexture);
}


int init_sdl2_display_multiple_input(sdl2_display_info_multiple_input* sdl2_dev)
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

	
	sdl2_dev->sdlTexture = malloc(sizeof(SDL_Texture*)*sdl2_dev->layer);
	
	for(i=0;i<sdl2_dev->layer;i++)
	{
		sdl2_dev->sdlTexture[i] = SDL_CreateTexture(sdl2_dev->sdlRenderer, sdl2_dev->pixformat[i], SDL_TEXTUREACCESS_STREAMING, sdl2_dev->imageSize_width[i], sdl2_dev->imageSize_height[i]);
	}
	
    return (0);
}


void quit_sdl2_display_multiple_input(sdl2_display_info_multiple_input* sdl2_dev)
{
	SDL_Quit();
	free(sdl2_dev->sdlTexture);
}



void sdl2_display_frame(sdl2_display_info* sdl2_dev,SDL_Rect* crop,SDL_Rect* scale)
{
	unsigned int oneLineByteSize;
	SDL_Rect crop_Rect,scale_Rect;
	
	oneLineByteSize = sdl2_dev->imageSize_width * sdl2_dev->components;
	if(crop == NULL)
	{
		crop_Rect.x = 0;
        crop_Rect.y = 0;
        crop_Rect.w = sdl2_dev->imageSize_width;
        crop_Rect.h = sdl2_dev->imageSize_height;
	}
	else
	{
		memcpy(&crop_Rect,crop,sizeof(SDL_Rect));
	}

	if(scale == NULL)
	{
		scale_Rect.x = 0;
        scale_Rect.y = 0;
        scale_Rect.w = sdl2_dev->windowsSize_width;		
        scale_Rect.h = sdl2_dev->windowsSize_height;		
	}
	else
	{
		memcpy(&scale_Rect,scale,sizeof(SDL_Rect));
	}

	SDL_UpdateTexture(sdl2_dev->sdlTexture[sdl2_dev->currentLayer], NULL, sdl2_dev->imageFrameBuffer, oneLineByteSize);
	//SDL_RenderClear(sdl2_dev->sdlRenderer);
    SDL_RenderCopy(sdl2_dev->sdlRenderer, sdl2_dev->sdlTexture[sdl2_dev->currentLayer], &crop_Rect, &scale_Rect);	//crop_Rect如果为NULL，则默认整个画面的crop
   // SDL_RenderPresent(sdl2_dev->sdlRenderer);
}




void sdl2_display_frame_multiple_input(sdl2_display_info_multiple_input* sdl2_dev,SDL_Rect* crop,SDL_Rect* scale)
{
	unsigned int oneLineByteSize;
	SDL_Rect crop_Rect,scale_Rect;
	int currentLayer = sdl2_dev->currentLayer;
	
	oneLineByteSize = sdl2_dev->imageSize_width[currentLayer] * sdl2_dev->components[currentLayer];
	if(crop == NULL)
	{
		crop_Rect.x = 0;
        crop_Rect.y = 0;
        crop_Rect.w = sdl2_dev->imageSize_width[currentLayer];
        crop_Rect.h = sdl2_dev->imageSize_height[currentLayer];
	}
	else
	{
		memcpy(&crop_Rect,crop,sizeof(SDL_Rect));
	}

	if(scale == NULL)
	{
		scale_Rect.x = 0;
        scale_Rect.y = 0;
        scale_Rect.w = sdl2_dev->windowsSize_width;		
        scale_Rect.h = sdl2_dev->windowsSize_height;		
	}
	else
	{
		memcpy(&scale_Rect,scale,sizeof(SDL_Rect));
	}

	SDL_UpdateTexture(sdl2_dev->sdlTexture[currentLayer], NULL, sdl2_dev->imageFrameBuffer[currentLayer], oneLineByteSize);
	//SDL_RenderClear(sdl2_dev->sdlRenderer);
    SDL_RenderCopy(sdl2_dev->sdlRenderer, sdl2_dev->sdlTexture[currentLayer], &crop_Rect, &scale_Rect);	//crop_Rect如果为NULL，则默认整个画面的crop
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


void sdl2_clear_frame_multiple_input(sdl2_display_info_multiple_input* sdl2_dev)
{
	SDL_RenderClear(sdl2_dev->sdlRenderer);
}

void sdl2_present_frame_multiple_input(sdl2_display_info_multiple_input* sdl2_dev)
{
	SDL_RenderPresent(sdl2_dev->sdlRenderer);
}


