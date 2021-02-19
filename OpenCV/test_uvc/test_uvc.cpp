#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"
#include <iostream>
 
using namespace cv;
using namespace std;
 
 
int main()
{
    cout << "Built with OpenCV " << CV_VERSION << endl;
 
	VideoCapture capture(12);
	if(!capture.isOpened())
	{
		cout << "open camera failed. " << endl;
		return -1;
	}
	
	while(true)
	{
		Mat frame;
		capture >> frame;
		if(!frame.empty())
		{
			imshow("camera", frame);
		}
		
		if(waitKey(1) > 0)
		{
			break;
		}
	}
 
    return 0;
}
