#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <cstdlib>

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
#include <sys/syscall.h>


using namespace std;
using namespace cv;



	
#define POS_OFFSET_ADD(startpos,offset,maxSize)	(((startpos+offset) >= maxSize)?((startpos+offset)-maxSize):(startpos+offset))
#define POS_OFFSET_SUB(startpos,offset,maxSize) ((startpos < offset)?(maxSize - (offset-startpos)):(startpos-offset))


// MJPEG
char* get_camerasrc_mjpeg(int devIndex) {
#if 1
    // for T4, M4B
    const int cam_width=640;//1920;
    const int cam_height=480;//1080;
    const int cam_frames=30;
#else
    // for M4/M4v2
    const int cam_width=432;
    const int cam_height=240;
    const int cam_frames=30;
#endif

	// device=/dev/video6 io-mode=1 ! video/x-raw,format=NV12,width=800,height=448,framerate=30/1 ! videoconvert ! appsink


    static char str[255]={'\0'};
	snprintf(str, sizeof(str)-1
		, "v4l2src device=/dev/video%d io-mode=4 ! image/jpeg,width=%d,height=%d,framerate=%d/1 ! jpegdec ! videoconvert ! video/x-raw ! appsink sync=false"
		, devIndex
		, cam_width
		, cam_height
		, cam_frames
		);
/*
	snprintf(str, sizeof(str)-1
		, "v4l2src device=/dev/video6 io-mode=1 ! video/x-raw,format=NV12,width=%d,height=%d,framerate=%d/1 ! videoconvert ! appsink"
		, cam_width
		, cam_height
		, cam_frames
		);*/


	return str;
}

// NV12
char* get_camerasrc_nv12(int devIndex) {
    const int cam_width=640;
    const int cam_height=480;
    const int cam_frames=30;
    static char str[255]={'\0'};
	snprintf(str, sizeof(str)-1
		, "v4l2src device=/dev/video%d io-mode=4 ! videoconvert ! video/x-raw,format=NV12,width=%d,height=%d,framerate=%d/1 ! videoconvert ! video/x-raw,format=BGR ! appsink"
		, devIndex
		, cam_width
		, cam_height
		, cam_frames
		);
	return str;
}

#if 0
#define BUFFER_NUM	10

// fps counter begin


// fps counter end
sem_t		display_sem;


Mat frame[BUFFER_NUM];
int pool_read;// = 0;
int pool_write;// = 0;

VideoCapture *pCapture[3]={0};
pid_t gettid() {
 return syscall(SYS_gettid);
}

void* task_readFrame(void* args) 
{
	int hasError;

	{
		char cpuIndex;
		int res;
		
		pid_t pid = gettid();
	 	pthread_t tid = pthread_self();
		printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);
	
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
	    for (cpuIndex = 4; cpuIndex < 6; cpuIndex++)
	        CPU_SET(cpuIndex, &cpuset);
		
		res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
		if (res != 0)
		{
			printf("pthread_setaffinity_np faild\r\n");
		}
	}
	
	
	while (1) 
	{ 
		hasError = 0;
		
		*pCapture[0] >> frame[pool_write];
		if (frame[pool_write].empty()) 
		{
			printf("Fail to read frame.\n");
			hasError = 1;
			break;
		}
		if (hasError) {
			break;
		}
		POS_OFFSET_ADD(pool_write,1,BUFFER_NUM);
		
		sem_post(&display_sem);
		
		
		
		/*
		usleep(20000);
		printf("1111\r\n");*/
	} 
}


int main(int argc,char* argv[]) 
{ 
	// check args
    if(argc < 2) {
        printf("Please provide the number of cameras, it must be 1,2 or 3.\n");
        exit(0);
    }
	errno = 0;
	char *endptr;
	long int count = strtol(argv[1], &endptr, 10);
	if (endptr == argv[1] || errno == ERANGE) {
		std::cerr << "Invalid parameter: " << argv[1] << '\n';
		exit (1);
	}
	if (count < 1 || count > 3) {
		std::cerr << "The parameter must be 1,2,3" << '\n';
		exit (1);
	}

	// check qt5 env
	const char* s = getenv("QTDIR");
	if (!s) {
		std::cerr << "Please execute the following command first: " << '\n';
		std::cerr << "\t. setqt5env" << '\n';
		exit (1);
	}
	
	{
		char cpuIndex;
		int res;
		pid_t pid = gettid();
		pthread_t tid = pthread_self();
		printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);

		
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
	    for (cpuIndex = 4; cpuIndex < 6; cpuIndex++)
	        CPU_SET(cpuIndex, &cpuset);
		
		res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
		if (res != 0)
		{
			printf("pthread_setaffinity_np faild\r\n");
		}
	}

	
	char windowName[10];
	int hasError;
	int devIndex=10;   // dev: /dev/video10
	for (int i=0; i<count; i++) {
		// mjpg
		char cmd[200];
		sprintf(cmd,"rkisp device=/dev/video6 io-mode=4 ! video/x-raw,format=NV12,width=640,height=480,framerate=30/1 ! videoconvert ! appsink");
		//pCapture[i] = new VideoCapture(get_camerasrc_mjpeg(devIndex),cv::CAP_GSTREAMER);

		printf("%s",cmd);
		pCapture[i] = new VideoCapture(cmd,cv::CAP_GSTREAMER);
		if (!pCapture[i]->isOpened()) {
			printf("Fail to open camera.\n");
			exit (1);
		}
		// opencv window
		sprintf(windowName,"%d",i+1);
		namedWindow(windowName, 1);

		devIndex += 2;  // dev: /dev/video12, /dev/video14, etc ..
	}

	pthread_t 	readFrame;

	
	sem_init(&display_sem, 0, 0);
	pthread_create(&readFrame,NULL,task_readFrame,NULL);
	
	
	time_t start, end;
	int fps_counter = 0;
	double sec;
	double fps;
	
	time(&start);
	while (1) 
	{
		sem_wait(&display_sem);
		//imshow(windowName,frame[pool_read]);

		// fps counter begin
		time(&end);
		fps_counter++;
		sec = difftime(end, start);
		if (sec > 1) {
		fps = fps_counter/sec;
		printf("%.2f fps\n", fps);
		fps_counter=0;
		time(&start);}
		
		POS_OFFSET_ADD(pool_read,1,BUFFER_NUM);

		if (waitKey(1) & 0xFF == 'q') 
			break;
	}

	pthread_join(readFrame,NULL);
//	pthread_join(displayFrame,NULL);



	
	// release
	for (int i=0; i<count; i++) {
		pCapture[i]->release();
		delete pCapture[i];
	}
	return 0; 
}
#endif

#if 1
int main(int argc,char* argv[]) 
{ 
	// check args
    if(argc < 2) {
        printf("Please provide the number of cameras, it must be 1,2 or 3.\n");
        exit(0);
    }
	errno = 0;
	char *endptr;
	long int count = strtol(argv[1], &endptr, 10);
	if (endptr == argv[1] || errno == ERANGE) {
		std::cerr << "Invalid parameter: " << argv[1] << '\n';
		exit (1);
	}
	if (count < 1 || count > 3) {
		std::cerr << "The parameter must be 1,2,3" << '\n';
		exit (1);
	}

	// check qt5 env
	const char* s = getenv("QTDIR");
	if (!s) {
		std::cerr << "Please execute the following command first: " << '\n';
		std::cerr << "\t. setqt5env" << '\n';
		exit (1);
	}

	VideoCapture *pCapture[3]={0};
	char windowName[10];
	int hasError;
	int devIndex=12;   // dev: /dev/video10
	for (int i=0; i<count; i++) {
		// mjpg
		char cmd[200];
		sprintf(cmd,"rkisp device=/dev/video6 io-mode=4 ! video/x-raw,format=NV12,width=640,height=480,framerate=30/1 ! videoconvert ! appsink");
		pCapture[i] = new VideoCapture(cmd,cv::CAP_GSTREAMER);
	//	pCapture[i] = new VideoCapture(get_camerasrc_mjpeg(devIndex),cv::CAP_GSTREAMER);
		if (!pCapture[i]->isOpened()) {
			printf("Fail to open camera.\n");
			exit (1);
		}
		// opencv window
		sprintf(windowName,"%d",i+1);
		namedWindow(windowName, 1);

		devIndex += 2;  // dev: /dev/video12, /dev/video14, etc ..
	}

	// fps counter begin
	time_t start, end;
	int fps_counter = 0;
	double sec;
	double fps;
        time(&start);
	// fps counter end

	while (1) { 
		Mat frame;
		hasError = 0;
		for (int i=0; i<count; i++) 
		{
			*pCapture[i] >> frame;
			
			if (frame.empty()) {
				printf("Fail to read frame.\n");
				hasError = 1;
				break;
			}
			sprintf(windowName,"%d",i+1);
			imshow(windowName,frame);

			// only calc one camera
			if (i == 0) {
				// fps counter begin
				time(&end);
				fps_counter++;
				sec = difftime(end, start);
				if (sec > 1) {
					fps = fps_counter/sec;
					printf("%.2f fps\n", fps);
					fps_counter=0;
					time(&start);
                                }
				// fps counter end
			}
		}
		if (hasError) {
			break;
		}
		if (waitKey(/*30*/1) & 0xFF == 'q') 
			break;
	} 
	
	// release
	for (int i=0; i<count; i++) {
		pCapture[i]->release();
		delete pCapture[i];
	}
	return 0; 
}

#endif


