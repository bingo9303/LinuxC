#include "define.h"

struct _display_pool display_pool[MAX_INPUT_UVC_CH][DISPLAY_BUFFER_FRAME_NUM];
int display_pool_read[MAX_INPUT_UVC_CH] = {0};
int	display_pool_write[MAX_INPUT_UVC_CH] = {0};


void malloc_decode_pool(struct _uvcCameraDev* dev)
{
	char i,j;
	for(i=0;i<dev->uvc_ch_num;i++)
	{
		for(j=0;j<DISPLAY_BUFFER_FRAME_NUM;j++)
		{
			display_pool[i][j].addr = malloc(sizeof(unsigned char)*POOL_MAX_MALLOC_SIZE*3);
			display_pool[i][j].flag = 0;
			if(display_pool[i][j].addr == NULL)
			{
				fprintf (stderr, "[%d]malloc display [%d] buffer faild\n",i,j);  
		        exit (EXIT_FAILURE);  
			}
		}

	}
}

void free_decode_pool(struct _uvcCameraDev* dev)
{
	char i,j;
	for(i=0;i<dev->uvc_ch_num;i++)
	{
		for(j=0;j<DISPLAY_BUFFER_FRAME_NUM;j++)
		{
			free(display_pool[i][j].addr);
		}
	}
}







void* task_decode_0(void* args) 
{
	unsigned char* startAddr;
	unsigned int len,oneLineByteSize;
	struct v4l2_buffer* buf ;
	struct _buffer_info* buffer_info;
	struct _uvcCameraDev* dev = (struct _uvcCameraDev*)args;
	pid_t pid = gettid();
 	pthread_t tid = pthread_self();
	printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);

	#if ENABLE_MANUAL_SET_CUP
		char cpuIndex;
		int res;
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
	    CPU_SET(4, &cpuset);
		res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
		if (res != 0)
		{
			printf("pthread_setaffinity_np faild\r\n");
		}
	#endif
	
	while(1)
	{
		sem_wait(&decode_sem[0]);
	#if ((ENABLE_DISPLAY_PTHREAD == 1) && (ENABLE_OPTIMIZE == 1))
		while((display_pool[0][display_pool_write[0]].flag == 1))
		{
			usleep(100);
		}
	#endif	
		buf = &decode_pool[0][decode_pool_read[0]];
		int v4l2_index = buf->index;
		
		buffer_info = &dev->buffer_info[0][v4l2_index];
		startAddr = buffer_info->start;
		len = buffer_info->length;
		decode_pool_read[0] = POS_OFFSET_ADD(decode_pool_read[0],1,DECODE_BUFFER_FRAME_NUM);

		struct jpeg_decompress_struct cinfo;
    	struct jpeg_error_mgr jerr;

		
		cinfo.err = jpeg_std_error(&jerr);
		cinfo.jpeg_color_space = JCS_RGB;//libjpeg解出来的像素格式为rgb
		cinfo.dct_method = JDCT_FASTEST;
		jpeg_create_decompress(&cinfo);

		jpeg_mem_src (&cinfo,startAddr,len);
		jpeg_read_header(&cinfo, TRUE);

		jpeg_start_decompress(&cinfo);
	
	//	printf("cinfo.output_height = %d,cinfo.output_width = %d,cinfo.output_components = %d\r\n",
				//	cinfo.output_height,cinfo.output_width,cinfo.output_components);

		
		unsigned char*  bufferPtr;
		unsigned int lineIndex = 0;
		oneLineByteSize = cinfo.output_width * cinfo.output_components;
		dev->oneLineByteSize[0][0] = oneLineByteSize;
		dev->imageSize_width[0] = cinfo.output_width;
		dev->imageSize_height[0] = cinfo.output_height;
		
		while (cinfo.output_scanline < cinfo.output_height) 
		{	
			bufferPtr = &display_pool[0][display_pool_write[0]].addr[lineIndex * oneLineByteSize];
			jpeg_read_scanlines(&cinfo, &bufferPtr, 1);
			lineIndex++;
		}
		

		jpeg_finish_decompress(&cinfo);
	    jpeg_destroy_decompress(&cinfo);

		if (-1 == xioctl (dev->fd[0], VIDIOC_QBUF, buf)) 
	    {
			fprintf (stderr, "[%d] VIDIOC_QBUF faild\n",0);
			xioctl (dev->fd[0], VIDIOC_QBUF, buf);
	       // exit (EXIT_FAILURE);
		}

#if ENABLE_DEBUG_BINGO
		printf("2.display_pool_write[%d] = %d\r\n",0,display_pool_write[0]);
#endif
		display_pool[0][display_pool_write[0]].flag = 1;
		display_pool_write[0] = POS_OFFSET_ADD(display_pool_write[0],1,DISPLAY_BUFFER_FRAME_NUM);
		
	#if ENABLE_DISPLAY_PTHREAD
		sem_post(&display_sem[0]);
	#else
		display_0(dev);
	#endif			
	}

}



void* task_decode_1(void* args)
{
#if 1
	unsigned char* startAddr;
	unsigned int len,oneLineByteSize;
	struct v4l2_buffer* buf ;
	struct _buffer_info* buffer_info;
	struct _uvcCameraDev* dev = (struct _uvcCameraDev*)args;
	pid_t pid = gettid();
 	pthread_t tid = pthread_self();
	printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);
	
	#if ENABLE_MANUAL_SET_CUP
		char cpuIndex;
		int res;
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
	    CPU_SET(5, &cpuset);
		
		res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
		if (res != 0)
		{
			printf("pthread_setaffinity_np faild\r\n");
		}
	#endif

	while(1)
	{
		sem_wait(&decode_sem[1]);
	#if ((ENABLE_DISPLAY_PTHREAD == 1) && (ENABLE_OPTIMIZE == 1))
		while((display_pool[1][display_pool_write[1]].flag == 1))
		{
			usleep(100);
		}
	#endif	
		buf = &decode_pool[1][decode_pool_read[1]];
		int v4l2_index = buf->index;
		
		buffer_info = &dev->buffer_info[1][v4l2_index];
		startAddr = buffer_info->start;
		len = buffer_info->length;
		decode_pool_read[1] = POS_OFFSET_ADD(decode_pool_read[1],1,DECODE_BUFFER_FRAME_NUM);

		struct jpeg_decompress_struct cinfo;
    	struct jpeg_error_mgr jerr;

		
		cinfo.err = jpeg_std_error(&jerr);
		cinfo.jpeg_color_space = JCS_RGB;//libjpeg解出来的像素格式为rgb
		cinfo.dct_method = JDCT_FASTEST;
		jpeg_create_decompress(&cinfo);

		jpeg_mem_src (&cinfo,startAddr,len);
		jpeg_read_header(&cinfo, TRUE);

		jpeg_start_decompress(&cinfo);
	
	//	printf("cinfo.output_height = %d,cinfo.output_width = %d,cinfo.output_components = %d\r\n",
				//	cinfo.output_height,cinfo.output_width,cinfo.output_components);

		
		unsigned char*  bufferPtr;
		unsigned int lineIndex = 0;
		oneLineByteSize = cinfo.output_width * cinfo.output_components;
		dev->oneLineByteSize[1][0] = oneLineByteSize;
		dev->imageSize_width[1] = cinfo.output_width;
		dev->imageSize_height[1] = cinfo.output_height;
		
		while (cinfo.output_scanline < cinfo.output_height) 
		{	
			bufferPtr = &display_pool[1][display_pool_write[1]].addr[lineIndex * oneLineByteSize];
			jpeg_read_scanlines(&cinfo, &bufferPtr, 1);
			lineIndex++;
		}
		

		jpeg_finish_decompress(&cinfo);
	    jpeg_destroy_decompress(&cinfo);

		if (-1 == xioctl (dev->fd[1], VIDIOC_QBUF, buf)) 
	    {
			fprintf (stderr, "[%d] VIDIOC_QBUF faild\n",1);  
			xioctl (dev->fd[1], VIDIOC_QBUF, buf);
	       // exit (EXIT_FAILURE);
		}

#if ENABLE_DEBUG_BINGO
		printf("2.display_pool_write[%d] = %d\r\n",1,display_pool_write[1]);
#endif
		display_pool[1][display_pool_write[1]].flag = 1;
		display_pool_write[1] = POS_OFFSET_ADD(display_pool_write[1],1,DISPLAY_BUFFER_FRAME_NUM);
	#if ENABLE_DISPLAY_PTHREAD
		sem_post(&display_sem[1]);
	#else
		display_1(dev);
	#endif			
	}
#endif
}




















