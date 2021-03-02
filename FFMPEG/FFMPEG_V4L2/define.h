#ifndef _DEFINE_H
#define _DEFINE_H

#include <iostream>
#include <queue>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <string>
#include <sched.h> 				
#include <pthread.h>
#include <sys/syscall.h>
#include <getopt.h> 
#include <semaphore.h>
#include <signal.h>
#include <unistd.h> 
#include <stdio.h>
#include <sys/time.h>
#include <time.h> 




extern "C"{

#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>

}

#define BUFFER_NUM		2

#define BINGO_DEBUG		0

#if BINGO_DEBUG
#define bingo_log(x)		printf x		
#else
#define bingo_log(x)
#endif



#include "FrameQueue.h"
#include "PacketQueue.h"
#include "Media.h"
#include "VideoDisplay.h"
#include "Video.h"




#endif


