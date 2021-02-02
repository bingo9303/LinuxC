#ifndef SDL2_DISPLAY_H
#define SDL2_DISPLAY_H



#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
#include <SDL2/SDL.h>


#define MAX_LAYER_NUM		5

typedef struct _sdl2_display_info
{
	int windowsSize_width;
	int windowsSize_height;
	
	int imageSize_width;
	int imageSize_height;
	int components;
	int layer;
	int currentLayer;
	int alphaTime;

	char* imageFrameBuffer;		//一帧图像的buff

	char* windowsName;
	int pixformat;

	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture** sdlTexture;
	
}sdl2_display_info;



typedef struct _sdl2_display_info_multiple_input
{
	int windowsSize_width;
	int windowsSize_height;
	
	int imageSize_width[MAX_LAYER_NUM];
	int imageSize_height[MAX_LAYER_NUM];
	int components[MAX_LAYER_NUM];
	int layer;
	int currentLayer;

	char* imageFrameBuffer[MAX_LAYER_NUM];		//一帧图像的buff

	char* windowsName;
	int pixformat[MAX_LAYER_NUM];

	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture** sdlTexture;
	
}sdl2_display_info_multiple_input;




int init_sdl2_display_one_input(sdl2_display_info* sdl2_dev);
void quit_sdl2_display_one_input(sdl2_display_info* sdl2_dev);
void sdl2_display_frame(sdl2_display_info* sdl2_dev,SDL_Rect* crop,SDL_Rect* scale);
void sdl2_clear_frame(sdl2_display_info* sdl2_dev);
void sdl2_present_frame(sdl2_display_info* sdl2_dev);
void sdl2_SetAlpha(sdl2_display_info* sdl2_dev,int layer,unsigned char value);
int sdl2_GetAlpha(sdl2_display_info* sdl2_dev,int layer,unsigned char* value);

int init_sdl2_display_multiple_input(sdl2_display_info_multiple_input* sdl2_dev);
void sdl2_display_frame_multiple_input(sdl2_display_info_multiple_input* sdl2_dev,SDL_Rect* crop,SDL_Rect* scale);
void quit_sdl2_display_multiple_input(sdl2_display_info_multiple_input* sdl2_dev);
void sdl2_clear_frame_multiple_input(sdl2_display_info_multiple_input* sdl2_dev);
void sdl2_present_frame_multiple_input(sdl2_display_info_multiple_input* sdl2_dev);



#endif

