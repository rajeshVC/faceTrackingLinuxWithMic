
#include "FaceDetection.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <random>
#include <thread>

#define NTH_FRAME 1
#define N_THREADS 2
#define CAMERA_FOV 140

using namespace std;
using namespace cv;


// bool isMicAccessEnabled = true;
//!for micDoA integration
struct FOV{
    int min = (180 - CAMERA_FOV) / 2;
    int max = 180 - min;
} FOV;

//!for micDoA integration
int micDoASimulator() {
    std::random_device rd;
    std::mt19937 mt(rd());

    std::uniform_int_distribution<std::mt19937::result_type> lk(0, 180);
    return lk(mt);
}

//! to identify the speaker and locate position/bbox
//!for micDoA integration
FaceInfo identifyTheSpeaker(const cv::Mat& image, const std::vector<FaceInfo>& faceList) {
    int angle = micDoASimulator();
    float nthPixelPerDegree = image.rows / 140.0;

    if(angle < FOV.min) {
        angle = FOV.min;
    } else if(angle > FOV.max) {
        angle = FOV.max;
    }

    angle -= FOV.min;

    float currentPixel = nthPixelPerDegree * angle;
    

    FaceInfo currentSpeaker = {};
    for(auto i=0; i<faceList.size(); i++){
        if(currentPixel < (faceList[i].x1 + faceList[i+1].x1)/2) {
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
    width = multiple * 16;
    height = multiple * 9;

    // std::cout<<"new aspect ratio: "<<width<<" x "<<height<<std::endl; 
    
}

void drawBiggerBoundingBox(const std::vector<FaceInfo>& in, const cv::Mat& resized, int type=0){
    
    float x1_min = in[0].x1, y1_min = in[0].y1, x2_max = in[0].x2, y2_max = in[0].y2;

    std::cout<<"FaceList with the respective WxH: "<<std::endl;
    std::cout<<"-------------------------------------------"<<std::endl;
    for(const auto& ch : in){
        std::cout<<"x1: "<<ch.x1<<" -- x2: "<<ch.x2<<std::endl;
        std::cout<<"y1: "<<ch.y1<<" -- y2: "<<ch.y2<<std::endl;
    }

    std::cout<<"----------------------------------------"<<std::endl;
    
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
    // std::cout<<"old aspect ratio: "<<occupied_width<<" x "<<occupied_height<<std::endl;
    
    resetToAspectRatio(bbox_w, bbox_h);

    if(in.size() == 1) {
        bbox_w *= 2.3;
        bbox_h *= 2.3;
        
    }

    float new_x1 = x1_min == 0 ? 0 : x1_min - ((bbox_w - occupied_width)/2);
    float new_y1 = y1_min == 0 ? 0 : y1_min - ((bbox_h - occupied_height)/2);
    
    if(in.size() == 1){
        new_y1 += (y1_min - new_y1) / 2;
    }
   
    cv::Rect bbox = cv::Rect(new_x1, new_y1, bbox_w, bbox_h);
    cv::rectangle(resized, bbox, cv::Scalar(0, 0, 0), 2);

    if(in.size() == 1 && type) {
        cv::putText(resized, "Speaker", cv::Point(new_x1, new_y1), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0,255,255), 1);
    }
    
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

    VideoCapture cap(2);
    // VideoCapture cap("../resources/videos/Walks2.mp4");
    if (!cap.isOpened()) {
        cerr << "ERROR: Unable to open the video" << endl;
        return 0;
    }

    std::cout << "Start grabbing, press ESC on Live window to terminate" << endl;

    int i = 0;

    while(1){
        cap >> frame;
        if (frame.empty()) {
            cerr << "ERROR: Unable to grab from the camera" << endl;
            break;
        }

        //resize the frame
        cv::Mat resized;
        cv::resize(frame, resized, cv::Size(320, 240));

        // int64 start = cv::getTickCount();

        std::vector<FaceInfo> finalBox;
        if(i % std::stoi(argv[1]) == 0) {
            
            ultra->detect(resized,finalBox,resized.rows,resized.cols);

            std::cout<<"Face list: "<<finalBox.size()<<std::endl;
            for(size_t i = 0; i < finalBox.size(); i++) {
                FaceInfo facebox = finalBox[i];
                float width  = facebox.x2-facebox.x1;
                float height = facebox.y2-facebox.y1;
                cv::Rect box=cv::Rect(facebox.x1, facebox.y1, width, height);

                if(argv[3]) {
                    std::vector<FaceInfo> locatedSpeaker;
                    locatedSpeaker.push_back(identifyTheSpeaker(resized, finalBox));

                    drawBiggerBoundingBox(locatedSpeaker, resized, 1);

                } else {
                    drawBiggerBoundingBox(finalBox, resized);

                }

                cv::rectangle(resized, box, cv::Scalar(50, 50, 255), 2);

                cv::rectangle(resized, cv::Point(0,0), cv::Point(30, 25), cv::Scalar(50, 50, 255), cv::FILLED);
                putText(resized, std::to_string(finalBox.size()),Point(10,15),FONT_HERSHEY_SIMPLEX,0.6, Scalar(255, 0, 0), 2);

            }
        }


        // double fps = cv::getTickFrequency() / (cv::getTickCount() - start);

        //show output
        String winName = "Face Detection";
        namedWindow(winName, cv::WINDOW_NORMAL);
        imshow(winName, resized);

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
