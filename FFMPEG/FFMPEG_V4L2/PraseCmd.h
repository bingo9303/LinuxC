#ifndef PRASE_CMD_H
#define PRASE_CMD_H


#include "define.h"


struct devInfo
{
	char ch;
	char devName[100];
	char input_format[20];
	char input_size[20];
	char input_framerate[10];
	int output_width;
	int output_height;
};


struct PraseCmdInfo
{	
	char input_ch_num;
	char inputSize_index;
	char inputFormat_index;
	char inputFramerate_index;

	int screen_width;
	int screen_height;

	struct devInfo dev[MAX_INPUT_CH];

	PraseCmdInfo();
	void parse_cmdInfo(int argc, char* argv[]);
};




#endif

