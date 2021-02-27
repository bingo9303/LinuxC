#include "define.h"

extern bool quit;

PacketQueue::PacketQueue()
{
	nb_packets = 0;
	size       = 0;
	

	mutex      = SDL_CreateMutex();
	cond       = SDL_CreateCond();
}




bool PacketQueue::enQueue(const AVPacket *packet)
{
	
	if(queue.size() >= BUFFER_NUM)
	{
		SDL_LockMutex(mutex);
		AVPacket* pkt = queue.front();
		queue.pop();
		av_packet_unref(pkt);
		av_packet_free(&pkt);
		
		
		pkt = av_packet_alloc();
		av_packet_ref(pkt, packet);
		queue.push(pkt);
		nb_packets = queue.size();
		SDL_CondSignal(cond);
		SDL_UnlockMutex(mutex);
	}
	else
	{
		AVPacket *pkt = av_packet_alloc();
		av_packet_ref(pkt, packet);
		SDL_LockMutex(mutex);
		queue.push(pkt);
		size += pkt->size;
		nb_packets++;
		SDL_CondSignal(cond);
		SDL_UnlockMutex(mutex);
	}
	
	
	return true;
}

bool PacketQueue::deQueue(AVPacket *packet, bool block)
{
	bool ret = false;

	SDL_LockMutex(mutex);
	while (true)
	{
		if (quit)
		{
			ret = false;
			break;
		}

		if (!queue.empty())
		{
			AVPacket* pkt = queue.front();
			av_packet_ref(packet, pkt);
			queue.pop();
			av_packet_unref(pkt);
			av_packet_free(&pkt);
			nb_packets--;
			size -= packet->size;
			ret = true;
			break;
		}
		else if (!block)
		{
			ret = false;
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