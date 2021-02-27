#include "define.h"


FrameQueue::FrameQueue()
{
	int i;
	nb_frames = 0;

	for(i=0;i<BUFFER_NUM;i++)
	{
		frameBuffer[i].useFlag = 0;
		frameBuffer[i].frame = av_frame_alloc();
	}

	mutex     = SDL_CreateMutex();
	cond      = SDL_CreateCond();
}


FrameQueue::~FrameQueue()
{
	int i;
	for(i=0;i<BUFFER_NUM;i++)
	{
		av_frame_free(&frameBuffer[i].frame);
	}
}


struct _frameBuffer* FrameQueue::getEmptyFrame(void)
{
	int i;
	for(i=0;i<BUFFER_NUM;i++)
	{
		if(frameBuffer[i].useFlag == 0)
		{
			return &frameBuffer[i];
		}
	}
	return NULL;
}


bool FrameQueue::enQueue(const AVFrame* frame)
{
	SDL_LockMutex(mutex);
	
	struct _frameBuffer* temp = getEmptyFrame();
	if(temp == NULL)
	{
		temp = queue.front();
		queue.pop();
		av_frame_unref(temp->frame);
		av_frame_ref(temp->frame, frame);
		queue.push(temp);
		nb_frames = queue.size();
	}
	else
	{
		av_frame_ref(temp->frame, frame);
		temp->useFlag = 1;
		queue.push(temp);
		nb_frames++;
	}



	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);


/*

	if(queue.size() >= 4)
	{
		SDL_LockMutex(mutex);
		
		AVFrame* tmp = queue.front();
		queue.pop();
		
		if (av_frame_ref(tmp, frame) < 0)	return false;
			
		queue.push(tmp);

		nb_frames = queue.size();

		SDL_CondSignal(cond);
		SDL_UnlockMutex(mutex);
	}
	else
	{
		AVFrame* p = av_frame_alloc();

		int ret = av_frame_ref(p, frame);
		if (ret < 0)
			return false;

		//p->opaque = (void *)new double(*(double*)p->opaque); //上一个指向的是一个局部的变量，这里重新分配pts空间

		SDL_LockMutex(mutex);
		queue.push(p);

		nb_frames++;
		
		SDL_CondSignal(cond);
		SDL_UnlockMutex(mutex);
	}*/
	return true;
}

bool FrameQueue::deQueue(AVFrame **frame)
{
	bool ret = true;

	SDL_LockMutex(mutex);
	while (true)
	{
		if (!queue.empty())
		{
			struct _frameBuffer* temp = queue.front();
			av_frame_ref(*frame, temp->frame);
			temp->useFlag = 0;
			queue.pop();
		//	av_frame_unref(temp->frame);
		//	av_frame_free(&tmp);
			nb_frames--;
			ret = true;
			break;
		}
		else
		{
			SDL_CondWait(cond, mutex);
		}
	}

	SDL_UnlockMutex(mutex);
	return ret;
}