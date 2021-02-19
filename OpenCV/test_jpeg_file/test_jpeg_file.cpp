#include <opencv2/opencv.hpp>
#include <iostream>
#include <sys/time.h>


using namespace cv;
using namespace std;


int main(int argc, char *argv[])
{
	struct timeval decode_tv;
	
	if (argc < 2) 
    {
        printf("insuffient auguments");
        exit(-1);
    }
	
	
	gettimeofday(&decode_tv,NULL);
	printf("1.%ld\r\n",decode_tv.tv_usec);


	Mat src = imread(argv[1],1);		//默认加载彩色图像，0表示GRAY灰度图，1为BGR彩色图，-1表示加载原图（可能是其他类型如HSV等其他空间）

	gettimeofday(&decode_tv,NULL);
	printf("2.%ld\r\n",decode_tv.tv_usec);

	if (src.empty()){
		cout << "could not load image..." << endl;
		getchar();
		return -1;
	}

	//当我们读取图片太大时，看不到全局，使用窗口函数可以设置大小
	namedWindow("input", WINDOW_AUTOSIZE);	//WINDOW_FREERATIO参数可以调整窗口大小。默认图像为WINDOW_AUTOSIZE显示原图，不能调整大小。
	gettimeofday(&decode_tv,NULL);
	printf("3.%ld\r\n",decode_tv.tv_usec);
	
	imshow("input", src);					//若无namedWindow，只有imshow，显示的图像窗口与图片一样大，无法调整窗口大小
										//imshow只能显示8位和浮点型的
	gettimeofday(&decode_tv,NULL);
	printf("4.%ld\r\n",decode_tv.tv_usec);
	waitKey(0);				//opencv自带阻塞函数，0表示一直阻塞，1表示延迟1毫秒后执行下一步
	destroyAllWindows();	//结束程序前将所有窗口销毁
	return 0;
}


