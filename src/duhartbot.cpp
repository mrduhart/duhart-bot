// "Duhart Bot" Control of a 2 DOF robotic arm by camera, based on color recognition
// Arm built with Bioloid Premium parts
// Draw a rectangle on the object to track
// Right click to change between learning algorithms: statistical (default) and backprojection
// Press r (lower case) to make robot arm follow
// Press ESC to terminate program
// Bronson Duhart
// 2014

// Vision libraries
#include <opencv/cv.h>
#include <opencv\highgui.h>
#include <opencv2\opencv.hpp>
#include <opencv2\core\core.hpp>
#include <opencv2\highgui\highgui.hpp>
// Processing ang text libraries
#include <sstream>
#include <string>
#include <iostream>
#include <math.h>
#include <complex>

using namespace std;
using namespace cv;

// Vision constants
#define FRAME_WIDTH			640
#define FRAME_HEIGHT		480
#define	MAX_NUM_OBJECTS		50
#define MIN_OBJECT_AREA		20*20
#define	MAX_OBJECT_AREA		FRAME_HEIGHT*FRAME_WIDTH/1.5

// Robot constants
#define L1					93
#define L2					125
#define	pi					3.14159
#define sgn(x)				( (x > 0) - (x < 0) ) 

// Vision variables
Mat sIm, hsv, binIm, isoIm, robIm, mask, roi, erMask, dilMask, mapX, mapY;
MatND hist, backproj;
Rect region;
VideoCapture cap;
Scalar hsvMean, hsvStdDev, hsvMin, hsvMax;
Point reg1, reg2;
// These are for the histogram
int h_bins = 30; int s_bins = 32; 
int histSize[] = {h_bins, s_bins};  
float h_range[] = {0, 179}; 
float s_range[] = {0, 255}; 
const float* ranges[] = {h_range, s_range}; 
int channels[] = {0, 1}; 
// And these, for the windows
string wdwCamera = "Duhart Bot";
string wdwHSV = "HSV";
string wdwBin = "Binary";
string wdwIso = "Isolated";
string wdwProj = "Projection";
string wdwRobot = "Robot";

// Robot and Vision variables
struct myPoint
{
	double x, y;
	myPoint()
		{
			x = y = 0.0;
	}
	myPoint(double p1, double p2)
		{
			x = p1;
			y = p2;
	}
	myPoint(int p1, int p2)
		{
			x = p1;
			y = p2;
	}
	myPoint(double p1)
		{
			x = p1;
			y = 0.0;
	}
	myPoint(int p1)
		{
			x = p1;
			y = 0.0;
	}
}
o0 = myPoint((double)FRAME_WIDTH/2, (double) 0.5*FRAME_HEIGHT), o1, o2, image, robot;
char key;
bool drawing = false, learnMethod = false, learn = true, arm = false;
double q1, q2, cos2;
double t;
int i, j;

// Vision procedures
void MouseRegion(int event, int x_m, int y_m, int flags, void* userdata);
void getFrames();
void putFrames();
void Setup();
void Learn(bool method);
void Recognize(bool method);
void Track();
void drawTarget(int x, int y, Mat &frame);

// Robot procedures
void NormCoords(myPoint &src, myPoint &dst, bool inverse);
void Kinematics(double c1, double c2, bool inverse);
void drawRobot();
void CM530();

// Auxiliary procedures
string intToString(int number);
void Save();


// ***********************************************************************************
// THE BIG BOY, main function
int main()
{
	Setup();

	while(key != 0x1B)
	{
		while(drawing)
		{
			putFrames();
			getFrames();
			Learn(learnMethod);
			waitKey(1);
		}
		putFrames();
		getFrames();
		Recognize(learnMethod);
		Track();
		Kinematics(image.x, image.y, true);
		drawRobot();

		if(key == 'r')
			arm ? arm = false : arm = true;
		if(arm)
			CM530();

		if(key == 'c')
		{
			cout<<"(x , y)  = "<<"("<<robot.x<<", "<<robot.y<<")"<<endl;
			cout<<"(q1, q2) = "<<"("<<q1<<", "<<q2<<")"<<endl;
		}

		key = waitKey(1);
		//Save();
	}

	destroyAllWindows();
	cap.release();
	system("pause");

	return 0;
}
// ***********************************************************************************

// THE ONES THAT MAKE THE HARD WORK!
// ***********************************************************************************
// Callback function to draw rectangle and change learning algorithm
void MouseRegion(int event, int x_m, int y_m, int flags, void* userdata)
{
    if  (event == EVENT_LBUTTONDOWN)
    {
		drawing = true;
		reg1.x = x_m;
		reg1.y = y_m;
    }
    else if (event == EVENT_MOUSEMOVE && drawing)
    {
		if (drawing == true)        
			rectangle(sIm, reg1, Point(x_m,y_m), Scalar(200,255,255), 1, CV_AA);
    }
	else if (event == EVENT_LBUTTONUP)
    {
		reg2.x = x_m;
		reg2.y = y_m;
		drawing = false;
		rectangle(sIm, reg1, Point(x_m,y_m), Scalar(200,255,255), 1, CV_AA);
    }
	if  (event == EVENT_RBUTTONDOWN && learnMethod)
	{
		learnMethod = false;
	}
	else if  (event == EVENT_RBUTTONDOWN && !learnMethod)
	{
		learnMethod = true;
	}
}

// Obtains frames from camera and saves where appropriate
void getFrames()
{
	cap>>sIm>>robIm;
	remap(sIm, sIm, mapX, mapY, CV_INTER_LINEAR);
	remap(robIm, robIm, mapX, mapY, CV_INTER_LINEAR);
	cvtColor(sIm, hsv, CV_BGR2HSV);
}

// Shows frames in their respective windows
void putFrames()
{
	imshow(wdwCamera, sIm);
	//imshow(wdwHSV, hsv);
	//imshow(wdwBin, binIm);
	//imshow(wdwProj, backproj);
	imshow(wdwIso, isoIm);
	//imshow(wdwRobot, robIm);
}

// Initializes windows, memory and many other things
void Setup()
{
	namedWindow(wdwCamera, WINDOW_AUTOSIZE);
	//namedWindow(wdwHSV, WINDOW_AUTOSIZE);
	//namedWindow(wdwBin, WINDOW_AUTOSIZE);
	//namedWindow(wdwProj, WINDOW_AUTOSIZE);
	namedWindow(wdwIso, WINDOW_AUTOSIZE);

	setMouseCallback(wdwCamera, MouseRegion, NULL);

	cap.open(0);
	cap.set(CV_CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);;
	cap.set(CV_CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);

	// Maps for remapping image later are created

	mapX.create(Size(FRAME_WIDTH, FRAME_HEIGHT), CV_32FC1);
	mapY.create(Size(FRAME_WIDTH, FRAME_HEIGHT), CV_32FC1);

	for( int j = 0; j < FRAME_HEIGHT; j++ )
		for( int i = 0; i < FRAME_WIDTH; i++ )
		{
			mapX.at<float>(j,i) = FRAME_WIDTH - i;
			mapY.at<float>(j,i) = j;
		}

	getFrames();

	calcHist(&hsv, 1, channels, mask, hist, 2, histSize, ranges, true, false );
	hsv.copyTo(binIm);
	binIm.setTo(Scalar::all(0));
	binIm.copyTo(backproj);
	binIm.copyTo(isoIm);
	erMask = Mat(19,19,CV_8U,Scalar(1));
	dilMask = Mat(15,15,CV_8U,Scalar(1));
}

// Time to learn what to follow through the defined algorithm
void Learn(bool method)
{
	// Based on normal distribution
	if(!method)
	{
		roi = hsv(Rect(reg1, reg2));
		meanStdDev(roi, hsvMean, hsvStdDev);
		for(i = 0; i < 3; i++)
		{
			hsvMin[i] = hsvMean[i] - 2.25*hsvStdDev[i];
			hsvMax[i] = hsvMean[i] + 2.25*hsvStdDev[i];
		}
	}

	// Based on backprojection
	else
	{
		region = Rect(reg1, reg2);  
		mask = Mat(Size(FRAME_WIDTH, FRAME_HEIGHT), CV_8UC1, Scalar::all(0));
		roi = Mat(Size(FRAME_WIDTH, FRAME_HEIGHT), CV_8UC1, Scalar::all(0));
		mask(region).setTo(Scalar::all(255));
		hsv.copyTo(roi, mask);
 
		// Get the Histogram and normalize it 
		calcHist(&roi, 1, channels, mask, hist, 2, histSize, ranges, true, false );  
		normalize( hist, hist, 0, 255, NORM_MINMAX, -1, Mat() );  
	}

	learn = false;
}

// Now show me how well you learned. Process image looking for specific color
void Recognize(bool method)
{
	// Normal distribution learning
	if(!method)
	{
		inRange(hsv, hsvMin, hsvMax, binIm);		
	}

	// Histogram learning
	else
	{
		// Get Backprojection 
		calcBackProject( &hsv, 1, channels, hist, backproj, ranges, 1, true ); 		
		threshold(backproj, binIm, 32, 255, THRESH_BINARY);				
	}
	erode(binIm, isoIm, erMask);
	dilate(binIm, isoIm, dilMask);
}

// Here we go...!
// *Not written by me, just edited
void Track()
{
	//these two vectors needed for output of findContours
	vector< vector<Point> > contours;
	vector<Vec4i> hierarchy;
	Mat temp;
	isoIm.copyTo(temp);

	//find contours of filtered image using openCV findContours function
	findContours(temp, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);

	//use moments method to find our filtered object
	double refArea = 0;
	bool objectFound = false;
	if (hierarchy.size() > 0) 
	{
		int numObjects = hierarchy.size();
        //if number of objects greater than MAX_NUM_OBJECTS we have a noisy filter
        if(numObjects < MAX_NUM_OBJECTS)
		{
			for (int i = 0; i >= 0; i = hierarchy[i][0])
			{
				Moments moment = moments((cv::Mat)contours[i]);
				double area = moment.m00;
				//if the area is less than 20 px by 20px then it is probably just noise
				//if the area is the same as the 3/2 of the image size, probably just a bad filter
				//we only want the object with the largest area so we safe a reference area each
				//iteration and compare it to the area in the next iteration.
                if(area > MIN_OBJECT_AREA && area < MAX_OBJECT_AREA && area > refArea)
				{
					image.x = moment.m10/area;
					image.y = moment.m01/area;
					objectFound = true;
					refArea = area;
				}

				else objectFound = false;
			}

			//let user know you found an object
			if(objectFound == true)
			{
				int baseline;
				Size tSize = getTextSize("Tracking Object", FONT_HERSHEY_COMPLEX, 1.2, 1, &baseline);
				putText(sIm, "Tracking Object", Point((FRAME_WIDTH - tSize.width)/2,50), FONT_HERSHEY_COMPLEX, 1.2, Scalar(200,255,255), 1);
				//draw object location on screen
				//drawTarget(image.x, image.y, sIm);
			}
		}

		else
		{
			int baseline;
			Size tSize = getTextSize("Too much noise!", FONT_HERSHEY_COMPLEX, 2, 2, &baseline);
			putText(sIm, "Too much noise!", Point((FRAME_WIDTH - tSize.width)/2,50), FONT_HERSHEY_COMPLEX, 1.2, Scalar(0,0,255), 2);
		}
	}
}

// Draws an aiming icon on the center of the object
// Not currently using, uncomment what needed if wanted (Yes, you'll have to read through code)
// Kidding, check putFrames() and Track() procedures
// *Not written by me either
void drawTarget(int x, int y, Mat &frame)
{
	Scalar target = Scalar(0,255,0);

	circle(frame, Point(x,y), 15, Scalar(0,200,0), 1);
	// This part is for avoiding memory violations if line goes out of camera frame
    if(y - 25 > 0)
		line(frame, Point(x,y), Point(x,y - 25), target, 1);
    else
		line(frame, Point(x,y), Point(x,0), target, 1);
    if(y + 25 < FRAME_HEIGHT)
		line(frame, Point(x,y), Point(x,y + 25), target, 1);
    else
		line(frame, Point(x,y), Point(x,FRAME_HEIGHT), target, 1);
    if(x - 25 > 0)
		line(frame, Point(x,y), Point(x-25,y), target, 1);
    else
		line(frame, Point(x,y), Point(0,y), target, 1);
    if(x + 25 < FRAME_WIDTH)
		line(frame, Point(x,y), Point(x+25,y), target, 1);
    else
		line(frame, Point(x,y), Point(FRAME_WIDTH,y), target, 1);

	putText(frame, intToString(x) + "," + intToString(y), Point(x + 5,y + 30), FONT_HERSHEY_COMPLEX, 0.5, Scalar(200,255,255), 1);
}

//Changes between coordinate frames
//Inverse == true: from camera to robot
//Inverse == false: from robot to camera
void NormCoords(myPoint &src, myPoint &dst, bool inverse)
{
	if(inverse)
	{
		dst.x = (src.x - o0.x)*(L1 + L2)/(o0.y);
		dst.y = (o0.y - src.y)*(L1 + L2)/(o0.y);

		// Restrictions of the workspace
		double x_base = 40.0; 
		if(abs(dst.x) <= x_base & dst.y <= 0.0)
			dst.x = sgn(dst.x)*x_base;
		if(dst.y < -92.5)
			dst.y = -92.5;
	}

	else
	{
		dst.x = o0.x + src.x*(o0.y)/(L1 + L2);
		dst.y = o0.y - src.y*(o0.y)/(L1 + L2);
	}
}

// Calculates the robot kinematics of c1 and c2
// Inverse == true: ... Really?... Ok... Inverse kinematics
// Inverse == false: Forward kinematics
void Kinematics(double c1, double c2, bool inverse)
{
	//Inverse kinematics
	if(inverse)
	{
		image = myPoint(c1, c2);
		NormCoords(image, robot, true);

		if(pow(robot.x,2) + pow(robot.y,2) > (L1 + L2)*(L1 + L2))
		{
			q2 = 0.0;
			q1 = -atan2(robot.x, robot.y);
		}

		else if(pow(robot.x,2) + pow(robot.y,2) < abs(L2 - L1)*abs(L2 - L1))
		{
			q2 = pi;
			q1 = -atan2(robot.x, robot.y);
		}

		else
		{
			cos2 = (pow(robot.x, 2) + pow(robot.y, 2) - L1*L1 - L2*L2)/(2*L1*L2);
			std::complex<double> sin2 = -sqrt(1.0 - pow(cos2, 2));
			q2 = atan2(sin2.real(), cos2);
			q1 = -atan2(robot.x, robot.y) - atan2(L2*sin(q2), L1 + L2*cos(q2));
		}

		if(abs(q1) >= 5*pi/6)
			q1 = sgn(q1)*5*pi/6;
		if(abs(q2) >= 5*pi/6)
			q2 = sgn(q2)*5*pi/6;
	}

	//Forward kinematics
	else
	{
		robot.x = -L1*sin(c1) - L2*sin(c1 + c2);
		robot.y = L1*cos(c1) + L2*cos(c1 + c2);
	}
}

// Draws a nice robot in the window to simulate the real one
// Useful for debugging and aiding user with training of how to control the robot
void drawRobot()
{
	myPoint base1 = myPoint(-45.0);
	myPoint base2 = myPoint(45.0);
	NormCoords(base1, base1, false);
	NormCoords(base2, base2, false);
	Point drawBase1 = Point(base1.x, base1.y);
	Point drawBase2 = Point(base2.x, base2.y);
	line(sIm, drawBase1, drawBase2, Scalar(50,50,50), 15);

	Kinematics(q1, q2, false);

	o1 = myPoint(-0.9*L1*sin(q1), 0.9*L2*cos(q1));
	NormCoords(o1, o1, false);
	Point drawO0 = Point(o0.x, o0.y);
	Point drawO1 = Point(o1.x, o1.y);
	line(sIm, drawO0, drawO1, Scalar(127,127,127), 10);
	line(sIm, drawO0, drawO1, Scalar(240,240,240), 2);
	
	robot.x *= 0.9;
	robot.y *= 0.9;
	NormCoords(robot, o2, false);
	Point drawO2 = Point(o2.x, o2.y);
	line(sIm, drawO1, drawO2, Scalar(127,127,127), 10);
	line(sIm, drawO1, drawO2, Scalar(240,240,240), 2);

	circle(sIm, drawO0, 10, Scalar(10,10,10), -1);
	circle(sIm, drawO1, 10, Scalar(10,10,10), -1);
	circle(sIm, drawO0, 5, Scalar(240,240,240), -1);
	circle(sIm, drawO1, 5, Scalar(240,240,240), -1);
}

// Serial communication with use of Zigbee SDK
// Due to complications with configuration, it uses an already built program.
// It was the only way to successfully communicate with CM530 controller
// TODO: Find a way to integrate the code of Send.exe with this one
void CM530()
{
	//q1 = -5*pi/6;
	//q2 = 5*pi/6;
	unsigned short GoalPos1 = (q1 + 5*pi/6)*(180/pi)*(1024/300);
	unsigned short GoalPos2 = (q2 + 5*pi/6)*(180/pi)*(1024/300); 

	// I sum error to the final position
	GoalPos1 += 30/512*GoalPos1 + 30;
	GoalPos2 += 30/512*GoalPos2 + 30;

	GoalPos2 |= 0x8000;

	string cmd = "Send " + intToString(GoalPos1) + " " + intToString(GoalPos2);
	//cout<<q1<<" "<<q2<<endl<<cmd<<endl;

	system(cmd.c_str());
}	

// This one helps conveting from integer to a string
string intToString(int number)
{
	stringstream ss;
	ss<<number;
	return ss.str();
}

// Auxiliary procedure to save a screen print of any window you like
// Sometimes you look funny in them... :) ... Also useful for future presentations
void Save()
{
	switch(key)
		{ 
		case 'i':
			;
		case 'I':
			imwrite("Isolated.jpg", isoIm);
			break;
		case 'c':
			;
		case 'C':
			imwrite("Camera.jpg", sIm);
			break;
		case 'b':
			;
		case 'B':
			imwrite("Binary.jpg", binIm);
			break;
		case 'h':
			;
		case 'H':
			imwrite("HSV.jpg", hsv);
			break;
		}
}
// ***********************************************************************************