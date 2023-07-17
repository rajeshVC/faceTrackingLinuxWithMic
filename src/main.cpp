
#include "FaceDetection.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>

using namespace std;
using namespace cv;

int main(int argc, char **argv)
{
    //image
    if(argc > 1)
    {
        if (argc <= 2)
        {
            fprintf(stderr, "Usage: %s <mnn .mnn> [image files...]\n", argv[0]);
            return 1;
        }

        string mnn_path = argv[1];
        FaceDetection faceDetector(mnn_path, 640, 480, 4, 0.5); // config model input

        for (int i = 2; i < argc; i++)
        {
            string image_file = argv[i];
            cout << "Processing " << image_file << endl;

            cv::Mat frame = cv::imread(image_file);
            auto start = chrono::steady_clock::now();
            vector<FaceInfo> face_info;
            faceDetector.detect(frame, face_info);

            for (auto face : face_info)
            {
                cv::Point pt1(face.x1, face.y1);
                cv::Point pt2(face.x2, face.y2);
                cv::rectangle(frame, pt1, pt2, cv::Scalar(0, 255, 0), 1);
                cv::putText(frame, std::to_string(face_info.size()), cv::Point(5, 25), cv::FONT_HERSHEY_DUPLEX, 0.7, cv::Scalar(50, 50, 255), 2);
            }

            auto end = chrono::steady_clock::now();
            chrono::duration<double> elapsed = end - start;
            cout << "all time: " << elapsed.count() << " s" << endl;
            cv::namedWindow("FaceDetection", cv::WINDOW_NORMAL);
            cv::imshow("FaceDetection", frame);
            cv::waitKey();
            string result_name = "result" + to_string(i) + ".jpg";
            cv::imwrite(result_name, frame);
        }
    } else {
    
        Mat frame;
        // cv::setNumThreads(2);
        chrono::steady_clock::time_point Tbegin, Tend;

        FaceDetection *ultra = new FaceDetection(8);

        // std::thread thr(&FaceDetection::initVideoStream, std::ref(ultra));
        VideoCapture cap(0);
        // std::thread t1(&VideoCapture, std::ref(cap), 0);
        // VideoCapture cap("../resources/videos/Walks2.mp4");
        if (!cap.isOpened()) {
            cerr << "ERROR: Unable to open the video" << endl;
            return 0;
        }

        cout << "Start grabbing, press ESC on Live window to terminate" << endl;
        

        while(1){
            cap >> frame;
            if (frame.empty()) {
                cerr << "ERROR: Unable to grab from the camera" << endl;
                break;
            }

            //resize the frame
            // cv::Mat resized;
            // cv::resize(frame, resized, cv::Size(480, 340));

            Tbegin = chrono::steady_clock::now();

            std::vector<FaceInfo> finalBox;
            // ultra->detect(resized,finalBox,resized.rows,resized.cols);

            // for(size_t i = 0; i < finalBox.size(); i++) {
            //     FaceInfo facebox = finalBox[i];
            //     cv::Rect box=cv::Rect(facebox.x1,facebox.y1,(facebox.x2-facebox.x1),(facebox.y2-facebox.y1));
            //     // cv::rectangle(resized, box, cv::Scalar(50, 50, 255), 2);
            // }

            Tend = chrono::steady_clock::now();
            //calculate frame rate
            float f = chrono::duration_cast <chrono::milliseconds> (Tend - Tbegin).count();
            // putText(resized, std::to_string(finalBox.size()),Point(10,20),FONT_HERSHEY_SIMPLEX,0.6, Scalar(0, 0, 255));
            std::cout<<"FPS: "<<cap.get(CAP_PROP_FPS)<<std::endl;
            //show output
            imshow("Face detection", frame);

            char esc = waitKey(10);
            if(esc == 27) break;
        }

        cout << "Closing the camera" << endl;
        destroyAllWindows();
        delete ultra;
    }
    // #endif

    return 0;
}
