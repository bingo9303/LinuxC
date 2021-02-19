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


	Mat src = imread(argv[1],1);		//Ĭ�ϼ��ز�ɫͼ��0��ʾGRAY�Ҷ�ͼ��1ΪBGR��ɫͼ��-1��ʾ����ԭͼ������������������HSV�������ռ䣩

	gettimeofday(&decode_tv,NULL);
	printf("2.%ld\r\n",decode_tv.tv_usec);

	if (src.empty()){
		cout << "could not load image..." << endl;
		getchar();
		return -1;
	}

	//�����Ƕ�ȡͼƬ̫��ʱ��������ȫ�֣�ʹ�ô��ں����������ô�С
	namedWindow("input", WINDOW_AUTOSIZE);	//WINDOW_FREERATIO�������Ե������ڴ�С��Ĭ��ͼ��ΪWINDOW_AUTOSIZE��ʾԭͼ�����ܵ�����С��
	gettimeofday(&decode_tv,NULL);
	printf("3.%ld\r\n",decode_tv.tv_usec);
	
	imshow("input", src);					//����namedWindow��ֻ��imshow����ʾ��ͼ�񴰿���ͼƬһ�����޷��������ڴ�С
										//imshowֻ����ʾ8λ�͸����͵�
	gettimeofday(&decode_tv,NULL);
	printf("4.%ld\r\n",decode_tv.tv_usec);
	waitKey(0);				//opencv�Դ�����������0��ʾһֱ������1��ʾ�ӳ�1�����ִ����һ��
	destroyAllWindows();	//��������ǰ�����д�������
	return 0;
}


