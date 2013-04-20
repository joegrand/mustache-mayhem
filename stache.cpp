/**
 * Based on https://code.ros.org/trac/opencv/browser/trunk/opencv/samples/cpp/tutorial_code/objectDetection/objectDetection2.cpp?rev=6553
 */
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgproc/imgproc_c.h"

#include <iostream>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace std;
using namespace cv;

const char copyright[] = "\
 IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. \n\
 \n\
 By downloading, copying, installing or using the software you agree to this license.\n\
 If you do not agree to this license, do not download, install, copy or use the software.\n\
\n\
\n\
                          License Agreement\n\
               For Open Source Computer Vision Library\n\
\n\
Copyright (C) 2000-2008, Intel Corporation, all rights reserved.\n\
Copyright (C) 2008-2011, Willow Garage Inc., all rights reserved.\n\
Copyright (C) 2012, Texas Instruments, all rights reserved.\n\
Third party copyrights are property of their respective owners.\n\
\n\
Redistribution and use in source and binary forms, with or without modification,\n\
are permitted provided that the following conditions are met:\n\
\n\
  * Redistributions of source code must retain the above copyright notice,\n\
    this list of conditions and the following disclaimer.\n\
\n\
  * Redistributions in binary form must reproduce the above copyright notice,\n\
    this list of conditions and the following disclaimer in the documentation\n\
    and/or other materials provided with the distribution.\n\
\n\
  * The name of the copyright holders may not be used to endorse or promote products\n\
    derived from this software without specific prior written permission.\n\
\n\
This software is provided by the copyright holders and contributors \"as is\" and\n\
any express or implied warranties, including, but not limited to, the implied\n\
warranties of merchantability and fitness for a particular purpose are disclaimed.\n\
In no event shall the Intel Corporation or contributors be liable for any direct,\n\
indirect, incidental, special, exemplary, or consequential damages\n\
(including, but not limited to, procurement of substitute goods or services;\n\
loss of use, data, or profits; or business interruption) however caused\n\
and on any theory of liability, whether in contract, strict liability,\n\
or tort (including negligence or otherwise) arising in any way out of\n\
the use of this software, even if advised of the possibility of such damage.\n\
\n";

/** Function Headers */
void detectAndDisplay(Mat frame);
void saveFrame(Mat frame);
int inputAvailable();
void inputSetup(int setup);

/** Global variables */
String face_cascade_name = "lbpcascade_frontalface.xml";
CascadeClassifier face_cascade;
const char * window_name = "stache - BeagleBone OpenCV demo";
IplImage* mask = 0;

/** Command-line arguments */
int numCamera = -1;
const char* stacheMaskFile = "stache-mask.png";
int scaleHeight = 6;
int offsetHeight = 4;
int camWidth = 0;
int camHeight = 0;
float camFPS = 0;

int savedFrames = 0;

/**
 * @function main
 */
int main(int argc, const char** argv) {
  CvCapture* capture;
  Mat frame;

  if(argc > 1) numCamera = atoi(argv[1]);
  if(argc > 2) stacheMaskFile = argv[2];
  if(argc > 3) scaleHeight = atoi(argv[3]);
  if(argc > 4) offsetHeight = atoi(argv[4]);
  if(argc > 5) camWidth = atoi(argv[5]);
  if(argc > 6) camHeight = atoi(argv[6]);
  if(argc > 7) camFPS = (float)atof(argv[7]);

  //-- 0. Print the copyright
  fprintf(stderr, "%s", copyright);

  //-- 1. Load the cascade
  if( !face_cascade.load(face_cascade_name) ){ fprintf(stderr, "--(!)Error loading\n"); exit(-1); };

  //-- 1a. Load the mustache mask
  mask = cvLoadImage(stacheMaskFile);
  if(!mask) { fprintf(stderr, "Could not load %s\n", stacheMaskFile); exit(-1); }

  //-- 2. Read the video stream
  capture = cvCaptureFromCAM(numCamera);
  if(camWidth) cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, camWidth);
  if(camHeight) cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, camHeight);
  if(camFPS) cvSetCaptureProperty(capture, CV_CAP_PROP_FPS, camFPS);
  if(capture) {
    inputSetup(1);
    while(true) {
      frame = cvQueryFrame(capture);
  
      //-- 3. Apply the classifier to the frame
      try {
       if(!frame.empty()) {
        detectAndDisplay( frame );
       } else {
        fprintf(stderr, " --(!) No captured frame -- Break!\n"); exit(-2);
       }

       char c = waitKey(10);

       struct stat stFileInfo;
       char *path = "/tmp/foo";
       int intStat;

       // Attempt to get the file attributes
       intStat = stat(path,&stFileInfo);
       if(intStat == 0) {
         // We were able to get the file attributes
         // so the file obviously exists.
         fprintf(stderr, "File found, taking pic\n"); 
         saveFrame(frame);
         unlink(path);
        }
       } catch(cv::Exception e) {
       }
    }
    inputSetup(0);
  } else {
    exit(-3);
  }
  exit(0);
}

/**
 * @function detectAndDisplay
 */
void detectAndDisplay(Mat frame) {
  std::vector<Rect> faces;
  Mat frame_gray;
  int i = 0;

  cvtColor(frame, frame_gray, CV_BGR2GRAY);
  equalizeHist(frame_gray, frame_gray);

  //-- Detect faces
  face_cascade.detectMultiScale(frame_gray, faces, 1.1, 2, 0, Size(80, 80));

  for(; i < faces.size(); i++) {
    //-- Scale and apply mustache mask for each face
    Mat faceROI = frame_gray(faces[i]);
    IplImage iplFrame = frame;
    IplImage *iplMask = cvCreateImage(cvSize(faces[i].width, faces[i].height/scaleHeight),
      mask->depth, mask->nChannels );
    cvSetImageROI(&iplFrame, cvRect(faces[i].x, faces[i].y + (faces[i].height/scaleHeight)*offsetHeight,
      faces[i].width, faces[i].height/scaleHeight));
    cvResize(mask, iplMask, CV_INTER_LINEAR);
    cvSub(&iplFrame, iplMask, &iplFrame);
    cvResetImageROI(&iplFrame);
  }

  //-- Show what you got
  flip(frame, frame, 1);
  imshow(window_name, frame);

  //-- 0a. Attempt to resize window
  cvResizeWindow(window_name, camWidth, camHeight);
}


void saveFrame(Mat frame) {
  char filename[40];
  flip(frame, frame, 1); // de-mirror-image-ize
  IplImage iplFrame = frame;
  sprintf(filename, "/home/root/stache/tmp/captured%03d.jpg", savedFrames);
  cvSaveImage(filename, &iplFrame);
  fprintf(stdout, "{\"tweet\":\"New BeagleStache captured!\",\"filename\":\"%s\"}\n", filename);
  fflush(stdout);
  savedFrames++;
  if(savedFrames >= 1000) savedFrames = 0;
}

int inputAvailable()  
{
  struct timeval tv;
  int c;
  fd_set fds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
  if(FD_ISSET(0, &fds)) {
   c = getchar();
   return c;
  } else {
   return 0;
  }
}

void inputSetup(int setup)  
{
  static struct termios oldt, newt;
  if(setup) {
   tcgetattr(STDIN_FILENO, &oldt);
   newt = oldt;
   newt.c_lflag &= ~(ICANON);
   tcsetattr( STDIN_FILENO, TCSANOW, &newt);
  } else {
   tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
  }
}
