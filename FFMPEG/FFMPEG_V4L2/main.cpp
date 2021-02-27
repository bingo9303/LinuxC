#include "define.h"

using namespace std;

bool quit = false;
pid_t gettid() {
 return syscall(SYS_gettid);
}


int main(int argv, char* argc[])
{
	char cpuIndex;
	int res;
	cpu_set_t cpuset;
	
	pid_t pid = gettid();
	pthread_t tid = pthread_self();
	printf("%s pid:%d,tid:%lu\r\n",__func__,pid,tid);
	CPU_ZERO(&cpuset);
	for (cpuIndex = 4; cpuIndex < 6; cpuIndex++)
		CPU_SET(cpuIndex, &cpuset);
	
	res = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
	

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

	char* filename = "/dev/video10";
	//char* filename = "F:\\test.rmvb";
	MediaState media(filename);
	if (media.openInput())
		SDL_CreateThread(decode_thread, "", &media); // 创建解码线程，读取packet到队列中缓存
	media.video->video_play(&media); // create video thread

	AVStream *video_stream = media.pFormatCtx->streams[media.video->stream_index];

	double video_duration = video_stream->duration * av_q2d(video_stream->time_base);

	cout << "video时长：" << video_duration << endl;

	SDL_Event event;
	while (true) // SDL event loop
	{
		SDL_WaitEvent(&event);
		
		switch (event.type)
		{
		case FF_QUIT_EVENT:
		case SDL_QUIT:
			quit = 1;
			SDL_Quit();

			return 0;
			break;

		case FF_REFRESH_EVENT:
			video_refresh_timer(&media);
			break;

		default:
			break;
		}
	}

	getchar();
	return 0;
}