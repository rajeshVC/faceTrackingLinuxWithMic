
#include "FaceDetection.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <random>
#include <thread>

#define NTH_FRAME 1
#define N_THREADS 2
//!check the FOV
#define CAMERA_FOV 160

#define RESOLUTION_X 1280
#define RESOLUTION_Y 720

#define ROI_STARTX 35
#define ROI_ENDX 550
#define ROI_STARTY 24
#define ROI_ENDY 324

#define SECONDARY_CAMERA 2
#define PRIMARY_CAMERA 4

#define MAPPING

using namespace std;
using namespace cv;

//!for micDoA integration
struct FOV{
    int min = (180 - CAMERA_FOV) / 2;
    int max = 180 - min;
} FOV;

struct BoundingBox {
    float x1, x2, y1, y2;
} Bbox;

#if(0)
// void drawBoundingBoxOnPrimaryDevice(const cv::Mat& image, const cv::Rect& bboxPrimary, const std::vector<FaceInfo>& finalBox) {
    

//     // cv::Rect bboxPrimary = cv::Rect(box.x1*(1280/roiWidth), box.y1, (box.x2-box.x1), (box.y2-box.y1));
//     cv::rectangle(image, bboxPrimary, cv::Scalar(255, 255, 255), 2);
//     putText(image, std::to_string(finalBox.size()),Point(10,15),FONT_HERSHEY_SIMPLEX,0.6, Scalar(255, 0, 0), 2);


// }

int getPan(const cv::Mat& image, BoundingBox box){
    int pan = -1;
    float bboxCentreX = (box.x2-box.x1)/2;
    int eachPixelWorthPan = panMax/(image.cols/2);

    if(bboxCentreX < image.cols/2) {
        pan = - (eachPixelWorthPan * bboxCentreX);
        
    } else if(bboxCentreX > image.cols/2){
        pan = eachPixelWorthPan * bboxCentreX;
        
    } else {
        pan = 0;
    }
    
    return pan;
}

int getTilt(const cv::Mat& image, BoundingBox box){
    int tilt = -1;
    float bboxCentreY = (box.y2-box.y1)/2;
    int eachPixelWorthTilt = tiltMax/(image.rows/2);

    if(bboxCentreY < image.cols/2) {
        tilt = - (eachPixelWorthTilt * bboxCentreY);
        
    } else if(bboxCentreY > image.cols/2) {
        tilt = eachPixelWorthTilt * bboxCentreY;
        
    } else {
        tilt = 0;
    }

    return tilt;
}

int getZoom(const cv::Mat& image, BoundingBox box){
    int zoomFactor = -1;
    int frameSize  = RESOLUTION_X * RESOLUTION_Y;
    float bboxSize = (box.x2-box.x1) * (box.y2-box.y1);

    zoomFactor = frameSize/bboxSize;

    return zoomFactor * zoomStep;

}

void setPTZ(BoundingBox box, cv::Mat image) {

    //!36000 supposed to be replaced by PanMax/TiltMax

    int pan   = getPan();
    int tilt  = getTilt();
    int zoom  = getZoom();

    
    
}


//!for micDoA integration
int micDoASimulator() {
    std::random_device rd;
    std::mt19937 mt(rd());

    std::uniform_int_distribution<std::mt19937::result_type> lk(0, 180);
    return lk(mt);
}

// //! to identify the speaker and locate position/bbox
// //!for micDoA integration
FaceInfo identifyTheSpeaker(const cv::Mat& image, const std::vector<FaceInfo>& faceList) {
    
    int angle = micDoASimulator();
    
    //!number of pixels need to be moved per degree of the device FOV
    float nthPixelPerDegree = image.cols / CAMERA_FOV;

    //!mapping the actual DoA to the device FOV
    if(angle < FOV.min) {
        angle = FOV.min;
    } else if(angle > FOV.max) {
        angle = FOV.max;
    }

    angle -= FOV.min;

    //! locating the pixel corresponding to the mapped angle
    float currentPixel = nthPixelPerDegree * angle;
    
    //!locating the speaker corresponding to the current pixel location
    FaceInfo currentSpeaker = {};
    for(auto i=0; i<faceList.size(); i++) {
        if(currentPixel < (faceList[i].x2 + (faceList[i+1].x1 - faceList[i].x2)/2)) {
            currentSpeaker = faceList[i];
            break;
        }
    }

    return currentSpeaker;
}
#endif

void resetToAspectRatio(float& width, float& height, int roiWidth, int roiHeight) {
    //!roiWidth and roiHeight parameters are not being used in the definition
    if(width <= 0) {
        width = -1;
    }
    if(height <= 0) {
        height = -1;
    }
   
    float multiple_9  = height/9.0;
    float multiple_16 = width/16.0;

    //!mapping the aspect ratio depending on the maximum multiple
    float multiple = std::max(multiple_16, multiple_9);
    
    width  = (multiple + 4) * 16;
    height = (multiple + 8) * 9;

    // std::cout<<"new aspect ratio: "<<width<<" x "<<height<<std::endl; 
    
}

cv::Rect drawBiggerBoundingBox(const std::vector<FaceInfo>& in, const cv::Mat& resized, int roiWidth=0, int roiHeight=0, int type=0){
    
    if(in.empty()){
        return cv::Rect();
    }

    float x1_min = in[0].x1, y1_min = in[0].y1, x2_max = in[0].x2, y2_max = in[0].y2;
  
    for(auto i=0; i<in.size(); i++) {
        x1_min = std::min(x1_min, in[i].x1);
        y1_min = std::min(y1_min, in[i].y1);
        x2_max = std::max(x2_max, in[i].x2);
        y2_max = std::max(y2_max, in[i].y2);
    }

    //!actual frame bounding box points as per the secondary device
    std::cout<<"x1_min: "<<x1_min<< " --- x2_max: "<<x2_max<<std::endl;
    std::cout<<"y1_min: "<<y1_min<< " --- y2_max: "<<y2_max<<std::endl;
    float occupied_width  = x2_max - x1_min;
    float occupied_height = y2_max - y1_min;
    
    float bbox_w = occupied_width;
    float bbox_h = occupied_height;
    std::cout<<"occupied width : "<<occupied_width<<std::endl;
    std::cout<<"occupied height: "<<occupied_height<<std::endl;
    
    //!resizing the frame bounding box to 16:9 aspect ratio
    resetToAspectRatio(bbox_w, bbox_h, roiWidth, roiHeight);

    //!coordinates of the subset view
    // std::cout<<"Diif of bigger bb and extreme face bb along X: "<<bbox_w-occupied_width<<std::endl;
    // std::cout<<"Diif of bigger bb and extreme face bb along Y: "<<bbox_h-occupied_height<<std::endl;

    float space_x = (bbox_w - occupied_width)/2;
    float space_y = (bbox_h - occupied_height)/2;
    float new_x1 = x1_min == 0 || space_x > x1_min ? 0 : x1_min - space_x;
    float new_y1 = y1_min == 0 || space_y > y1_min ? 0 : y1_min - space_y;
    new_y1      += (y1_min - new_y1) / 2;

    std::cout<<"FrameBB-II x1    : "<<new_x1<<std::endl;
    std::cout<<"FrameBB-II y1    : "<<new_y1<<std::endl;
    std::cout<<"FrameBB-II width : "<<bbox_w<<std::endl;
    std::cout<<"FrameBB-II height: "<<bbox_h<<std::endl;
    //!frame bounding box as per the secondary view
    cv::Rect bbox = cv::Rect(new_x1, new_y1, bbox_w, bbox_h);

    cv::rectangle(resized, bbox, cv::Scalar(0, 0, 0), 2);

    #ifdef MAPPING
    //!mapping the subset to the actual size of the primary device
    float RatioWidth  = (float) RESOLUTION_X/roiWidth;
    float RatioHeight = (float) RESOLUTION_Y/roiHeight;
    std::cout<<"RatioWidth : "<<RatioWidth<<std::endl;
    std::cout<<"RatioHeight: "<<RatioHeight<<std::endl;
    new_x1 *= RatioWidth;
    new_y1 *= RatioHeight;
    bbox_w *= RatioWidth;
    bbox_h *= RatioHeight;
    cv::Rect BBprimary = cv::Rect(new_x1, new_y1, bbox_w, bbox_h);

    std::cout<<"FrameBB-I x1    : "<<new_x1<<std::endl;
    std::cout<<"FrameBB-I y1    : "<<new_y1<<std::endl;
    std::cout<<"FrameBB-I width : "<<bbox_w<<std::endl;
    std::cout<<"FrameBB-I height: "<<bbox_h<<std::endl;
    #endif

    if(in.size() == 1 && type) {
        cv::putText(resized, "Speaker", cv::Point(new_x1, new_y1), cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0,255,255), 1);
    }
    #ifdef MAPPING
    return BBprimary;
    #else
    return bbox;
    #endif
    

}

int main(int argc, char **argv)
{
     if(argc <= 1) {
        fprintf(stderr, "Usage: %s <nth Frame> <no of threads>\n", argv[0]);
        return 1;
     }
    Mat frame;

    FaceDetection *ultra = new FaceDetection(std::stoi(argv[2]));
// const char *pipeline = " tcambin serial=40120113 ! video/x-raw, format=BGRx, width=1280,height=720, framerate=60/1 ! videoconvert ! appsink";
    VideoCapture capSecondary(SECONDARY_CAMERA);
    if (!capSecondary.isOpened()) {
        cerr << "ERROR: Unable to open the video" << endl;
        return 0;
    }

    capSecondary.set(CAP_PROP_FRAME_HEIGHT, 360);
    capSecondary.set(CAP_PROP_FRAME_WIDTH, 640);
    
    std::cout << "Start grabbing, press ESC on Live window to terminate" << endl;

    int i = 0;

    #ifdef MAPPING
    VideoCapture capPrimary(PRIMARY_CAMERA);
    if(!capPrimary.isOpened()) {
        std::cout<<"Failed to open the camera"<<std::endl;
        exit(1);
    }

    capPrimary.set(CAP_PROP_FRAME_HEIGHT, 720);
    capPrimary.set(CAP_PROP_FRAME_WIDTH, 1280);
    
    cv::Mat framePrimary;
    
    #endif


    
    while(1) {
        capSecondary >> frame;
        if (frame.empty()) {
            cerr << "ERROR: Unable to grab from the camera" << endl;
            break;
        }
        #ifdef MAPPING
        capPrimary >> framePrimary;
        if (framePrimary.empty()) {
            cerr << "ERROR: Unable to grab from the camera" << endl;
            break;
        }
        

        //!Subset view of primary device
        int roiWidth  = ROI_ENDX - ROI_STARTX;
        int roiHeight = ROI_ENDY - ROI_STARTY;

        //read the frame from remote side and check the reoslution it is opened in - 1080p (16:9)
        //!Creating ROI out of secondary device view to be subset of the primary device view
        cv::Rect myROI(ROI_STARTX, ROI_STARTY, roiWidth, roiHeight);
        cv::Mat resized = frame(myROI);

        #endif
        // //!detecting the people in the camera view
        std::vector<FaceInfo> finalBox;
        if(i % std::stoi(argv[1]) == 0) {
            
            #ifdef MAPPING
            ultra->detect(resized,finalBox,resized.rows,resized.cols);
            std::cout<<"Secondary cam width: "<<resized.cols<<std::endl;
            std::cout<<"Secondary cam height: "<<resized.rows<<std::endl;
            #else
            ultra->detect(frame,finalBox,frame.rows,frame.cols);
            std::cout<<"Secondary cam width: "<<frame.cols<<std::endl;
            std::cout<<"Secondary cam height: "<<frame.rows<<std::endl;
            #endif

            std::cout<<"Face list: "<<finalBox.size()<<std::endl;
   
            // std::cout<<"FaceList with the respective WxH: "<<std::endl;
            // std::cout<<"-------------------------------------------"<<std::endl;
            // for(const auto& ch : finalBox){
            //     std::cout<<"x1: "<<ch.x1<<" -- x2: "<<ch.x2<<std::endl;
            //     std::cout<<"y1: "<<ch.y1<<" -- y2: "<<ch.y2<<std::endl;
            // }

            // std::cout<<"----------------------------------------"<<std::endl;
            // std::cout<<"----------------------------------------"<<std::endl;
            //!map this small image to actual image
            for(size_t i = 0; i < finalBox.size(); i++) {
                FaceInfo facebox = finalBox[i];
                float width      = facebox.x2-facebox.x1;
                float height     = facebox.y2-facebox.y1;
      
                
                std::cout<<"FaceBB-II x1: "<<facebox.x1<<std::endl;
                std::cout<<"FaceBB-II y1: "<<facebox.y1<<std::endl;
                std::cout<<"FaceBB-II w : "<<width<<std::endl;
                std::cout<<"FaceBB-II h : "<<height<<std::endl;
                #ifdef MAPPING
                std::cout<<"FaceBB-I x1 : "<<facebox.x1 * RESOLUTION_X/roiWidth<<std::endl;
                std::cout<<"FaceBB-I y1 : "<<facebox.y1 * RESOLUTION_Y/roiHeight<<std::endl;
                std::cout<<"FaceBB-I w  : "<<width * RESOLUTION_X/roiWidth<<std::endl;
                std::cout<<"FaceBB-I h  : "<<height * RESOLUTION_Y/roiHeight<<std::endl;
                #endif

                //!face bounding box of subset image

                cv::Rect faceBB = cv::Rect(facebox.x1, facebox.y1, (facebox.x2-facebox.x1), (facebox.y2-facebox.y1));

                // if(argv[3]) {
                //     //!micDoA integration
                //     std::vector<FaceInfo> locatedSpeaker;
                //     locatedSpeaker.push_back(identifyTheSpeaker(resized, finalBox));

                //     drawBiggerBoundingBox(locatedSpeaker, resized, 1);

                // } else {
                
                #ifdef MAPPING
        //         //!Secondary device frame bounding box being drawn in the method whereas
                //!the return value is the mapped value for primary device

                cv::Rect frameBBprimary = drawBiggerBoundingBox(finalBox, resized, roiWidth, roiHeight);
        //         //! Secondary device
                cv::rectangle(resized, faceBB, cv::Scalar(50, 50, 255), 2);
                cv::rectangle(resized, cv::Point(0,0), cv::Point(30, 25), cv::Scalar(50, 50, 255), cv::FILLED);
                putText(resized, std::to_string(finalBox.size()), Point(10,15), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 0, 0), 2);

        //         //!Primary device
                // std::cout<<"Checking precedence..."<<std::endl;
                // std::cout<<"facebox.x1: "<<facebox.x1<<std::endl;
                // std::cout<<"facebox.x2: "<<facebox.x2<<std::endl;
                // std::cout<<"facebox.x1 * RESOLUTION_X/roiWidth: "<<facebox.x1 * RESOLUTION_X/roiWidth<<std::endl;
                // std::cout<<"facebox.x1: "<<facebox.x1<<std::endl;
                // std::cout<<"facebox.x2: "<<facebox.x2<<std::endl;
                // std::cout<<"facebox.x2-facebox.x1: "<<facebox.x2-facebox.x1<<std::endl;

                std::cout<<"facebox.x1 * (RESOLUTION_X/roiWidth): "<<facebox.x1 * (RESOLUTION_X/roiWidth)<<std::endl;
                std::cout<<"(facebox.x2-facebox.x1) * RESOLUTION_X/roiWidth: "<<(facebox.x2-facebox.x1) * RESOLUTION_X/roiWidth<<std::endl;
                std::cout<<"(facebox.x2-facebox.x1) * (RESOLUTION_X/roiWidth): "<<(facebox.x2-facebox.x1) * (RESOLUTION_X/roiWidth)<<std::endl;
                cv::Rect faceBBprimary = cv::Rect(facebox.x1 * RESOLUTION_X/roiWidth, facebox.y1*RESOLUTION_Y/roiHeight, (facebox.x2-facebox.x1) * RESOLUTION_X/roiWidth, (facebox.y2-facebox.y1) * RESOLUTION_Y/roiHeight);
                cv::rectangle(framePrimary, faceBBprimary, cv::Scalar(255, 255, 255), 2);
                cv::rectangle(framePrimary, frameBBprimary, cv::Scalar(0, 255, 255), 2);
                putText(framePrimary, std::to_string(finalBox.size()),Point(10,15),FONT_HERSHEY_SIMPLEX,0.6, Scalar(255, 0, 0), 2);
                #else
                cv::Rect frameBBprimary = drawBiggerBoundingBox(finalBox, frame);
        //         //! Secondary device
                cv::rectangle(frame, faceBB, cv::Scalar(50, 50, 255), 2);
                cv::rectangle(frame, cv::Point(0,0), cv::Point(30, 25), cv::Scalar(50, 50, 255), cv::FILLED);
                putText(frame, std::to_string(finalBox.size()), Point(10,15), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 0, 0), 2);

                #endif
  

            }
        }

        #ifdef MAPPING
        //show output
        String winName1 = "[SECONDARY CAMERA]";
        namedWindow(winName1, cv::WINDOW_NORMAL);
        imshow(winName1, resized);

        String winName = "[PRIMARY CAMERA]";
        namedWindow(winName, cv::WINDOW_NORMAL);
        imshow(winName, framePrimary);
        i++;
        #else
        String winName1 = "[SECONDARY CAMERA]";
        // namedWindow(winName1);
        imshow(winName1, frame);
        #endif

        char esc = waitKey(5);
        if(esc == 27) break;
    }

    std::cout << "Closing the camera" << endl;
    capSecondary.release();

    #ifdef MAPPING
    capPrimary.release();
    #endif
    cv::destroyAllWindows();
    delete ultra;

    return 0;
}
