
#include "FaceDetection.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
// #include <chrono>

#define NTH_FRAME 1
#define N_THREADS 2

using namespace std;
using namespace cv;

int main(int argc, char **argv)
{
     if(argc <= 1) {
        fprintf(stderr, "Usage: %s <nth Frame> <no of threads>\n", argv[0]);
        return 1;
     }
    Mat frame;
    chrono::steady_clock::time_point Tbegin, Tend;

    FaceDetection *ultra = new FaceDetection(std::stoi(argv[2]));

    VideoCapture cap(0);
    // VideoCapture cap("../resources/videos/Walks2.mp4");
    if (!cap.isOpened()) {
        cerr << "ERROR: Unable to open the video" << endl;
        return 0;
    }

    cout << "Start grabbing, press ESC on Live window to terminate" << endl;

    int i = 0;
    // std::cout<<"Nth frame: "<<argv[1]<<" ---- nthreads: "<<argv[2]<<std::endl;
    while(1){
        cap >> frame;
        if (frame.empty()) {
            cerr << "ERROR: Unable to grab from the camera" << endl;
            break;
        }

        
        //resize the frame
        cv::Mat resized;
        cv::resize(frame, resized, cv::Size(160, 120));

        // Tbegin = chrono::steady_clock::now();
        int64 start = cv::getTickCount();

        std::vector<FaceInfo> finalBox;
        if(i % std::stoi(argv[1]) == 0) {
            // frameReading = false;
            cv::cvtColor(resized, resized, cv::COLOR_BGR2GRAY);
            ultra->detect(resized,finalBox,resized.rows,resized.cols);
            for(size_t i = 0; i < finalBox.size(); i++) {
                FaceInfo facebox = finalBox[i];
                cv::Rect box=cv::Rect(facebox.x1,facebox.y1,(facebox.x2-facebox.x1),(facebox.y2-facebox.y1));
                cv::rectangle(resized, box, cv::Scalar(50, 50, 255), 2);
                cv::rectangle(resized, cv::Point(0,0), cv::Point(30, 25), cv::Scalar(50, 50, 255), cv::FILLED);
                putText(resized, std::to_string(finalBox.size()),Point(10,15),FONT_HERSHEY_SIMPLEX,0.6, Scalar(255, 0, 0), 2);

            }
        }

        // Tend = chrono::steady_clock::now();
        double fps = cv::getTickFrequency() / (cv::getTickCount() - start);


        //calculate frame rate
        // float f = chrono::duration_cast <chrono::milliseconds> (Tend - Tbegin).count();

        //show output
        String winName = "Face Detection";
        namedWindow(winName, cv::WINDOW_NORMAL);
        imshow(winName, resized);

        i++;

        char esc = waitKey(5);
        if(esc == 27) break;
    }

    cout << "Closing the camera" << endl;
    destroyAllWindows();
    delete ultra;

    return 0;
}
