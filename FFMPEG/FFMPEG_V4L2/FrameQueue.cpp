#include "define.h"


FrameQueue::FrameQueue()
{
	nb_frames = 0;

	mutex     = SDL_CreateMutex();
	cond      = SDL_CreateCond();
}




bool FrameQueue::enQueue(const AVFrame* frame)
{
	
	if(queue.size() >= BUFFER_NUM)
	{
		SDL_LockMutex(mutex);
		AVFrame* tmp = queue.front();
		queue.pop();
		av_frame_unref(tmp);
		av_frame_free(&tmp);
		
		tmp = av_frame_alloc();
		av_frame_ref(tmp, frame);
		queue.push(tmp);
		nb_frames = queue.size();
		SDL_CondSignal(cond);
		SDL_UnlockMutex(mutex);
	}
	else
	{
		AVFrame* p = av_frame_alloc();
		av_frame_ref(p, frame);
		SDL_LockMutex(mutex);
		queue.push(p);
		nb_frames++;
		SDL_CondSignal(cond);
		SDL_UnlockMutex(mutex);
	}
	
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
			AVFrame* temp = queue.front();
			av_frame_ref(*frame, temp);
			queue.pop();
			av_frame_unref(temp);
			av_frame_free(&temp);
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