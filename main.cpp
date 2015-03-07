//
//  main.cpp
//  Mustache Mayhem
//
//  Created by Joe Grand [www.grandideastudio.com]
//

/**
 * Code based heavily on:
 * www.element14.com/community/community/designcenter/single-board-computers/next-gen_beaglebone//blog/2013/06/01/boothstache-facial-hair-fun-with-beaglebone-black
 * Which, in turn, is based on:
 * http://code.ros.org/trac/opencv/browser/trunk/opencv/samples/cpp/tutorial_code/objectDetection/objectDetection2.cpp?rev=6553
 *
 */

// OpenCV
#include "opencv2/opencv.hpp"   // http://opencv.org/

// Fonts
#include "cairo/cairo.h"        // http://cairographics.org/
#include "cairo/cairo-ft.h"

#ifdef _OSX
	#include "freetype2/freetype.h" // http://www.freetype.org/
#else
	#include "freetype/freetype.h"
#endif
 
// SDL/Audio
#ifdef _OSX // 2.0.3 + https://stackoverflow.com/questions/22368202/xcode-5-crashes-when-running-an-app-with-sdl-2 + http://www.libsdl.org/projects/SDL_mixer/
    #include "SDL2/SDL.h"
    #include "SDL2_mixer/SDL_mixer.h"
#else // 1.2.15
    #include "SDL/SDL.h"
    #include "SDL/SDL_mixer.h"
#endif

// User Interface
#ifdef _OSX // Terminal/Keyboard
    #include "conio.h"          // http://linux-conioh.sourceforge.net/
#else       // GPIO
    #include "libsoc_gpio.h"    // https://github.com/jackmitch/libsoc
    #include "libsoc_debug.h"
#endif

#include <string>
#include <time.h>
#include <math.h>
#include <stdlib.h>

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
IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. \n\r\
\n\r\
By downloading, copying, installing or using the software you agree to this license.\n\r\
If you do not agree to this license, do not download, install, copy or use the software.\n\r\
\n\r\
\n\r\
License Agreement\n\r\
For Open Source Computer Vision Library\n\r\
\n\r\
Copyright (C) 2000-2008, Intel Corporation, all rights reserved.\n\r\
Copyright (C) 2008-2011, Willow Garage Inc., all rights reserved.\n\r\
Copyright (C) 2012, Texas Instruments, all rights reserved.\n\r\
Third party copyrights are property of their respective owners.\n\r\
\n\r\
Redistribution and use in source and binary forms, with or without modification,\n\r\
are permitted provided that the following conditions are met:\n\r\
\n\r\
* Redistributions of source code must retain the above copyright notice,\n\r\
this list of conditions and the following disclaimer.\n\r\
\n\r\
* Redistributions in binary form must reproduce the above copyright notice,\n\r\
this list of conditions and the following disclaimer in the documentation\n\r\
and/or other materials provided with the distribution.\n\r\
\n\r\
* The name of the copyright holders may not be used to endorse or promote products\n\r\
derived from this software without specific prior written permission.\n\r\
\n\r\
This software is provided by the copyright holders and contributors \"as is\" and\n\r\
any express or implied warranties, including, but not limited to, the implied\n\r\
warranties of merchantability and fitness for a particular purpose are disclaimed.\n\r\
In no event shall the Intel Corporation or contributors be liable for any direct,\n\r\
indirect, incidental, special, exemplary, or consequential damages\n\r\
(including, but not limited to, procurement of substitute goods or services;\n\r\
loss of use, data, or profits; or business interruption) however caused\n\r\
and on any theory of liability, whether in contract, strict liability,\n\r\
or tort (including negligence or otherwise) arising in any way out of\n\r\
the use of this software, even if advised of the possibility of such damage.\n\r\
\n\r";

/** Function Headers */
void detectAndDisplay(Mat frame);
void saveFrame(Mat frame);
int  mm_DisplayTitle(void);
char mm_GetController(void);
#ifndef _OSX
    int  mm_InitializeController(void);
#endif
int  mm_InitializeAudio(void);
int  mm_ConfigureOpenCV(void);
void mm_ConfigureFont(void);
void putTextCairo(
                  cv::Mat &targetImage,
                  std::string const &text,
                  cv::Point2d centerPoint,
                  cairo_font_face_t &fontFace,
                  double fontSize,
                  cv::Scalar textColor);
const char *int_to_binary(int x);


/** Global variables */
const char* window_name = "Mustache Mayhem";
String face_cascade_name = "lbpcascade_frontalface.xml";
CascadeClassifier face_cascade;
IplImage* mask = 0;
cairo_font_face_t *myfont_face;
int savedFrames = 0;        // Counter for saved images
Mix_Chunk* sound = NULL;	// Pointer to our sound
CvCapture* capture;         // Pointer to our video stream


/** Gameplay configuration */
const char* mmTitleFile = "mm-title.jpg";               // 800 x 480
const char* mmTitleSoundFile = "mm-sound-title.wav";    // http://www.youtube.com/watch?v=BIroEpKoPgA w/ vocals by Napoleon Dynamite
const char* mmEffectSoundFile = "mm-sound-effect.wav";  // Played when mustaches are acquired during the game
const char* mmHighScoreFile = "mm-highscore.txt";       // High score text file, ASCII 4 decimal digits only
const char* mmFontFile = "emulogic.ttf";                // Nintendo 8-bit font
const CvScalar mmColorBrown = {0, 0.2, 0.31};           // BGR: 0, 50, 79 (/255)
const CvScalar mmColorOrange = {0.15, 0.545, 0.88};     // BGR: 38, 139, 225 (/255)
const CvScalar mmColorYellow = {0.3, 0.78, 0.96};       // BGR: 77, 199, 246 (/255)
char mmGameFlag = false;    // True when game is active
unsigned int mmTimer;       // Game timer
unsigned int mmScore;       // Current score
unsigned int mmHighScore;   // High score

#define MM_SEC_PER_GAME      60     // Time per game, maximum 599 (9 minutes, 59 seconds)
#define MM_SEC_PER_END       10     // Time to wait on game over screen before returning to title

#define MM_DISP_WIDTH        800    // Default gameplay resolution
#define MM_DISP_HEIGHT       480

#ifdef _OSX                         // Desired OS X display resolution
    #define OSX_DISP_WIDTH   1200
    #define OSX_DISP_HEIGHT  720
#endif

// Virtual Boy controller interface
#ifndef _OSX
    #define GPIO_VB_CLK  115    // P9_27, Output (to controller), Clock
    #define GPIO_VB_RST  49     // P9_23, Output (to controller), Reset
    #define GPIO_VB_DAT  7      // P9_42, Input (from controller), Data
    gpio *gpio_vb_clk, *gpio_vb_rst, *gpio_vb_dat;

    // Bit masks
    #define VB_MASK_RDD     0x8000      // Right D-pad, down
    #define VB_MASK_RDL     0x4000      // Right D-pad, left
    #define VB_MASK_SEL     0x2000      // Select
    #define VB_MASK_STR     0x1000      // Start
    #define VB_MASK_LDU     0x0800      // Left D-pad, up
    #define VB_MASK_LDD     0x0400      // Left D-pad, down
    #define VB_MASK_LDL     0x0200      // Left D-pad, left
    #define VB_MASK_LDR     0x0100      // Left D-pad, right
    #define VB_MASK_RDR     0x0080      // Right D-pad, right
    #define VB_MASK_RDU     0x0040      // Right D-pad, up
    #define VB_MASK_LBB     0x0020      // Left back button
    #define VB_MASK_RBB     0x0010      // Right back button
    #define VB_MASK_B       0x0008      // B
    #define VB_MASK_A       0x0004      // A
    #define VB_MASK_UNUSED  0x0002      // Unused (always 0)
    #define VB_MASK_BAT     0x0001      // Low battery
    #define VB_MASK_ALL    ~0xFFFC
#endif


/** Command-line arguments */
int numCamera = -1;                                 // First available camera
const char* stacheMaskFile = "stache-mask.png";     // Mustache image (inverted)
int scaleHeight = 6;                                // OpenCV video capture settings
int offsetHeight = 4;
int camWidth = MM_DISP_WIDTH;
int camHeight = MM_DISP_HEIGHT;
float camFPS = 10;


class timer {
private:
    time_t begTime;
public:
    void start() {
        begTime = time(NULL);
        time(&begTime);
    }
    
    double elapsedSec() {   // Elapsed time in seconds since start
        return difftime(time(NULL), begTime);
    }
    
    bool isTimeout(double seconds) {   // If value >= elapsed time in seconds
        return seconds >= elapsedSec();
    }
};
timer t;


/**
 * @function main
 */
int main(int argc, const char** argv)
{
    Mat frame;
    FILE *pFile;
    
    if(argc > 1) numCamera = atoi(argv[1]);
    if(argc > 2) stacheMaskFile = argv[2];
    if(argc > 3) scaleHeight = atoi(argv[3]);
    if(argc > 4) offsetHeight = atoi(argv[4]);
    if(argc > 5) camWidth = atoi(argv[5]);
    if(argc > 6) camHeight = atoi(argv[6]);
    if(argc > 7) camFPS = (float)atof(argv[7]);
    
    fprintf(stderr, "%s", copyright);   // Print the copyright
    mm_ConfigureFont();                 // Configure font (using cairo + freetype)
    mm_InitializeAudio();               // Initialize SDL audio
#ifndef _OSX
    if (mm_InitializeController()) exit(EXIT_FAILURE);  // Initialize Virtual Boy controller
#endif
    
    while(true)
    {
        if (mm_DisplayTitle()) exit(EXIT_FAILURE);      // Display title screen
        if (mm_ConfigureOpenCV()) exit(EXIT_FAILURE);   // Configure OpenCV (load cascade + image mask)
        
        // Read the video stream
        capture = cvCaptureFromCAM(numCamera);
        if(camWidth) cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, camWidth);
        if(camHeight) cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, camHeight);
        if(camFPS) cvSetCaptureProperty(capture, CV_CAP_PROP_FPS, camFPS);
        if(capture)
        {
            // Initialize gameplay settings
            sound = Mix_LoadWAV(mmEffectSoundFile); // Load our WAV file
            if (sound == NULL) {
                fprintf(stderr, "--(!) Unable to load effect WAV file: %s\n\r", Mix_GetError());
            }
            else
                Mix_VolumeChunk(sound, 30); // Set volume between 0 and MIX_MAX_VOLUME
            
            // Load high score, if there is one
            pFile = fopen(mmHighScoreFile, "r");
            if (pFile != NULL)
            {
                char buffer[12];
                fgets(buffer, 5, pFile);
                mmHighScore = atoi(buffer); // Convert high score string to decimal value
                fprintf(stderr, "High score read from file: %04d\n\r", mmHighScore);
                fclose(pFile);
            }
            else
            {
                fprintf(stderr, "--(!) Unable to open high score file (read)\n\r");
                mmHighScore = 0;
            }
            
            mmTimer = MM_SEC_PER_GAME;  // Load our countdown timer
            mmScore = 0;                // Reset score
            mmGameFlag = true;          // Set gameplay flag
            t.start();                  // Start the system timer
        
            while (mmGameFlag) // While we're in game mode
            {
                frame = cvQueryFrame(capture); // Grab frame from camera
                
                try
                {
                    if (!frame.empty()) // If the frame is good
                        detectAndDisplay(frame); // Apply the classifier (face detection) and play game
                    else
                    {
                        fprintf(stderr, " --(!) Did not detect a frame\n\r");
                        exit(EXIT_FAILURE);
                    }
                    
                    waitKey(1);
                }
                catch(cv::Exception e){}
            }
        }
        else
        {
            fprintf(stderr, "--(!) Can't get stream from camera\n\r");
            exit(EXIT_FAILURE);
        }
    }
}


/**
 * @function detectAndDisplay
 */
void detectAndDisplay(Mat frame) {
    std::vector<Rect> faces;
    Mat frame_gray;
    int i;
    static int i_prev;
    char mmScoreString[64];
    char c;
    int channel;   // Channel on which our sound is played
    FILE *pFile;

    cvtColor(frame, frame_gray, CV_BGR2GRAY);
    equalizeHist(frame_gray, frame_gray);
    
    // Detect faces
    face_cascade.detectMultiScale(frame_gray, faces, 1.1, 2, 0, Size(80, 80));
    
    // Scale and apply mustache mask for each face
    for(i = 0; i < faces.size(); i++)
    {
        Mat faceROI = frame_gray(faces[i]);
        IplImage iplFrame = frame;
        IplImage *iplMask = cvCreateImage(cvSize(faces[i].width, faces[i].height/scaleHeight), mask->depth, mask->nChannels );
        cvSetImageROI(&iplFrame, cvRect(faces[i].x, faces[i].y + (faces[i].height/scaleHeight)*offsetHeight, faces[i].width, faces[i].height/scaleHeight));
        cvResize(mask, iplMask, CV_INTER_LINEAR);
        cvSub(&iplFrame, iplMask, &iplFrame);
        cvResetImageROI(&iplFrame);
    }
    waitKey(1);

#ifdef _OSX
    flip(frame, frame, 1); // Flip the frame horizontally (mirror)
#endif
    
#ifndef _OSX
    Size frame_size(MM_DISP_WIDTH, MM_DISP_HEIGHT);
    resize(frame, frame, frame_size, 0, 0, INTER_NEAREST);   // Resize the image to fit the LCDs maximum resolution
#endif
    
    mmTimer = MM_SEC_PER_GAME - t.elapsedSec(); // Adjust countdown timer
    if (mmTimer <= 0) mmGameFlag = false; // Game over when time runs out
    
    // Add text overlay to the frame
    rectangle(frame, cv::Point2d(0, 0), cv::Point2d(MM_DISP_WIDTH, 75), CV_RGB(0, 0, 0), CV_FILLED); // Create rectangle
    putTextCairo(frame, "MOJO        HIGH SCORE        TIME", cv::Point2d(MM_DISP_WIDTH/2, 25), *myfont_face, 20, mmColorOrange);
    sprintf(mmScoreString, "%04d           %04d           %1d:%02d", mmScore, mmHighScore, mmTimer/60, mmTimer%60);
    putTextCairo(frame, mmScoreString, cv::Point2d(MM_DISP_WIDTH/2, 50), *myfont_face, 20, mmColorYellow);
    if (!mmGameFlag)
        putTextCairo(frame, "GAME OVER", cv::Point2d(MM_DISP_WIDTH/2, (MM_DISP_HEIGHT+75)/2), *myfont_face, 20, mmColorOrange);
    
#ifdef _OSX
    Size frame_size(OSX_DISP_WIDTH, OSX_DISP_HEIGHT);
    resize(frame, frame, frame_size, 0, 0, INTER_LINEAR);   // Resize the frame to fit OS X display
#endif
    
    imshow(window_name, frame); // Update the window
    waitKey(1);

    if (!mmGameFlag)
    {
        if (mmScore > mmHighScore)  // Save the score if it's a new high score
        {
            pFile = fopen(mmHighScoreFile, "w");
            if (pFile != NULL)
            {
                char buffer[12];
                sprintf(buffer, "%04d", mmScore); // Convert high score to ASCII string in decimal
                fputs(buffer, pFile);
                fprintf(stderr, "High score written to file: %s\n\r", buffer);
                fclose(pFile);
            }
            else
                fprintf(stderr, "--(!) Unable to open high score file (write)\n\r");
        }
        
        if (sound != NULL) Mix_FreeChunk(sound);    // Release the memory allocated to our sound
        cvReleaseCapture(&capture);                 // Close the video stream
        t.start();                                  // Start the system timer
        
        // Wait for timeout or any keypress to return to the title screen
        while(mm_GetController() == 0 && (MM_SEC_PER_END - t.elapsedSec()) > 0);
    }
    else  // Game is still in play
    {
        c = mm_GetController();     // Read keyboard/controller input
        switch (c)
        {
            case ' ':   // Spacebar (OS X) || A key (OS X) || A button (VB)
            case 'A':
            case 'a':
                if (i > 0 && (i_prev == 0))
                {
                    if (sound != NULL) channel = Mix_PlayChannel(-1, sound, 0); // Play sound effect (one time)
                    if (channel == -1) {
                        fprintf(stderr, "--(!) Unable to play effect WAV file: %s\n\r", Mix_GetError());
                    }
                    
                    mmScore += (int)pow(i, 2);              // Weighted score based on number of mustaches on the screen
                    if (mmScore > 9999) mmScore -= 10000;   // Limit maximum score
                    i_prev = i;         // Scoring isn't allowed again until all mustaches disappear from the frame
                    saveFrame(frame);   // Save the image
                }
                else
                  i_prev = 0;
                break;
            case '\n':  // Return (OS X) || B button (VB)
                saveFrame(frame);   // Save the image
                break;
            case 0x1b:  // ESC (OS X) || L+R (VB)
                mmGameFlag = false;                         // End game early
                if (sound != NULL) Mix_FreeChunk(sound);    // Release the memory allocated to our sound
                cvReleaseCapture(&capture);                 // Close the video stream
                break;
        }
    }
}


/**
 * @function saveFrame
 */
void saveFrame(Mat frame)
{
    char filename[64];
    IplImage iplFrame = frame;
    bool filenameOK = 0;
    
    while (!filenameOK) // repeat until we find an unused filename
    {
#ifdef _OSX
        sprintf(filename, "tmp/captured%03d.jpg", savedFrames);
#else
        sprintf(filename, "/home/root/stache/tmp/captured%03d.jpg", savedFrames);
#endif
        
        if (access(filename, F_OK) != -1)  // file exists
        {
            savedFrames++; // increment file count
            if (savedFrames >= 1000) savedFrames = 0; // limit number of files to save
        }
        else  // file doesn't exist, so the filename is safe to use
        {
            cvSaveImage(filename, &iplFrame);
            fprintf(stderr, "Frame saved: %s\n\r", filename);
            filenameOK = true;
        }
    }
}


/**
 * @function mm_DisplayTitle
 */
int mm_DisplayTitle(void) // Display title screen
{
    Mat frame;
    int channel;   // Channel on which our sound is played
    char c;
    
    frame = imread(mmTitleFile, CV_LOAD_IMAGE_COLOR);   // Read the file
    if(frame.empty())  // Check for invalid input
    {
        fprintf(stderr, "--(!) Error loading title screen\n\r");
        return 1;
    }
    
    putTextCairo(frame, "BY JOE GRAND", cv::Point2d(MM_DISP_WIDTH/2, MM_DISP_HEIGHT-100), *myfont_face, 20, mmColorBrown);
#ifdef _OSX
    putTextCairo(frame, "PRESS SPACEBAR TO PLAY", cv::Point2d(MM_DISP_WIDTH/2, MM_DISP_HEIGHT-60), *myfont_face, 20, mmColorBrown);
#else
    putTextCairo(frame, "PRESS START TO PLAY", cv::Point2d(MM_DISP_WIDTH/2, MM_DISP_HEIGHT-60), *myfont_face, 20, mmColorBrown);
#endif
    
#ifdef _OSX
    Size frame_size(OSX_DISP_WIDTH, OSX_DISP_HEIGHT);
    resize(frame, frame, frame_size, 0, 0, INTER_LINEAR);   // Resize the frame to fit OS X display
#endif
    
    imshow(window_name, frame);   // Show our image inside of it
    
    // Load our WAV file
    sound = Mix_LoadWAV(mmTitleSoundFile);
    if(sound == NULL) {
        fprintf(stderr, "--(!) Unable to load title WAV file: %s\n\r", Mix_GetError());
    }
    else
        Mix_VolumeChunk(sound, 30); // Set volume between 0 and MIX_MAX_VOLUME
    
    // Play our sound file (infinite loops), and capture the channel on which it is played
    if (sound != NULL) channel = Mix_PlayChannel(-1, sound, -1);
    if (channel == -1) {
        fprintf(stderr, "--(!) Unable to play title WAV file: %s\n\r", Mix_GetError());
    }

#ifdef _OSX
    waitKey(1);
#else
    waitKey(250); // Extended delay to allow image to properly display
#endif
    
    do
    {
        c = mm_GetController(); // Read keyboard/controller input
        switch (c)
        {
            case 0x1b:  // ESC (OS X) || L+R (VB)
                if (channel != -1) Mix_HaltChannel(channel); // Stop currently playing audio
                if (sound != NULL) channel = Mix_PlayChannel(-1, sound, -1); // Replay audio from the beginning
                if (channel == -1) {
                    fprintf(stderr, "--(!) Unable to play WAV file: %s\n\r", Mix_GetError());
                }
                break;
        }
        
        waitKey(1);

    } while (c != ' '); // Wait for spacebar (OS X) or Start button (VB)

    if (channel != 1) Mix_FadeOutChannel(channel, 1000);  // Fade out & stop audio in preparation of gameplay
    if (channel != 1) while(Mix_Playing(channel) != 0);   // Wait until the sound has stopped playing
    if (sound != NULL) Mix_FreeChunk(sound);              // Release the memory allocated to our sound
    
    return 0;
}


/**
 * @function mm_GetController
 */
char mm_GetController(void)  // User interface
{
    char c;
    
#ifdef _OSX   // Read keyboard/controller input
    if (kbhit())  // Wait for keypress
        c = getch(); // Get key
#else         // Read Virtual Boy controller
    // Details from http://www.goliathindustries.com/vb/download/vbprog.pdf

    int i;
    uint16_t button_data = 0;

    libsoc_gpio_set_level(gpio_vb_clk, HIGH);
        
    // Strobe reset to wake up controller
    libsoc_gpio_set_level(gpio_vb_rst, LOW);
    usleep(100);
    libsoc_gpio_set_level(gpio_vb_rst, HIGH);
    usleep(100);
    libsoc_gpio_set_level(gpio_vb_rst, LOW);
    
    for (i = 0; i < 16; ++i)
    {
        usleep(100);
        button_data <<= 1; // Shift to make room for the next bit
        button_data |= libsoc_gpio_get_level(gpio_vb_dat);
        
        usleep(100);
        libsoc_gpio_set_level(gpio_vb_clk, LOW);
        usleep(100);
        libsoc_gpio_set_level(gpio_vb_clk, HIGH);
    }

    //fprintf(stderr, "Button Data: %s [0x%2X]\n\r", int_to_binary(button_data), button_data);
    
    // Check button and set return value to match keyboard character
    button_data |= VB_MASK_ALL;
    button_data ^= 0xFFFF;      // Invert the bits (active low)
    switch (button_data)
    {
        case VB_MASK_B:
            c = '\n';
            break;
        case VB_MASK_STR:
            if (!mmGameFlag) c = ' ';
            else c = -1;
            break;
        case VB_MASK_A:
            if (mmGameFlag) c = ' ';
            else c = -1;
            break;
        case VB_MASK_LBB | VB_MASK_RBB:
            c = 0x1b; // ESC
            break;
        case VB_MASK_SEL:
        case VB_MASK_LBB:
        case VB_MASK_RBB:
            c = -1; // Button was pressed, but we're not handling it
            break;
        default:
            c = 0; // No button press
    }
#endif
    
    return c;
}

#ifndef _OSX
/**
 * @function mm_InitializeController
 */
int mm_InitializeController(void) // Initialize Virtual Boy controller
{
    //libsoc_set_debug(1); // Enable debug output
    
    // Request pins from system
    gpio_vb_clk = libsoc_gpio_request(GPIO_VB_CLK, LS_SHARED);
    gpio_vb_rst = libsoc_gpio_request(GPIO_VB_RST, LS_SHARED);
    gpio_vb_dat = libsoc_gpio_request(GPIO_VB_DAT, LS_SHARED);

    if (gpio_vb_clk == NULL || gpio_vb_rst == NULL || gpio_vb_dat == NULL)
    {
        fprintf(stderr, "--(!) Unable to initialize GPIO pins\n\r");
        return 1;
    }
    
    // Set pin directions
    libsoc_gpio_set_direction(gpio_vb_clk, OUTPUT);
    libsoc_gpio_set_direction(gpio_vb_rst, OUTPUT);
    libsoc_gpio_set_direction(gpio_vb_dat, INPUT);
    
    if (libsoc_gpio_get_direction(gpio_vb_clk) != OUTPUT ||
        libsoc_gpio_get_direction(gpio_vb_rst) != OUTPUT ||
        libsoc_gpio_get_direction(gpio_vb_dat) != INPUT)
    {
        fprintf(stderr, "--(!) Unable to set GPIO pin directions\n\r");
        return 1;
    }
    
    // Set default states
    libsoc_gpio_set_level(gpio_vb_clk, HIGH);
    libsoc_gpio_set_level(gpio_vb_rst, LOW);
    
    return 0;
}
#endif


/**
 * @function mm_InitializeAudio
 */
int mm_InitializeAudio(void) // Initialize SDL audio
{
    // Code from http://content.gpwiki.org/SDL_mixer:Tutorials:Playing_a_WAV_Sound_File
    
    const int    audio_rate = 44100;            // Frequency of audio playback
    const Uint16 audio_format = AUDIO_S16SYS; 	// Format of the audio we're playing
    const int    audio_channels = 2;            // 2 channels = stereo
    const int    audio_buffers = 4096;          // Size of the audio buffers in memory
    
    if (SDL_Init(SDL_INIT_AUDIO) != 0)
    {
        fprintf(stderr, "--(!) Unable to initialize SDL: %s\n\r", SDL_GetError());
        return 1;
    }
    
    if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) != 0)
    {
        fprintf(stderr, "--(!) Unable to initialize mixer: %s\n\r", Mix_GetError());
        return 1;
    }
    
    return 0;
}


/**
 * @function mm_ConfigureOpenCV
 */
int mm_ConfigureOpenCV(void) // Configure OpenCV (load cascade + image mask)
{
    if( !face_cascade.load(face_cascade_name) )
    {
        fprintf(stderr, "--(!) Error loading cascade\n\r");
        return 1;
    }
    
    mask = cvLoadImage(stacheMaskFile);
    if(mask == NULL)
    {
        fprintf(stderr, "--(!) Could not load mask %s\n\r", stacheMaskFile);
        return 1;
    }
    
    return 0;
}


/**
 * @function mm_ConfigureFont
 */
void mm_ConfigureFont(void) // Configure font (using cairo + freetype)
{
    // Code from http://cboard.cprogramming.com/c-programming/146124-problem-loading-font-face-using-freetype-cairo.html
    
    FT_Library library;
    FT_Face face;
    FT_Init_FreeType( &library );
    if (FT_New_Face( library, mmFontFile, 0, &face ) != 0)
    {
        fprintf(stderr, "--(!) Error loading font\n\r");
        exit(EXIT_FAILURE);
    }
    myfont_face = cairo_ft_font_face_create_for_ft_face(face, 0);
}


/**
 * @function putTextCairo
 */
void putTextCairo(
                  cv::Mat &targetImage,
                  std::string const &text,
                  cv::Point2d centerPoint,
                  cairo_font_face_t &fontFace,
                  double fontSize,
                  cv::Scalar textColor)
{
    // Code from https://stackoverflow.com/questions/11917124/opencv-how-to-use-other-font-than-hershey-with-cvputtext-like-arial
    
    // Create Cairo
    cairo_surface_t* surface =
    cairo_image_surface_create(
                               CAIRO_FORMAT_ARGB32,
                               targetImage.cols,
                               targetImage.rows);
    
    cairo_t* cairo = cairo_create(surface);
    
    // Wrap Cairo with a Mat
    cv::Mat cairoTarget(
                        cairo_image_surface_get_height(surface),
                        cairo_image_surface_get_width(surface),
                        CV_8UC4,
                        cairo_image_surface_get_data(surface),
                        cairo_image_surface_get_stride(surface));
    
    // Put image onto Cairo
    cv::cvtColor(targetImage, cairoTarget, cv::COLOR_BGR2BGRA);
    
    // Set font and write text   
    cairo_set_font_face(cairo, &fontFace);
    cairo_set_font_size(cairo, fontSize);
    cairo_set_source_rgb(cairo, textColor[2], textColor[1], textColor[0]);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cairo, text.c_str(), &extents);
    
    cairo_move_to(
                  cairo,
                  centerPoint.x - extents.width/2 - extents.x_bearing,
                  centerPoint.y - extents.height/2- extents.y_bearing);
    cairo_show_text(cairo, text.c_str());
    
    // Copy the data to the output image
    cv::cvtColor(cairoTarget, targetImage, cv::COLOR_BGRA2BGR);
    
    cairo_destroy(cairo);
    cairo_surface_destroy(surface);
}


/**
 * @function int_to_binary
 */
const char *int_to_binary(int x)
{
    static char b[17];
    b[0] = '\0';
    
    int z;
    for (z = 0x8000; z > 0; z >>= 1)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }
    
    return b;
}

