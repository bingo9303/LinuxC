#include "define.h"



struct v4l2_buffer decode_pool[MAX_INPUT_UVC_CH][DECODE_BUFFER_FRAME_NUM];
int decode_pool_read[MAX_INPUT_UVC_CH] = {0};
int decode_pool_write[MAX_INPUT_UVC_CH] = {0};



struct formatListInfo
{
	unsigned int index;
	char name[16];
};

static struct formatListInfo v4l2FormatList[] = {
	{	V4L2_PIX_FMT_MJPEG,		"MJPEG"},
	{	V4L2_PIX_FMT_YUYV,		"YUYV" },
	{	V4L2_PIX_FMT_RGB24,		"RGB8-8-8"},
};


int  xioctl(int fd,int  request,void * arg)  
{  
    int r;  
    do r = ioctl (fd, request, arg);  
    while (-1 == r && EINTR == errno);  
    return r;  
}  



void  open_uvc_device(struct _uvcCameraDev* dev,int ch)  
{  
    struct stat st;   


    if (-1 == stat (dev->dev_name[ch], &st)) 
    {  
        fprintf (stderr, "[%d]Cannot identify '%s': %d, %s\n",  
                    ch,dev->dev_name[ch], errno, strerror (errno));  
        exit (EXIT_FAILURE);  
    }  

    if (!S_ISCHR (st.st_mode)) 
    {  
        fprintf (stderr, "[%d] %s is no device\n",ch,dev->dev_name[ch]);  
        exit (EXIT_FAILURE);  
    }  

#if ENABLE_SELECT_POLLING  
    dev->fd[ch] = open (dev->dev_name[ch], O_RDWR | O_NONBLOCK, 0);  //不阻塞
#else
	dev->fd[ch] = open (dev->dev_name[ch], O_RDWR, 0);
#endif

    if (-1 == dev->fd[ch]) 
    {  
        fprintf (stderr, "[%d]Cannot open '%s': %d, %s\n",  
                        ch,dev->dev_name[ch], errno, strerror (errno));  
        exit (EXIT_FAILURE);  
    }  
} 

void  close_uvc_device(struct _uvcCameraDev* dev,int ch)  
{  
    if (-1 == close (dev->fd[ch]))  
    {
		fprintf (stderr, "[%d] uvc close\n",ch);  
        exit (EXIT_FAILURE);  
	} 
    dev->fd[ch] = -1;  
}  


static void  init_mmap(struct _uvcCameraDev* dev,int ch)  
{  
    struct v4l2_requestbuffers req;  
  
    CLEAR (req);  

    req.count               = V4L2_BUFFER_FRAME_NUM;  //缓冲帧
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    req.memory              = V4L2_MEMORY_MMAP;  
  
    if (-1 == xioctl (dev->fd[ch], VIDIOC_REQBUFS, &req)) 
    {  
        if (EINVAL == errno) 
        {  
            fprintf (stderr, "[%d] %s does not support memory mapping\n",ch,&dev->dev_name[ch][0]);  
            exit (EXIT_FAILURE);  
        } 
        else 
        {  
			fprintf (stderr, "[%d] VIDIOC_REQBUFS faild\n",ch);
            exit (EXIT_FAILURE);  
        }  
    }  
    
    dev->buffer_info[ch] = calloc (req.count, sizeof (struct _buffer_info));  
  
    if (!dev->buffer_info[ch]) 
	{  
	    fprintf (stderr, "[%d]Out of memory\n",ch);  
	    exit (EXIT_FAILURE);  
    }  

    for (dev->n_buffers[ch] = 0; dev->n_buffers[ch] < req.count; ++dev->n_buffers[ch]) 
    {  
        struct v4l2_buffer buf;  

        CLEAR (buf);  

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        buf.memory      = V4L2_MEMORY_MMAP;  
        buf.index       = dev->n_buffers[ch];  

        if (-1 == xioctl (dev->fd[ch], VIDIOC_QUERYBUF, &buf))  
        {
			fprintf (stderr, "[%d]VIDIOC_QUERYBUF faild\n",ch);  
	    	exit (EXIT_FAILURE);
		}

        dev->buffer_info[ch][dev->n_buffers[ch]].length = buf.length;  
        dev->buffer_info[ch][dev->n_buffers[ch]].start =  

				mmap (  NULL /* start anywhere */,  
                        buf.length,  
                        PROT_READ | PROT_WRITE /* required */,  
                        MAP_SHARED /* recommended */,  
                        dev->fd[ch], buf.m.offset);  

        if (MAP_FAILED == dev->buffer_info[ch][dev->n_buffers[ch]].start)  
        {
			fprintf (stderr, "[%d]mmap %d faild\n",ch,dev->n_buffers[ch]);  
	    	exit (EXIT_FAILURE);	
		}
                
    }  
}  


void  init_uvc_device(struct _uvcCameraDev* dev,int ch)  
{  
	struct v4l2_capability cap;  
    struct v4l2_cropcap cropcap;  
    struct v4l2_crop crop;  
    struct v4l2_format fmt; 
    struct v4l2_fmtdesc desc;



    if (-1 == xioctl (dev->fd[ch], VIDIOC_QUERYCAP, &cap)) 
    {  
        if (EINVAL == errno) 
        {  
            fprintf (stderr, "[%d] %s is no V4L2 device\n",ch, &dev->dev_name[ch][0]);  
            exit (EXIT_FAILURE);  
        } 
        else 
        {  
        	fprintf (stderr, "[%d] VIDIOC_QUERYCAP\n",ch);  
        	exit (EXIT_FAILURE); 
        }  
    }  

    printf("Driver Name:%s\nCard Name:%s\nBus info:%s\nDriver Version:%u.%u.%u\n",
    		cap.driver,
    		cap.card,
    		cap.bus_info,
    		(cap.version>>16)&0XFF, 
    		(cap.version>>8)&0XFF,
    		cap.version&0XFF);
  
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
    {  
        fprintf (stderr, "[%d] %s is no video capture device\n",ch,&dev->dev_name[ch][0]);  
        exit (EXIT_FAILURE);  
    }  
   
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) 
	{  
        fprintf (stderr, "[%d] %s does not support streaming i/o\n",ch,&dev->dev_name[ch][0]);  
        exit (EXIT_FAILURE);  
    }  

    desc.index = 0;
	desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while (ioctl(dev->fd[ch], VIDIOC_ENUM_FMT, &desc) == 0) 
	{
		printf("format %s\n", desc.description);
		desc.index++;
	}
   
    /* Select video input, video standard and tune here. */  
  
    CLEAR (cropcap);  
  
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;    
    if (0 == xioctl (dev->fd[ch], VIDIOC_CROPCAP, &cropcap)) 
    {  
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        crop.c = cropcap.defrect; /* reset to default */  

        if (-1 == xioctl (dev->fd[ch], VIDIOC_S_CROP, &crop)) 
        {  
            switch (errno) 
            {  
                case EINVAL:  
					printf("[%d] Cropping not supported\n",ch);
                    break;  
                default:  
                    printf("[%d] VIDIOC_S_CROP faild,Errors ignored\n",ch);  
                    break;  
            }  
        }  
    } 
    else 
    {      
        /* Errors ignored. */  
		printf("[%d] VIDIOC_CROPCAP faild,Errors ignored\n",ch);
    }    
    CLEAR (fmt);  

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    fmt.fmt.pix.width       = dev->uvc_input_width[ch];   
    fmt.fmt.pix.height      = dev->uvc_input_height[ch];  
    fmt.fmt.pix.pixelformat = v4l2FormatList[dev->v4l2Format[ch]].index;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if (-1 == xioctl (dev->fd[ch], VIDIOC_S_FMT, &fmt))  
    {
		fprintf (stderr, "[%d] VIDIOC_S_FMT faild\n",ch);  
        exit (EXIT_FAILURE);
	}
	else
	{
		printf("use format : %s\r\n",v4l2FormatList[dev->v4l2Format[ch]].name);
	}
	

    /* Note VIDIOC_S_FMT may change width and height. */  
  
    /* Buggy driver paranoia. */  
	unsigned int min;
	if(dev->v4l2Format[ch] == FORMAT_YUYV)
        min = fmt.fmt.pix.width * 2;  	
    else if(dev->v4l2Format[ch] == FORMAT_RGB888)
        min = fmt.fmt.pix.width * 3;
    else
		goto request_buffers;

        
    if (fmt.fmt.pix.bytesperline < min)  
    {
		fmt.fmt.pix.bytesperline = min;  
	}		

    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;  
    if (fmt.fmt.pix.sizeimage < min)  
    {
		fmt.fmt.pix.sizeimage = min;  
	}

	
	
request_buffers:
    init_mmap(dev,ch);
}  
  

void  start_capturing(struct _uvcCameraDev* dev,int ch)  
{  
    unsigned int i;  
    enum v4l2_buf_type type; 

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
	
    for (i = 0; i < dev->n_buffers[ch]; ++i) 
    {  
        struct v4l2_buffer buf;  

        CLEAR (buf);  

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        buf.memory      = V4L2_MEMORY_MMAP;  
        buf.index       = i;  

        if (-1 == xioctl (dev->fd[ch], VIDIOC_QBUF, &buf))  
        {
			fprintf (stderr, "[%d] VIDIOC_QBUF faild\n",ch);  
        	exit (EXIT_FAILURE);
		}
    }  
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
	
    if (-1 == xioctl (dev->fd[ch], VIDIOC_STREAMON, &type))  
    {
		fprintf (stderr, "[%d] VIDIOC_STREAMON faild\n",ch);  
        exit (EXIT_FAILURE);
	}
}  


void  stop_capturing(struct _uvcCameraDev* dev,int ch)  
{  
    enum v4l2_buf_type type;  
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
	if (-1 == xioctl (dev->fd[ch], VIDIOC_STREAMOFF, &type))  
	{
		fprintf (stderr, "[%d] VIDIOC_STREAMOFF faild\n",ch);  
        exit (EXIT_FAILURE);
	}
}  

extern void test(void* args);
  
static int  read_frame(struct _uvcCameraDev* dev,int ch)  
{    
	//这里可能需要判断一下是否被使用还没被清空
	struct v4l2_buffer* buf = &decode_pool[ch][decode_pool_write[ch]];
	memset(buf,0,sizeof(struct v4l2_buffer)); 
	
    buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    buf->memory = V4L2_MEMORY_MMAP;  

    if (-1 == xioctl (dev->fd[ch], VIDIOC_DQBUF, buf)) 
    {  
        switch (errno) 
        {  
            case EAGAIN: 
				fprintf (stderr, "[%d] VIDIOC_DQBUF faild EAGAIN\n",ch);
                return 0;  

            case EIO:  
                /* Could ignore EIO, see spec. */  
				fprintf (stderr, "[%d] VIDIOC_DQBUF faild EIO\n",ch);
				return 0;  
                /* fall through */  

            default:  
				fprintf (stderr, "[%d] VIDIOC_DQBUF faild errno =%d\n",ch,errno);  
				return 0;  
        		//exit (EXIT_FAILURE);
				break; 
        }  
    }  
    assert (buf->index < dev->n_buffers[ch]);   
//    process_image (&dev->v4l2_buffer[ch][buf.index]);  

#if ENABLE_DEBUG_BINGO
	static struct timeval decode_tv;
	gettimeofday(&decode_tv,NULL);
	printf("1.decode_pool_write[%d] = %d , %ld\r\n",ch,decode_pool_write[ch],decode_tv.tv_usec);
#endif
	decode_pool_write[ch] = POS_OFFSET_ADD(decode_pool_write[ch],1,DECODE_BUFFER_FRAME_NUM);

	sem_post(&decode_sem[ch]);
    return 1;  
}  


void  uninit_device(struct _uvcCameraDev* dev,int ch)  
{  
    unsigned int i;  
	for (i = 0; i < dev->n_buffers[ch]; ++i)  
	{
		if (-1 == munmap (dev->buffer_info[ch][i].start, dev->buffer_info[ch][i].length))  
		{
			printf ("[%d] munmap %d faild\n",ch,i);  
		}
	}
	     
    free (dev->buffer_info[ch]);  
}  

#if ENABLE_SELECT_POLLING  
void* task_v4l2_0(void* args)  
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
	
    while (1) 
    {  
        for (;;) 
        {  		
            fd_set fds;  
            struct timeval tv;  
            int r;  

            FD_ZERO (&fds);  
            FD_SET (dev->fd[0], &fds);  

            /* Timeout. */  
            tv.tv_sec = 10;  
            tv.tv_usec = 0;  

            r = select (dev->fd[0] + 1, &fds, NULL, NULL, &tv);  

            if (-1 == r) {  
				
                if (EINTR == errno)  continue;  
				else
				{
					fprintf (stderr, "[0] select faild\n"); 
					continue;  
        			//exit (EXIT_FAILURE);
				}
            }  

            if (0 == r) {  
				int m;
                fprintf (stderr, "[0] select timeout\n"); 
				for(m=0;m<V4L2_BUFFER_FRAME_NUM;m++)
				{
					struct v4l2_buffer* buf = &decode_pool[0][m];
					if (-1 == xioctl (dev->fd[0], VIDIOC_QBUF, buf)) 
				    {
						fprintf (stderr, "[%d] VIDIOC_QBUF faild select timeout\n",m);  
					}
				}
				continue;
               // exit (EXIT_FAILURE);  
            }  

            if (read_frame (dev,0))  
            {
                break;             
            }
            /* EAGAIN - continue select loop. */  
        }  
    }  
}  


void* task_v4l2_1(void* args) 
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
	
	while (1) 
    {  
        for (;;) 
        { 			
            fd_set fds;  
            struct timeval tv;  
            int r;  

            FD_ZERO (&fds);  
            FD_SET (dev->fd[1], &fds);  

            /* Timeout. */  
            tv.tv_sec = 10;  
            tv.tv_usec = 0;  

            r = select (dev->fd[1] + 1, &fds, NULL, NULL, &tv);  

            if (-1 == r) {  
				
                if (EINTR == errno)  continue;  
				else
				{
					fprintf (stderr, "[1] select faild\n");  
					continue;  
        			//exit (EXIT_FAILURE);
				}
            }  

            if (0 == r) {  
                fprintf (stderr, "[1] select timeout\n");  
                int m;
				for(m=0;m<V4L2_BUFFER_FRAME_NUM;m++)
				{
					struct v4l2_buffer* buf = &decode_pool[1][m];
					if (-1 == xioctl (dev->fd[1], VIDIOC_QBUF, buf)) 
				    {
						fprintf (stderr, "[%d] VIDIOC_QBUF faild select timeout\n",m);  
					}
				}
				continue; 
            }  

            if (read_frame (dev,1))  
            {
                break;             
            }
            /* EAGAIN - continue select loop. */  
        }  
    } 
#endif
}

#else

void* task_v4l2_0(void* args)  
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
	
    while (1) 
    {  
		//开始捕获图像
		enum v4l2_buf_type type;
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		xioctl (dev->fd[0], VIDIOC_STREAMON, &type);

		struct v4l2_buffer* buf = &decode_pool[0][decode_pool_write[0]];
		memset(buf,0,sizeof(struct v4l2_buffer)); 
		
		buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
		buf->memory = V4L2_MEMORY_MMAP;  
		xioctl (dev->fd[0], VIDIOC_DQBUF, buf);

		assert (buf->index < dev->n_buffers[0]);   
		
		static struct timeval decode_tv;
		gettimeofday(&decode_tv,NULL);

		printf("1.decode_pool_write[%d] = %d , %ld\r\n",0,decode_pool_write[0],decode_tv.tv_usec);

		decode_pool_write[0] = POS_OFFSET_ADD(decode_pool_write[0],1,DECODE_BUFFER_FRAME_NUM);

		sem_post(&decode_sem[0]);
		usleep(1);
    }  
}  

void* task_v4l2_1(void* args) 
{

}


#endif






  


  

  

#if 0
static void  process_image(const void *p)  
{  
    
    char strbuff[50];
    unsigned char* startAddr;
	unsigned int len;
    //printf("1.%ld\r\n",GetTime_Ms());
  	switch (io) 
  	{
		case IO_METHOD_READ: 
		case IO_METHOD_MMAP:  
			{
				struct buffer * buf = (struct buffer * )p;
				startAddr = buf->start;
				len = buf->length;
			}
			break;
		case IO_METHOD_USERPTR:  
			{
				struct v4l2_buffer * buf = (struct v4l2_buffer *)p;
				startAddr = (unsigned char*)buf->m.userptr;
				len = buf->length;
			}			
			break;
	}


    if(fb_info.fb_depth == 32)
    {
        if(videoFormatType == FORMAT_YUYV)
        {
            yuyv_to_rgbxxx(  startAddr,
                            len,
                            fb_info.framebuff,
                            fb_info.fb_width, 
                            fb_info.fb_height,
                            0, 0,
                            DISPLAY_SIZE_X,
                            FB_32B_RGB8888); 
        }
        else if(videoFormatType == FORMAT_RGB888)
        {
            rgb888_to_rgbxxx(  startAddr,
		                        len,
		                        fb_info.framebuff,
		                        fb_info.fb_width, 
		                        fb_info.fb_height,
		                        0, 0,
		                        DISPLAY_SIZE_X,
		                        FB_32B_RGB8888);
        }
		else if(videoFormatType == FORMAT_MJPEG)
		{
			mjpeg_to_rgbxxx(  startAddr,
                        		len,
                        		fb_info.framebuff,
                        		fb_info.fb_width, 
		                        fb_info.fb_height,
                        		0, 0,
                        		FB_32B_RGB8888);
		}       
    }
    else if(fb_info.fb_depth == 24)
    {
        if(videoFormatType == FORMAT_YUYV)
        {
            yuyv_to_rgbxxx(  startAddr,
                        len,
                        fb_info.framebuff,
                        fb_info.fb_width, 
                        fb_info.fb_height,
                        0, 0,
                        DISPLAY_SIZE_X,
                        FB_24B_RGB888); 
        }
        else if(videoFormatType == FORMAT_RGB888)
        {
            rgb888_to_rgbxxx(  startAddr,
                        len,
                        fb_info.framebuff,
                        fb_info.fb_width, 
                        fb_info.fb_height,
                        0, 0,
                        DISPLAY_SIZE_X,
                        FB_24B_RGB888);
        }
		else if(videoFormatType == FORMAT_MJPEG)
		{
			mjpeg_to_rgbxxx(  startAddr,
                        		len,
                        		fb_info.framebuff,
                        		fb_info.fb_width, 
		                        fb_info.fb_height,
                        		0, 0,
                        		FB_24B_RGB888);
		}      
    }
    else if(fb_info.fb_depth == 16)
    {
        if(videoFormatType == FORMAT_YUYV)
        {
            yuyv_to_rgbxxx(   startAddr,
                                len,
                                fb_info.framebuff,
                                fb_info.fb_width, 
                                fb_info.fb_height,
                                0, 0,
                                DISPLAY_SIZE_X,
                                FB_16B_RGB565);
        }
        else if(videoFormatType == FORMAT_RGB888)
        {
            rgb888_to_rgbxxx(  startAddr,
                        len,
                        fb_info.framebuff,
                        fb_info.fb_width, 
                        fb_info.fb_height,
                        0, 0,
                        DISPLAY_SIZE_X,
                        FB_16B_RGB565);
        }
		else if(videoFormatType == FORMAT_MJPEG)
		{
			mjpeg_to_rgbxxx(  startAddr,
                        		len,
                        		fb_info.framebuff,
                        		fb_info.fb_width, 
		                        fb_info.fb_height,
                        		0, 0,
                        		FB_16B_RGB565);
		}    
    }    
   // printf("2.%ld\r\n",GetTime_Ms());
}  
#endif
  


