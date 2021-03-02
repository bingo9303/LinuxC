#include "define.h"
void  usage(FILE *fp,int argc,char **argv)  
{  
    fprintf (fp,  
                "Usage: %s [options]\n\n"  
                "Options:\n"  
                "-d | --device name		Video device name [/dev/video]\n"  
                "-h | --help			Print this message\n"  
                "-f | --format			Input Video Format:mjpeg or rawvideo\n"  
                "-s | --size			UVC Device Input Size\n"
                "-r | --frameRate		UVC Device Input frameRate\n"
                "-W | --Width			Window Display Width\n"
                "-H | --Height			Window Display Height\n"
                "version : %d.%02d			author : RGBLink/bingo\n"
                "",argv[0],SOFT_VERSION_HIGH,SOFT_VERSION_LOW);  
}  

 



  
static const char short_options [] = "d:hf:s:r:W:H:";  
  
static const struct option long_options [] = {  
        { "device",     required_argument,      NULL,           'd' },  
        { "help",       no_argument,            NULL,           'h' },  
        { "format",		no_argument,            NULL,           'f' },
        { "size",     	no_argument,            NULL,           's' },
       	{ "framerate", 	no_argument,            NULL,           'r' },
        { "Width",     	no_argument,            NULL,           'W' },
        { "Height",     no_argument,            NULL,           'H' },
        { 0, 0, 0, 0 }  
};  


PraseCmdInfo::PraseCmdInfo()
{
	int i;
	input_ch_num = 0;
	inputSize_index = 0;
	inputFormat_index = 0;
	inputFramerate_index = 0;


	for(i=0;i<MAX_INPUT_CH;i++)
	{
		dev[i].ch = i;
		memset(dev[i].devName,0,100);
		sprintf(dev[i].input_size,"%s","1920x1080");
		sprintf(dev[i].input_format,"%s","mjpeg");
		sprintf(dev[i].input_framerate,"%d",30);
	}

	dev[0].output_width = 1920;
	dev[0].output_height = 1080;

	dev[1].output_width = 640;
	dev[1].output_height = 480;
	
	screen_width = 1920;
	screen_height = 1080;

}




void PraseCmdInfo::parse_cmdInfo(int argc, char* argv[])
{
	for (;;) 
    {  
        int index;  
        int c;  
            
        c = getopt_long (argc, argv,(const char*)short_options, (const option*)long_options,&index);  

        if (-1 == c)  
                break;  

        switch (c) {  
			
	        case 0: /* getopt_long() flag */  
	                break;  
	        case 'd':  
					strcpy((char*)dev[input_ch_num].devName,optarg);
					input_ch_num++;
	                break;  
	        case 'h':  
	                usage (stdout, argc, argv);  
	                exit (EXIT_SUCCESS);
					break;
			case 's':
					strcpy((char*)dev[inputSize_index].input_size,optarg);
					inputSize_index++;
					break;
			case 'r':
					sprintf(dev[inputFramerate_index].input_framerate,"%d",atoi(optarg));
					inputFramerate_index++;						
					break;	
			case 'H':
					screen_height = atoi(optarg);
					break;
			case 'W':
					screen_width = atoi(optarg);
					break;
			
	        case 'f':
	              if((!strcmp("rawvideo",optarg)) || (!strcmp("mjpeg",optarg)))
	              {
	              	strcpy((char*)dev[inputFormat_index].input_format,optarg);
	              }
	              else
	              {
	                usage (stderr, argc, argv);  
	                exit (EXIT_FAILURE);
	              }
				  inputFormat_index++;
	              break;
	        default:  
	                usage (stderr, argc, argv);  
	                exit (EXIT_FAILURE);  
        }  
    }  

	if(input_ch_num == 0)
	{
		printf("Please Input Device Name\r\n");
	}

	
}








