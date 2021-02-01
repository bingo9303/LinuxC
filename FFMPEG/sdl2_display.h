#ifndef SDL2_DISPLAY_H
#define SDL2_DISPLAY_H



#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>



typedef struct _sdl2_display_info
{
	int windowsSize_width;
	int windowsSize_height;
	
	int imageSize_width;
	int imageSize_height;
	

	char* imageFrameBuffer;		//Ò»Ö¡Í¼ÏñµÄbuff

	char* windowsName;
	int pixformat;

	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
}sdl2_display_info;



void usage(char *msg);
int init_sdl2_display_one_layer(sdl2_display_info* sdl2_dev);
void quit_sdl2_display_one_layer(void);
void sdl2_display_frame(sdl2_display_info* sdl2_dev,SDL_Rect* crop,SDL_Rect* scale);




#endif

