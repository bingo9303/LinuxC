#ifndef SDL2_DISPLAY_H
#define SDL2_DISPLAY_H



#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
#include <SDL2/SDL.h>


#define MAX_CH_NUM		5


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
	char alphaFlag[5];				//1-淡出  2-淡入0-不做处理
} OutLayerSetting;


typedef struct _sdl2_display_info_multiple_input
{
/* 输出参数 */
	int outputChNum;
	char* windowsName;
	int windowsSize_width;
	int windowsSize_height;

/* 输入参数 */
	int inputSourceNum;	
	int imageSize_width[MAX_CH_NUM];
	int imageSize_height[MAX_CH_NUM];
	char* imageFrameBuffer[MAX_CH_NUM];		//一帧图像的buff	
	int pixformat[MAX_CH_NUM];
	int components[MAX_CH_NUM];
	unsigned int oneLineByteSize[MAX_CH_NUM];
	int frame_time[MAX_CH_NUM];
	double frame_rate[MAX_CH_NUM];
/* 图层参数 */
	LayerSetting layerInfo[MAX_CH_NUM];
	OutLayerSetting outLayerAlpha;
	int alphaTime;

/*  SDL2 参数 */
	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture[MAX_CH_NUM];
}sdl2_display_info;


int init_sdl2_display(sdl2_display_info* sdl2_dev);
void quit_sdl2_display(sdl2_display_info* sdl2_dev);
void sdl2_display_frame(sdl2_display_info* sdl2_dev,int layerCh);
void sdl2_clear_frame(sdl2_display_info* sdl2_dev);
void sdl2_present_frame(sdl2_display_info* sdl2_dev);
void sdl2_SetAlpha(sdl2_display_info* sdl2_dev,int layerCh,unsigned char value);
int sdl2_GetAlpha(sdl2_display_info* sdl2_dev,int layerCh,unsigned char* value);







#endif

