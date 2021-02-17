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
#include <sys/time.h>


//#include <stdafx.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
   // FILE __iob_func[3] = { *stdin,*stdout,*stderr };
#include <SDL2/SDL.h>



/***************** function declaration ******************/

void            usage(char *msg);
/************ function implementation ********************/

int main(int argc, char *argv[])
{
    /*

    * declaration for jpeg decompression

    */

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE           *infile;
	int fd_tty,inputcharNum=0;
	char buf_tty[10];
    unsigned char* buffer = NULL;
	unsigned int lineIndex = 0;
	unsigned int oneLineByteSize;
    /*
    * declaration for framebuffer device
    */
    unsigned int    screensize_x = 960;
    unsigned int    screensize_y = 540;

    /*
    * check auguments
    */
    if (argc < 2) 
    {
        usage("insuffient auguments");
        exit(-1);
    }

	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//用只读和非阻塞的方式打开文件dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }
    /*
    * open input jpeg file
    */
    if ((infile = fopen(argv[1], "rb")) == NULL) {
            printf("open %s failed\r\n", argv[1]);
            exit(-1);
    }

	if(argc == 3)
	{
		screensize_x = atoi(argv[2]);
	}
	else if(argc == 4)
	{
		screensize_x = atoi(argv[2]);
		screensize_y = atoi(argv[3]);
	}


	
	if (SDL_Init(SDL_INIT_VIDEO)) {
	//	  printf("Could not initialize SDL - %s\n", SDL_GetError());
		  return -1;
	  }


	
	
	buffer = (unsigned char *) malloc(1920 * 1080 * 3);

    /*
    * init jpeg decompress object error handler
    */
	static struct timeval decode_tv;
	gettimeofday(&decode_tv,NULL);
    printf("111...%ld\r\n",decode_tv.tv_usec);

	
    cinfo.err = jpeg_std_error(&jerr);
	cinfo.jpeg_color_space = JCS_RGB;//libjpeg解出来的像素格式为rgb
	cinfo.dct_method = JDCT_FASTEST;
    jpeg_create_decompress(&cinfo);

    /*

    * bind jpeg decompress object to infile

    */
    
    jpeg_stdio_src(&cinfo, infile);

    /*
    * read jpeg header
    */

    jpeg_read_header(&cinfo, TRUE);


    /*
    * decompress process.
    * note: after jpeg_start_decompress() is called
    * the dimension infomation will be known,
    * so allocate memory buffer for scanline immediately
    */

    jpeg_start_decompress(&cinfo);
/*
	printf("cinfo.output_height = %d,cinfo.output_width = %d,cinfo.output_components = %d\r\n",
				cinfo.output_height,cinfo.output_width,cinfo.output_components);*/

	
	unsigned char*  bufferPtr;
	oneLineByteSize = cinfo.output_width * cinfo.output_components;
//	buffer = (unsigned char *) malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);
	if(buffer == NULL)		printf("malloc mem faild\r\n");

	

	while (cinfo.output_scanline < cinfo.output_height) 
	{	
		bufferPtr = &buffer[lineIndex * oneLineByteSize];
		jpeg_read_scanlines(&cinfo, &bufferPtr, 1);
		lineIndex++;
	}

	 /*
    * finish decompress, destroy decompress object
    */

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    

	gettimeofday(&decode_tv,NULL);
	printf("222...%ld\r\n",decode_tv.tv_usec);

	SDL_Window *screen;
    //SDL 2.0 Support for multiple windows
    screen = SDL_CreateWindow("Simplest Video Play SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        screensize_x, screensize_y, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!screen) {
        printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
        return -1;
    }

	SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

	Uint32 pixformat = 0;
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	pixformat = SDL_PIXELFORMAT_RGB24;/*SDL_PIXELFORMAT_IYUV*/;		//这里的像素格式指的是输入SDL的图像的像素格式

	SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer, pixformat, SDL_TEXTUREACCESS_STATIC/*SDL_TEXTUREACCESS_STREAMING*/, cinfo.output_width, cinfo.output_height);

	
	SDL_Rect scale_Rect;
	SDL_Rect crop_Rect;
	
		
	while(1)
    {
        SDL_UpdateTexture(sdlTexture, NULL, buffer, oneLineByteSize);	//这个相当于crop，填入的jpegBufferFrame单位的一帧画面
        																		//最后一个参数是，显示一行需要多少字节
		
		crop_Rect.x = 0;
        crop_Rect.y = 0;
        crop_Rect.w = cinfo.output_width;
        crop_Rect.h = cinfo.output_height;

        scale_Rect.x = 0;
        scale_Rect.y = 0;
        scale_Rect.w = screensize_x;		
        scale_Rect.h = screensize_y;
	

        SDL_RenderClear(sdlRenderer);
        SDL_RenderCopy(sdlRenderer, sdlTexture, &crop_Rect, &scale_Rect);	//crop_Rect如果为NULL，则默认整个画面的crop
        SDL_RenderPresent(sdlRenderer);
        //Delay 40ms
     //   SDL_Delay(40);
     
     	usleep(20000);
        inputcharNum=read(fd_tty,buf_tty,10);
        if(inputcharNum<0)
        {
        	//printf("no inputs\n");
        }
		else
		{
			if(buf_tty[0] == 'q')	break;
		}

    }
    SDL_Quit();

	

   
	/*
    * release memory buffer

    */

    free(buffer);
    /*
    * close jpeg inputing file
    */

    fclose(infile);
	
	close(fd_tty);
    return (0);
}


void usage(char *msg)
{
    fprintf(stderr, "%s/n", msg);
    printf("Usage: fv some-jpeg-file.jpg/n");
}



/*

#include "stdafx.h"
#include <stdio.h>

extern "C" {

#include "libavcodec\avcodec.h"
#include "libavformat\avformat.h"
#include "libswresample\swresample.h"
#include "libswscale\swscale.h"
    FILE __iob_func[3] = { *stdin,*stdout,*stderr };
#include "SDL.h"
}


const int bpp = 12;
int screen_w = 640, screen_h = 360;
const int pixel_w = 640, pixel_h = 360;

unsigned char buffer[pixel_h * pixel_h * bpp / 8];


int main(int argc, char* argv[]) { //这里的main函数的参数主要是sdl里面也有一个main函数

    //yuv到屏幕的过程
    //SDL Simple DirectMedia Layer
    if (SDL_Init(SDL_INIT_VIDEO)) {
        printf("Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window *screen;
    //SDL 2.0 Support for multiple windows
    screen = SDL_CreateWindow("Simplest Video Play SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!screen) {
        printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
        return -1;
    }
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

    Uint32 pixformat = 0;
    //IYUV: Y + U + V  (3 planes)
    //YV12: Y + V + U  (3 planes)
    pixformat = SDL_PIXELFORMAT_IYUV;

    SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer, pixformat, SDL_TEXTUREACCESS_STREAMING, pixel_w, pixel_h);

    FILE *fp = NULL;
    fp = fopen("demo.yuv", "rb+");

    if (fp == NULL) {
        printf("cannot open this file\n");
        return -1;
    }

    SDL_Rect sdlRect;

    while (1) {
        if (fread(buffer, 1, pixel_w*pixel_h*bpp / 8, fp) != pixel_w*pixel_h*bpp / 8) {
            // Loop
            fseek(fp, 0, SEEK_SET);
            fread(buffer, 1, pixel_w*pixel_h*bpp / 8, fp);
        }

        SDL_UpdateTexture(sdlTexture, NULL, buffer, pixel_w);

        sdlRect.x = 0;
        sdlRect.y = 0;
        sdlRect.w = screen_w;
        sdlRect.h = screen_h;

        SDL_RenderClear(sdlRenderer);
        SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
        SDL_RenderPresent(sdlRenderer);
        //Delay 40ms
        SDL_Delay(40);

    }
    SDL_Quit();
    return 0;

}
*/

