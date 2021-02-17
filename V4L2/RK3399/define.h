#ifndef _DEFINE_H
#define _DEFINE_H



#include <stdio.h>   
#include <stdlib.h>   
#include <string.h>   
#include <assert.h>   
#include <getopt.h>             
#include <fcntl.h>              
#include <unistd.h>   
#include <errno.h>   
#include <sys/stat.h>   
#include <sys/types.h>   
#include <sys/time.h>   
#include <sys/mman.h>   
#include <sys/ioctl.h>     
#include <asm/types.h>          
#include <linux/videodev2.h>  
#include <getopt.h> 
#define __USE_GNU  				//绑定核心需要的头文件
#include <sched.h> 				//绑定核心需要的头文件，需要在pthread.h之前
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/time.h>
#include <jpeglib.h>
#include <jerror.h>
#include <sys/syscall.h>

#include <SDL2/SDL.h>




#define MAX_INPUT_UVC_CH	2
#define ENABLE_DISPLAY_PTHREAD	1
#define ENABLE_MANUAL_SET_CUP 1
#define ENABLE_SELECT_POLLING	1
#define ENABLE_DEBUG_BINGO  0
#define ENABLE_OPTIMIZE		1


#define BUFFER_FRAME_NUM		4
#define V4L2_BUFFER_FRAME_NUM	BUFFER_FRAME_NUM
#define DECODE_BUFFER_FRAME_NUM   4
#define DISPLAY_BUFFER_FRAME_NUM  4

#define POOL_MAX_MALLOC_SIZE	(4096*2160)



#define CLEAR(x) memset (&(x), 0, sizeof (x))   
#define POS_OFFSET_ADD(startpos,offset,maxSize)	(((startpos+offset) >= maxSize)?((startpos+offset)-maxSize):(startpos+offset))
#define POS_OFFSET_SUB(startpos,offset,maxSize) ((startpos < offset)?(maxSize - (offset-startpos)):(startpos-offset))

enum
{
	FORMAT_MJPEG = 0,
    FORMAT_YUYV ,
    FORMAT_RGB888,
};

struct _buffer_info {  
        void *                  start;  
        size_t                  length;  
};  

struct _display_pool
{
	unsigned char* addr;
	int flag;
};

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
	unsigned char layer[MAX_INPUT_UVC_CH];
	int alpha[MAX_INPUT_UVC_CH];
	char alphaFlag[MAX_INPUT_UVC_CH];				//1-淡入  2-淡出0-不做处理
} OutLayerSetting;



struct _uvcCameraDev
{
/* input info */

	/* user info */
	char dev_name[MAX_INPUT_UVC_CH][50];
	int v4l2Format[MAX_INPUT_UVC_CH];
	unsigned int uvc_input_width[MAX_INPUT_UVC_CH];
	unsigned int uvc_input_height[MAX_INPUT_UVC_CH];

	/* v4l2 info */
	int fd[MAX_INPUT_UVC_CH]; 
	int uvc_ch_num;
	struct _buffer_info* buffer_info[MAX_INPUT_UVC_CH];
	int n_buffers[MAX_INPUT_UVC_CH];

	/* decode info */
	char* imageFrameBuffer[MAX_INPUT_UVC_CH][3];
	unsigned int oneLineByteSize[MAX_INPUT_UVC_CH][3];
	unsigned int imageSize_width[MAX_INPUT_UVC_CH];
	unsigned int imageSize_height[MAX_INPUT_UVC_CH];

/* output info */
	/* user info */
	char* windowsName;
	int windowsSize_width;
	int windowsSize_height;
	

	/* SDL2 info */
	int pixformat[MAX_INPUT_UVC_CH];
	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture[MAX_INPUT_UVC_CH];

	/* layer info */
	LayerSetting layerInfo[MAX_INPUT_UVC_CH];
	OutLayerSetting outLayerAlpha;
	int alphaTime;

};



extern sem_t		display_sem[MAX_INPUT_UVC_CH];
extern sem_t		decode_sem[MAX_INPUT_UVC_CH];
extern pthread_mutex_t sdl2_mutex;
extern struct v4l2_buffer decode_pool[MAX_INPUT_UVC_CH][DECODE_BUFFER_FRAME_NUM];
extern int decode_pool_read[MAX_INPUT_UVC_CH];
extern int decode_pool_write[MAX_INPUT_UVC_CH];

extern struct _display_pool display_pool[MAX_INPUT_UVC_CH][DISPLAY_BUFFER_FRAME_NUM];
extern int display_pool_read[MAX_INPUT_UVC_CH];
extern int	display_pool_write[MAX_INPUT_UVC_CH];


void  open_uvc_device(struct _uvcCameraDev* dev,int ch);
void  close_uvc_device(struct _uvcCameraDev* dev,int ch) ;
void  init_uvc_device(struct _uvcCameraDev* dev,int ch) ;
void  start_capturing(struct _uvcCameraDev* dev,int ch);
void  stop_capturing(struct _uvcCameraDev* dev,int ch) ;
void  uninit_device(struct _uvcCameraDev* dev,int ch);


int init_sdl2_display(struct _uvcCameraDev* dev);
void quit_sdl2_display(struct _uvcCameraDev* dev);
void sdl2_display_frame(struct _uvcCameraDev* dev,int layerCh);
void sdl2_clear_frame(struct _uvcCameraDev* dev);
void sdl2_present_frame(struct _uvcCameraDev* dev);
void sdl2_SetAlpha(struct _uvcCameraDev* dev,int layerCh,int value);
int sdl2_GetAlpha(struct _uvcCameraDev* dev,int layerCh,int* value);
void* task_v4l2_0(void* args);
void* task_v4l2_1(void* args);
void* task_decode_0(void* args);
void* task_decode_1(void* args);
void* task_display_0(void* args);
void* task_display_1(void* args) ;
int  xioctl(int fd,int  request,void * arg) ;
void malloc_decode_pool(struct _uvcCameraDev* dev);
void free_decode_pool(struct _uvcCameraDev* dev);
void display_0(struct _uvcCameraDev* dev);
void display_1(struct _uvcCameraDev* dev);
pid_t gettid();
void* task_alpha(void* args);
void* task_clear(void* args);




#endif













