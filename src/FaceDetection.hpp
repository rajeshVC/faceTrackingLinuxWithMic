#ifndef FaceDetection_hpp
#define FaceDetection_hpp

#include "Interpreter.hpp"

#include "MNNDefine.h"
#include "Tensor.hpp"
#include "ImageProcess.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>

#define num_featuremap 4
#define hard_nms 1
#define blending_nms 2 /* mix nms was been proposaled in paper blaze face, aims to minimize the temporal jitter*/


typedef struct FaceInfo
{
	float x1;
	float y1;
	float x2;
	float y2;
	float score;
	float area;

	float landmarks[10];

} FaceInfo;

class FaceDetection
{
public:
    FaceDetection(const std::string &mnn_path,
              int input_width, int input_length, int num_thread_ = 4, float score_threshold_ = 0.7, 
              float iou_threshold_ = 0.3, int topk_ = -1);

	FaceDetection(int);

    ~FaceDetection();

    int detect(cv::Mat &img, std::vector<FaceInfo> &face_list);

	int detect(cv::Mat& img, std::vector<FaceInfo>& face_list,int resize_h,int resize_w,
		float score_threshold = 0.7, float nms_threshold = 0.4, int top_k = 10000);

    void initVideoStream();

private:
    void generateBBox(std::vector<FaceInfo> &bbox_collection, MNN::Tensor *scores, MNN::Tensor *boxes);
	
    void generateBBox(std::vector<FaceInfo>& collection, MNN::Tensor* score_map, MNN::Tensor* box_map, float score_threshold,
		int fea_w, int fea_h, int cols, int rows, int scale_id);
    
    void nms(std::vector<FaceInfo> &input, std::vector<FaceInfo> &output, int type = blending_nms);

	void get_topk_bbox(std::vector<FaceInfo>& input, std::vector<FaceInfo>& output, int topk);

private:
    std::shared_ptr<MNN::Interpreter> faceDetection_interpreter;
    MNN::Session *faceDetection_session = nullptr;
    MNN::Tensor *input_tensor = nullptr;
//
	std::vector< MNN::Tensor*> outputTensors;
	MNN::CV::ImageProcess::Config img_config;
//
    int num_thread;
    int image_w;
    int image_h;

    int in_w;
    int in_h;
    int num_anchors;

    int num_output_scales = -1;

    float score_threshold;
    float iou_threshold = 0.3;

    const float mean_vals[3] = {127, 127, 127};
    // const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
    const float norm_vals[3] = {1.0 / 128, 1.0 / 128, 1.0 / 128};
    // const float norm_vals[3] = {1.0 / 127.5, 1.0 / 127.5, 1.0 / 127.5};

    const float center_variance = 0.1;
    const float size_variance = 0.2;
    const std::vector<std::vector<float>> min_boxes = {
        {10.0f, 16.0f, 24.0f},
        {32.0f, 48.0f},
        {64.0f, 96.0f},
        {128.0f, 192.0f, 256.0f}};
    const std::vector<float> strides = {8.0, 16.0, 32.0, 64.0};
    std::vector<std::vector<float>> featuremap_size;
    std::vector<std::vector<float>> shrinkage_size;
    std::vector<int> w_h_list;

    std::vector<std::vector<float>> priors = {};

	std::string mnn_model_file;

	std::vector<float> receptive_field_list;
	std::vector<float> receptive_field_stride;
	std::vector<float> bbox_small_list;
	std::vector<float> bbox_large_list;
	std::vector<float> receptive_field_center_start;
	std::vector<float> constant;

	std::vector<std::string> output_blob_names;
};

#endif /* FaceDetection_hpp */
