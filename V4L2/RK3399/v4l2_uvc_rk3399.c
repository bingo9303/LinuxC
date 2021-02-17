#include "define.h"



typedef void * (*_pthreadFunc) (void*);


#define DEFUALT_DISPLAY_SIZE_X  640
#define DEFUALT_DISPLAY_SIZE_Y  480

#define DEFAULT_UVC_SIZE_X		640
#define DEFAULT_UVC_SIZE_Y		480


int fd_tty;

sem_t		decode_sem[MAX_INPUT_UVC_CH];
sem_t		display_sem[MAX_INPUT_UVC_CH];



pthread_mutex_t sdl2_mutex = PTHREAD_MUTEX_INITIALIZER ;



pthread_t 	tid_alpha;
pthread_t 	tid_command;
pthread_t 	tid_clear;
pthread_t 	tid_v4l2[MAX_INPUT_UVC_CH];
pthread_t 	tid_decode[MAX_INPUT_UVC_CH];
pthread_t 	tid_display[MAX_INPUT_UVC_CH];

_pthreadFunc  pthread_v4l2[MAX_INPUT_UVC_CH] = {task_v4l2_0,task_v4l2_1};
_pthreadFunc  pthread_decode[MAX_INPUT_UVC_CH] = {task_decode_0,task_decode_1};
_pthreadFunc  pthread_display[MAX_INPUT_UVC_CH] = {task_display_0,task_display_1};



	


  
static void  usage(FILE *fp,int argc,char **argv)  
{  
    fprintf (fp,  
                "Usage: %s [options]\n\n"  
                "Options:\n"  
                "-d | --device name   Video device name [/dev/video]\n"  
                "-h | --help          Print this message\n"  
                "-f | --format        Input Video Format:mjpeg, yuyv, rgb888\n"  
                "-x | --uvcx		  UVC Device Input Width\n"
                "-y | --uvcy		  UVC Device Input Height\n"
                "-W | --Width		  Window Display Width\n"
                "-H | --Height		  Window Display Height\n"
                "",argv[0]);  
}  
  
static const char short_options [] = "d:f:x:y:W:H:";  
  
static const struct option  
long_options [] = {  
        { "device",     required_argument,      NULL,           'd' },  
        { "help",       no_argument,            NULL,           'h' },  
        { "format",		no_argument,            NULL,           'f' },
        { "uvcx",     	no_argument,            NULL,           'x' },
  		{ "uvcy",     	no_argument,            NULL,           'y' },
        { "Width",     	no_argument,            NULL,           'W' },
        { "Height",     no_argument,            NULL,           'H' },
        { 0, 0, 0, 0 }  
};  

pid_t gettid() {
 return syscall(SYS_gettid);
}


void* task_command(void* args)
{
	int inputcharNum;
	char buf_tty[10];
	struct _uvcCameraDev* dev = (struct _uvcCameraDev*)args;
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
			//	stopAllTaskFlag = 1;
				break;
			}
			else if(buf_tty[0] == 's')
			{
				OutLayerSetting* outLayerAlpha = &dev->outLayerAlpha;
				sdl2_GetAlpha(dev,0,&outLayerAlpha->alpha[0]);
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
				OutLayerSetting* outLayerAlpha = &dev->outLayerAlpha;
				if((layerCh>=1) && (layerCh<=dev->uvc_ch_num))
				{
					ret = sdl2_GetAlpha(dev,layerCh-1,&outLayerAlpha->alpha[layerCh-1]);
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


void Stop(int signo) 
{
    printf("stop!!!\n");
    _exit(0);
}


int main(int argc, char *argv[])
{  
	int ch;
	struct _uvcCameraDev* dev = malloc(sizeof(struct _uvcCameraDev));
	memset(dev,0,sizeof(struct _uvcCameraDev));
	
    for (;;) 
    {  
        int index;  
        int c;  
		static char format_index = 0;
		static char ucvx_index = 0;
		static char ucvy_index = 0;
		
            
        c = getopt_long (argc, argv,short_options, long_options,&index);  

        if (-1 == c)  
                break;  

        switch (c) {  
        case 0: /* getopt_long() flag */  
                break;  
        case 'd':  
				strcpy((char*)dev->dev_name[dev->uvc_ch_num],optarg);
				dev->uvc_ch_num++;
                break;  
        case 'h':  
                usage (stdout, argc, argv);  
                exit (EXIT_SUCCESS);
		case 'x':
				dev->uvc_input_width[ucvx_index] = atoi(optarg);
				ucvx_index++;
				break;
		case 'y':
				dev->uvc_input_height[ucvy_index] = atoi(optarg);
				ucvy_index++;
				break;
		case 'W':
				dev->windowsSize_width = atoi(optarg);
				break;
		case 'H':
				dev->windowsSize_height = atoi(optarg);
				break;
        case 'f':
              if(!strcmp("yuyv",optarg))
              {
              	dev->v4l2Format[format_index] = FORMAT_YUYV;
              }
              else if(!strcmp("rgb888",optarg))
              {
                dev->v4l2Format[format_index] = FORMAT_RGB888;
              }
			  else if(!strcmp("mjpeg",optarg))
			  {
				dev->v4l2Format[format_index] = FORMAT_MJPEG;
			  }
              else
              {
                usage (stderr, argc, argv);  
                exit (EXIT_FAILURE);
              }
			  format_index++;
              break;
        default:  
                usage (stderr, argc, argv);  
                exit (EXIT_FAILURE);  
        }  
    }  

	if(dev->uvc_ch_num == 0)
	{
		printf("no select uvc device\r\n");
		usage (stderr, argc, argv);  
        exit (EXIT_FAILURE); 
	}
	signal(SIGINT, Stop); 

	fd_tty=open("/dev/tty",O_RDONLY|O_NONBLOCK);//用只读和非阻塞的方式打开文件dev/tty
    if(fd_tty<0)
    {
        perror("open dev/tty");
        exit(1);
    }

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

	malloc_decode_pool(dev);

	
	
	for(ch=0;ch<dev->uvc_ch_num;ch++)
	{
		if(dev->uvc_input_width[ch] == 0)	dev->uvc_input_width[ch] = DEFAULT_UVC_SIZE_X;
		if(dev->uvc_input_height[ch] == 0)	dev->uvc_input_height[ch] = DEFAULT_UVC_SIZE_Y;

		printf("dev->uvc_input_width[%d]=%d,dev->uvc_input_height[%d]=%d\r\n",
			ch,dev->uvc_input_width[ch],ch,dev->uvc_input_height[ch]);

		dev->pixformat[ch] = SDL_PIXELFORMAT_RGB24;	

		dev->layerInfo[ch].inputPort = ch;
		dev->outLayerAlpha.alpha[ch] = (ch==0)?0xFF:0;
		dev->outLayerAlpha.alphaFlag[ch] = 0;
	}
	dev->alphaTime = 600;

	if(dev->windowsSize_width == 0)		dev->windowsSize_width = DEFUALT_DISPLAY_SIZE_X;
	if(dev->windowsSize_height == 0)	dev->windowsSize_height = DEFUALT_DISPLAY_SIZE_Y;



	for(ch=0;ch<dev->uvc_ch_num;ch++)
	{
		open_uvc_device(dev,ch);
		init_uvc_device(dev,ch);
	}

	/*初始化SDL2*/
	init_sdl2_display(dev);
	for(ch=0;ch<dev->uvc_ch_num;ch++)
	{
		sem_init(&decode_sem[ch], 0, 0);
		pthread_create(&tid_decode[ch],NULL,pthread_decode[ch],dev);
		
	}
#if ENABLE_DISPLAY_PTHREAD
	for(ch=0;ch<dev->uvc_ch_num;ch++)
	{
		sem_init(&display_sem[ch], 0, 0);
		pthread_create(&tid_display[ch],NULL,pthread_display[ch],dev);	
	}
#endif
	
	
	for(ch=0;ch<dev->uvc_ch_num;ch++)
	{
		start_capturing (dev,ch); 
		pthread_create(&tid_v4l2[ch],NULL,pthread_v4l2[ch],dev);
	}

	

	pthread_create(&tid_alpha,NULL,task_alpha,dev);
	pthread_create(&tid_command,NULL,task_command,dev);
	pthread_create(&tid_clear,NULL,task_clear,dev);

	for(ch=0;ch<dev->uvc_ch_num;ch++)
	{
		pthread_join(tid_v4l2[ch],NULL);
	}

	for(ch=0;ch<dev->uvc_ch_num;ch++)
	{
		pthread_join(tid_decode[ch],NULL);
	}
#if ENABLE_DISPLAY_PTHREAD
	for(ch=0;ch<dev->uvc_ch_num;ch++)
	{
		pthread_join(tid_display[ch],NULL);
	}
#endif

	for(ch=0;ch<dev->uvc_ch_num;ch++)
	{
	#if ENABLE_DISPLAY_PTHREAD
		sem_destroy(&display_sem[ch]);
	#endif
		sem_destroy(&decode_sem[ch]);
	}

	

	for(ch=0;ch<dev->uvc_ch_num;ch++)
	{
		stop_capturing (dev,ch);
		uninit_device (dev,ch); 
		close_uvc_device(dev,ch) ;
	}

	quit_sdl2_display(dev);

   

	free_decode_pool(dev);
	free(dev);
	

    exit (EXIT_SUCCESS);  

    return 0;  
}
