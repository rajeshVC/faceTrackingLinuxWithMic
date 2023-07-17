cmake_minimum_required(VERSION 3.10)
project(FaceDetection)

set(CMAKE_CXX_STANDARD 11)

find_package(OpenCV REQUIRED)
include_directories(
        mnn/include
        src
)

link_directories(mnn/lib)

add_executable(FaceDetection src/main.cpp src/FaceDetection.cpp)
target_link_libraries(FaceDetection MNN ${OpenCV_LIBS})