
#include "FaceDetection.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <random>
#include <thread>

#define NTH_FRAME 1
#define N_THREADS 2
//!check the FOV
#define CAMERA_FOV 160

#define STARTX 174
#define ENDX 623
#define STARTY 110
#define ENDY 362

#define SECONDARY_CAMERA 4
#define PRIMARY_CAMERA 2

using namespace std;
using namespace cv;


// bool isMicAccessEnabled = true;
//!for micDoA integration
struct FOV{
    int min = (180 - CAMERA_FOV) / 2;
    int max = 180 - min;
} FOV;

struct BoundingBox {
    float x1, x2, y1, y2;
} Bbox;

void drawBoundingBoxOnPrimaryDevice(const cv::Mat& image, const BoundingBox& box, const std::vector<FaceInfo>& finalBox) {
    
    VideoCapture capPrimary(PRIMARY_CAMERA);
    if(!capPrimary.isOpened()) {
        std::cout<<"Failed to open the camera"<<std::endl;
        exit(1);
    }
    cv::Mat framePrimary;
    while(1) {
        capPrimary.read(framePrimary);
        if (framePrimary.empty()) {
            cerr << "ERROR: Unable to grab from the camera" << endl;
            break;
        }

        cv::Rect bboxPrimary = cv::Rect(box.x1, box.y1, (box.x2-box.x1), (box.y2-box.y1));
        cv::rectangle(image, bboxPrimary, cv::Scalar(255, 255, 255), 2);
        putText(image, std::to_string(finalBox.size()),Point(10,15),FONT_HERSHEY_SIMPLEX,0.6, Scalar(255, 0, 0), 2);

        String winName = "[PRIMARY CAMERA]";
        namedWindow(winName, cv::WINDOW_NORMAL);
        imshow(winName, image);


        char esc = waitKey(5);
        if(esc == 27) break;
    }

    capPrimary.release();
    cv::destroyAllWindows();

}

// int getPan(const cv::Mat& image, BoundingBox box){
//     int pan = -1;
//     if(bboxCentreX < image.cols/2) {
//         pan = - (eachPixelWorthPan * bboxCentreX);
        
//     } else {
//         pan = eachPixelWorthPan * bboxCentreX;
        
//     }
    
//     return pan;
// }

// int getTilt(const cv::Mat& image, BoundingBox box){
//     int tilt = -1;
//     if(bboxCentreX < image.cols/2) {
//         tilt = - (eachPixelWorthTilt * bboxCentreY);
        
//     } else {
//         tilt = eachPixelWorthTilt * bboxCentreY;
        
//     }
//     return tilt;
// }

// int getZoom(const cv::Mat& image, BoundingBox box1){
//     int zoom = -1;

//     int deltaBBox = 
// }

// void setPTZ(BoundingBox box, cv::Mat image) {
//     float bboxCentreX = (box.x2 - box.x1)/2;
//     float bboxCentreY = (box.y2 - box.y1)/2;

//     //!36000 supposed to be replaced by PanMax/TiltMax
//     int eachPixelWorthPan = 36000/(image.cols/2);
//     int eachPixelWorthTilt = 36000/(image.rows/2);

//     int pan = -1, tilt = -1, zoom = -1;
//     if(bboxCentreX < image.cols/2) {
//         pan = - (eachPixelWorthPan * bboxCentreX);

//     } else {
//         pan = eachPixelWorthPan * bboxCentreX;
        
//     }


// }

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
    
    float nthPixelPerDegree = image.rows / CAMERA_FOV;

    if(angle < FOV.min) {
        angle = FOV.min;
    } else if(angle > FOV.max) {
        angle = FOV.max;
    }

    angle -= FOV.min;

    float currentPixel = nthPixelPerDegree * angle;
    

    FaceInfo currentSpeaker = {};
    for(auto i=0; i<faceList.size(); i++) {
        if(currentPixel < (faceList[i].x2 + (faceList[i+1].x1 - faceList[i].x2)/2)) {
            currentSpeaker = faceList[i];
            break;
        }
    }

    return currentSpeaker;
}

void resetToAspectRatio(float& width, float& height) {

    if(width <= 0) {
        width = -1;
    }
    if(height <= 0) {
        height = -1;
    }
   
    float multiple_9 = height/9.0;
    float multiple_16 = width/16.0;

    float multiple = std::max(multiple_16, multiple_9);
    
    width = (multiple + 2) * 16;
    height = (multiple + 8) * 9;

    // std::cout<<"new aspect ratio: "<<width<<" x "<<height<<std::endl; 
    
}

cv::Rect drawBiggerBoundingBox(const std::vector<FaceInfo>& in, const cv::Mat& resized, int type=0){
    
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

    std::cout<<"x1_min: "<<x1_min<< " --- x2_max: "<<x2_max<<std::endl;
    std::cout<<"y1_min: "<<y1_min<< " --- y2_max: "<<y2_max<<std::endl;
    float occupied_width  = x2_max - x1_min;
    float occupied_height = y2_max - y1_min;
    
    float bbox_w = occupied_width;
    float bbox_h = occupied_height;
    
    resetToAspectRatio(bbox_w, bbox_h);

    float new_x1 = x1_min == 0 ? 0 : x1_min - ((bbox_w - occupied_width)/2);
    float new_y1 = y1_min == 0 ? 0 : y1_min - ((bbox_h - occupied_height)/2);
    new_y1 += (y1_min - new_y1) / 2;
 
   
    cv::Rect bbox = cv::Rect(new_x1, new_y1, bbox_w, bbox_h);
    // cv::rectangle(resized, bbox, cv::Scalar(0, 0, 0), 2);
    

    if(in.size() == 1 && type) {
        cv::putText(resized, "Speaker", cv::Point(new_x1, new_y1), cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0,255,255), 1);
    }

    return bbox;
    
}

int main(int argc, char **argv)
{
     if(argc <= 1) {
        fprintf(stderr, "Usage: %s <nth Frame> <no of threads>\n", argv[0]);
        return 1;
     }
    Mat frame;
    chrono::steady_clock::time_point Tbegin, Tend;

    FaceDetection *ultra = new FaceDetection(std::stoi(argv[2]));

    std::cout<<"here about to open secondary cam"<<std::endl;

    VideoCapture cap(SECONDARY_CAMERA);
    // VideoCapture cap("../resources/videos/Walks2.mp4");
    if (!cap.isOpened()) {
        cerr << "ERROR: Unable to open the video" << endl;
        return 0;
    }
    std::cout<<"here about to open primary cam"<<std::endl;


    VideoCapture capPrimary(PRIMARY_CAMERA);
    if(!capPrimary.isOpened()) {
        std::cout<<"Failed to open the camera"<<std::endl;
        exit(1);
    }

    std::cout << "Start grabbing, press ESC on Live window to terminate" << endl;

    int i = 0;
    cv::Mat framePrimary;
    

    while(1){
        cap >> frame;
        if (frame.empty()) {
            cerr << "ERROR: Unable to grab from the camera" << endl;
            break;
        }
        capPrimary >> framePrimary;
        if (framePrimary.empty()) {
            cerr << "ERROR: Unable to grab from the camera" << endl;
            break;
        }

        //resize the frame

        //resize using the empherical starX startY endX endY


        int kwidth = ENDX- STARTX;
        int kheight = ENDY - STARTY;

        //read the frame from remote side and check the reoslution it is opened in - 1080p (16:9)
        // cv::resize(frame, resized, cv::Size(1920, 1080));
        cv::Rect myROI(STARTX,STARTY,kwidth,kheight);
        cv::Mat resized = frame(myROI);
    
        std::vector<FaceInfo> finalBox;
        if(i % std::stoi(argv[1]) == 0) {
            
            ultra->detect(resized,finalBox,resized.rows,resized.cols);


            std::cout<<"Face list: "<<finalBox.size()<<std::endl;

            std::cout<<"FaceList with the respective WxH: "<<std::endl;
            std::cout<<"-------------------------------------------"<<std::endl;
            for(const auto& ch : finalBox){
                std::cout<<"x1: "<<ch.x1<<" -- x2: "<<ch.x2<<std::endl;
                std::cout<<"y1: "<<ch.y1<<" -- y2: "<<ch.y2<<std::endl;
            }

            std::cout<<"----------------------------------------"<<std::endl;
            std::cout<<"----------------------------------------"<<std::endl;
        //!map this small image to actual image
        // float ovX1, ovY1, ovX2, ovY2;
            for(size_t i = 0; i < finalBox.size(); i++) {
                FaceInfo facebox = finalBox[i];
                float width  = facebox.x2-facebox.x1;
                float height = facebox.y2-facebox.y1;
                
                BoundingBox bb;
                bb.x1 = facebox.x1;
                bb.x2 = facebox.x2;
                bb.y1 = facebox.y1;
                bb.y2 = facebox.y2;
                cv::Rect box = cv::Rect(bb.x1 * (1280/kwidth), bb.y1* (720/kheight), (bb.x2-bb.x1)* (1280/kwidth), (bb.y2-bb.y1)* (720/kheight));
               

                // std::cout<<"ovX1: "<<ovX1 <<" --- ovX2: "<<ovX2<<std::endl;
                // std::cout<<"ovY1: "<<ovY1 <<" --- ovY2: "<<ovY2<<std::endl;

                // if(argv[3]) {
                //     std::vector<FaceInfo> locatedSpeaker;
                //     locatedSpeaker.push_back(identifyTheSpeaker(resized, finalBox));

                //     drawBiggerBoundingBox(locatedSpeaker, resized, 1);

                // } else {
                cv::Rect pbox = drawBiggerBoundingBox(finalBox, resized);
                if(pbox.empty()){
                    std::cout<<"Empty bounding box from OV camera"<<std::endl;
                    exit(1);
                }
                // drawBoundingBoxOnPrimaryDevice(resized, bb, finalBox);

                // }

                // cv::rectangle(resized, box, cv::Scalar(50, 50, 255), 2);

                // cv::rectangle(resized, cv::Point(0,0), cv::Point(30, 25), cv::Scalar(50, 50, 255), cv::FILLED);
                // putText(resized, std::to_string(finalBox.size()),Point(10,15),FONT_HERSHEY_SIMPLEX,0.6, Scalar(255, 0, 0), 2);

                // cv::Rect bboxPrimary = cv::Rect(bb.x1, bb.y1, (bb.x2-bb.x1), (bb.y2-bb.y1));
                cv::rectangle(framePrimary, box, cv::Scalar(255, 255, 255), 2);
                putText(framePrimary, std::to_string(finalBox.size()),Point(10,15),FONT_HERSHEY_SIMPLEX,0.6, Scalar(255, 0, 0), 2);

            }
        }
        // std::cout<<"breaking while loop"<<std::endl;
        // break;
        // double fps = cv::getTickFrequency() / (cv::getTickCount() - start);

        //show output
        // String winName1 = "[SECONDARY CAMERA]";
        // namedWindow(winName1, cv::WINDOW_NORMAL);
        // imshow(winName1, resized);
        // String winName = "[SECONDARY CAMERA]";

        String winName = "[PRIMARY CAMERA]";
        namedWindow(winName, cv::WINDOW_NORMAL);
        imshow(winName, framePrimary);

        i++;

        char esc = waitKey(5);
        if(esc == 27) break;
    }

    std::cout << "Closing the camera" << endl;
    cap.release();
    cv::destroyAllWindows();
    delete ultra;

    return 0;
}
