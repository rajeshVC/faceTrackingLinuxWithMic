

#define clip(x, y) (x < 0 ? 0 : (x > y ? y : x))

#include "FaceDetection.hpp"

using namespace std;

FaceDetection::FaceDetection(const std::string &mnn_path,
                     int input_width, int input_length, int num_thread_,
                     float score_threshold_, float iou_threshold_, int topk_) {
    num_thread = num_thread_;
    score_threshold = score_threshold_;
    iou_threshold = iou_threshold_;
    in_w = input_width;
    in_h = input_length;
    w_h_list = {in_w, in_h};

    for (auto size : w_h_list) {
        std::vector<float> fm_item;
        for (float stride : strides) {
            fm_item.push_back(ceil(size / stride));
        }
        featuremap_size.push_back(fm_item);
    }

    for (auto size : w_h_list) {
        shrinkage_size.push_back(strides);
    }
    /* generate prior anchors */
    for (int index = 0; index < num_featuremap; index++) {
        float scale_w = in_w / shrinkage_size[0][index];
        float scale_h = in_h / shrinkage_size[1][index];
        for (int j = 0; j < featuremap_size[1][index]; j++) {
            for (int i = 0; i < featuremap_size[0][index]; i++) {
                float x_center = (i + 0.5) / scale_w;
                float y_center = (j + 0.5) / scale_h;

                for (float k : min_boxes[index]) {
                    float w = k / in_w;
                    float h = k / in_h;
                    priors.push_back({clip(x_center, 1), clip(y_center, 1), clip(w, 1), clip(h, 1)});
                }
            }
        }
    }
    /* generate prior anchors finished */

    num_anchors = priors.size();
    std::cout<<"num_anchors (priors size()): "<<num_anchors<<std::endl;

    faceDetection_interpreter = std::shared_ptr<MNN::Interpreter>(MNN::Interpreter::createFromFile(mnn_path.c_str()));
    MNN::ScheduleConfig config;
    config.numThread = num_thread;
    MNN::BackendConfig backendConfig;
    backendConfig.precision = (MNN::BackendConfig::PrecisionMode) 2;
    config.backendConfig = &backendConfig;

    faceDetection_session = faceDetection_interpreter->createSession(config);

    input_tensor = faceDetection_interpreter->getSessionInput(faceDetection_session, nullptr);

}

FaceDetection::FaceDetection(int scale_num)
{
    // initVideoStream(scale_num);
    num_output_scales = scale_num;
	num_thread = 4;
    outputTensors.resize(num_output_scales*2);

	if (num_output_scales == 5) {
        mnn_model_file = "../models/symbol_10_320_20L_5scales_v2_deploy.mnn";
		receptive_field_list = { 20, 40, 80, 160, 320 };
		receptive_field_stride = { 4, 8, 16, 32, 64 };
		bbox_small_list = { 10, 20, 40, 80, 160 };
		bbox_large_list = { 20, 40, 80, 160, 320 };
		receptive_field_center_start = { 3, 7, 15, 31, 63 };

		for(size_t i = 0; i < receptive_field_list.size(); i++) {
			constant.push_back(receptive_field_list[i] / 2);
		}

		output_blob_names = { "softmax0","conv8_3_bbox",
		                                  "softmax1","conv11_3_bbox",
										  "softmax2","conv14_3_bbox",
		                                  "softmax3","conv17_3_bbox",
		                                  "softmax4","conv20_3_bbox" };
	}
	else if (num_output_scales == 8) {
        mnn_model_file = "../models/symbol_10_560_25L_8scales_v1_deploy.mnn";
		receptive_field_list = { 15, 20, 40, 70, 110, 250, 400, 560 };
		receptive_field_stride = { 4, 4, 8, 8, 16, 32, 32, 32 };
		bbox_small_list = { 10, 15, 20, 40, 70, 110, 250, 400 };
		bbox_large_list = { 15, 20, 40, 70, 110, 250, 400, 560 };
		receptive_field_center_start = { 3, 3, 7, 7, 15, 31, 31, 31 };

		for(size_t i = 0; i < receptive_field_list.size(); i++) {
			constant.push_back(receptive_field_list[i] / 2);
		}

		output_blob_names = { "softmax0","conv8_3_bbox",
			"softmax1","conv10_3_bbox",
			"softmax2","conv13_3_bbox",
			"softmax3","conv15_3_bbox",
			"softmax4","conv18_3_bbox",
			"softmax5","conv21_3_bbox",
			"softmax6","conv23_3_bbox",
		    "softmax7","conv25_3_bbox" };
	}

	faceDetection_interpreter = std::shared_ptr<MNN::Interpreter>(MNN::Interpreter::createFromFile(mnn_model_file.c_str()));
	MNN::ScheduleConfig config;
	config.type = MNN_FORWARD_CPU;
	config.numThread = num_thread;

	MNN::BackendConfig backendConfig;
	backendConfig.precision = MNN::BackendConfig::Precision_High;
	backendConfig.power = MNN::BackendConfig::Power_Low;
	config.backendConfig = &backendConfig;

	faceDetection_session = faceDetection_interpreter->createSession(config);
	input_tensor = faceDetection_interpreter->getSessionInput(faceDetection_session, NULL);
	for(size_t i = 0; i < output_blob_names.size(); i++) {
		outputTensors[i] = faceDetection_interpreter->getSessionOutput(faceDetection_session, output_blob_names[i].c_str());
	}

	::memcpy(img_config.mean, mean_vals, sizeof(mean_vals));
	::memcpy(img_config.normal, norm_vals, sizeof(norm_vals));


	img_config.sourceFormat = (MNN::CV::ImageFormat)2;
	img_config.destFormat = (MNN::CV::ImageFormat)2;

	img_config.filterType = (MNN::CV::Filter)(2);
	img_config.wrap = (MNN::CV::Wrap)(2);
}

FaceDetection::~FaceDetection() {
    faceDetection_interpreter->releaseModel();
    faceDetection_interpreter->releaseSession(faceDetection_session);
}

int FaceDetection::detect(cv::Mat &raw_image, std::vector<FaceInfo> &face_list) {
    if (raw_image.empty()) {
        std::cout << "image is empty ,please check!" << std::endl;
        return -1;
    }

    image_h = raw_image.rows;
    image_w = raw_image.cols;
    cv::Mat image;
    cv::resize(raw_image, image, cv::Size(in_w, in_h));

    faceDetection_interpreter->resizeTensor(input_tensor, {1, 3, in_h, in_w});
    faceDetection_interpreter->resizeSession(faceDetection_session);
    // std::shared_ptr<MNN::CV::ImageProcess> pretreat(
    //         MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::BGR, mean_vals, 3,
    //                                       norm_vals, 3));
    std::shared_ptr<MNN::CV::ImageProcess> pretreat(
            MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::RGB, mean_vals, 3,
                                          norm_vals, 3));
    pretreat->convert(image.data, in_w, in_h, image.step[0], input_tensor);

    auto start = chrono::steady_clock::now();


    // run network
    faceDetection_interpreter->runSession(faceDetection_session);

    // get output data

    string scores = "scores";
    string boxes = "boxes";
    MNN::Tensor *tensor_scores = faceDetection_interpreter->getSessionOutput(faceDetection_session, scores.c_str());
    MNN::Tensor *tensor_boxes = faceDetection_interpreter->getSessionOutput(faceDetection_session, boxes.c_str());

    MNN::Tensor tensor_scores_host(tensor_scores, tensor_scores->getDimensionType());

    tensor_scores->copyToHostTensor(&tensor_scores_host);

    MNN::Tensor tensor_boxes_host(tensor_boxes, tensor_boxes->getDimensionType());

    tensor_boxes->copyToHostTensor(&tensor_boxes_host);

    std::vector<FaceInfo> bbox_collection;


    auto end = chrono::steady_clock::now();
    chrono::duration<double> elapsed = end - start;
    cout << "inference time:" << elapsed.count() << " s" << endl;

    generateBBox(bbox_collection, tensor_scores, tensor_boxes);
    nms(bbox_collection, face_list);
    return 0;
}

int FaceDetection::detect(cv::Mat &img, std::vector<FaceInfo> &face_list, int resize_h, int resize_w, float score_threshold, float nms_threshold, int top_k, std::vector<int> skip_scale_branch_list)
{

	if (img.empty()) {
		std::cout << "image is empty ,please check!" << std::endl;
		return -1;
	}

	image_h = img.rows;
	image_w = img.cols;

    cv::Mat in;
    cv::resize(img,in,cv::Size(resize_w,resize_h));
    float ratio_w=(float)image_w/ resize_w;
    float ratio_h=(float)image_h/ resize_h;

	//resize session and input tensor
	std::vector<int> inputDims = { 1, 3, resize_h, resize_w };
	std::vector<int> shape = input_tensor->shape();
	shape[0] = 1;
	shape[2] = resize_h;
	shape[3] = resize_w;
	faceDetection_interpreter->resizeTensor(input_tensor, shape);
	faceDetection_interpreter->resizeSession(faceDetection_session);

	//prepare data
	std::shared_ptr<MNN::CV::ImageProcess> pretreat(MNN::CV::ImageProcess::create(img_config));
	pretreat->convert(in.data, resize_w, resize_h, in.step[0], input_tensor);


	//forward
	faceDetection_interpreter->runSession(faceDetection_session);

	std::vector<FaceInfo> bbox_collection;
	for (int i = 0; i <num_output_scales; i++) {

		MNN::Tensor* tensor_score = new MNN::Tensor(outputTensors[2*i], MNN::Tensor::CAFFE);
		outputTensors[2*i]->copyToHostTensor(tensor_score);

		MNN::Tensor* tensor_location = new MNN::Tensor(outputTensors[2*i+1], MNN::Tensor::CAFFE);
		outputTensors[2 * i + 1]->copyToHostTensor(tensor_location);

		std::vector<float> score;
		std::vector<float> location;
		std::vector<int> shape_score= tensor_score->shape();
		std::vector<int> shape_loc=tensor_location->shape();
		for (int j = 0; j < 100; j++) {
			score.push_back(tensor_score->host<float>()[j]);
		}

		for (int j = 0; j < 100; j++) {
			location.push_back(tensor_location->host<float>()[j]);
			//std::cout << location[j] << std::endl;
		}

		generateBBox(bbox_collection, tensor_score, tensor_location, score_threshold,
			tensor_score->width(), tensor_score->height(), img.cols, img.rows, i);

		delete tensor_score;
		delete tensor_location;
	}
	std::vector<FaceInfo> valid_input;
	get_topk_bbox(bbox_collection, valid_input, top_k);
	// nms(valid_input, face_list, nms_threshold);
    nms(valid_input, face_list);

    for(size_t i=0;i<face_list.size();i++){
        face_list[i].x1*=ratio_w;
        face_list[i].y1*=ratio_h;
        face_list[i].x2*=ratio_w;
        face_list[i].y2*=ratio_h;

        float w,h,maxSize;
        float cenx,ceny;
        w=face_list[i].x2-face_list[i].x1;
        h=face_list[i].y2-face_list[i].y1;

		maxSize = w > h ? w : h;
        cenx=face_list[i].x1+w/2;
        ceny=face_list[i].y1+h/2;
        face_list[i].x1=cenx-maxSize/2>0? cenx - maxSize / 2:0;
        face_list[i].y1=ceny-maxSize/2>0? ceny - maxSize / 2:0;
        face_list[i].x2=cenx+maxSize/2>image_w? image_w-1: cenx + maxSize / 2;
        face_list[i].y2=ceny+maxSize/2> image_h? image_h-1: ceny + maxSize / 2;

    }
	return 0;
}

void FaceDetection::generateBBox(std::vector<FaceInfo> &bbox_collection, MNN::Tensor *scores, MNN::Tensor *boxes) {
    for (int i = 0; i < num_anchors; i++) {
        std::cout<<"score_threshold: "<<score_threshold<<std::endl;
        if (scores->host<float>()[i * 2 + 1] > score_threshold) {
            // scores->host<float>()[i * 2 + 1] < 0.6 ) {
            std::cout<<"scores->host<float>()[i * 2 + 1]: "<<scores->host<float>()[i * 2 + 1]<<std::endl;

            FaceInfo rects;
            float x_center = boxes->host<float>()[i * 4] * center_variance * priors[i][2] + priors[i][0];
            float y_center = boxes->host<float>()[i * 4 + 1] * center_variance * priors[i][3] + priors[i][1];
            float w = exp(boxes->host<float>()[i * 4 + 2] * size_variance) * priors[i][2];
            float h = exp(boxes->host<float>()[i * 4 + 3] * size_variance) * priors[i][3];

            rects.x1 = clip(x_center - w / 2.0, 1) * image_w;
            rects.y1 = clip(y_center - h / 2.0, 1) * image_h;
            rects.x2 = clip(x_center + w / 2.0, 1) * image_w;
            rects.y2 = clip(y_center + h / 2.0, 1) * image_h;
            rects.score = clip(scores->host<float>()[i * 2 + 1], 1);
            bbox_collection.push_back(rects);
        }
    }
}

void FaceDetection::generateBBox(std::vector<FaceInfo> &bbox_collection, MNN::Tensor *score_map, MNN::Tensor *box_map, float score_threshold, int fea_w, int fea_h, int cols, int rows, int scale_id)
{
	float* RF_center_Xs = new float[fea_w];
	float* RF_center_Xs_mat = new float[fea_w * fea_h];
	float* RF_center_Ys = new float[fea_h];
	float* RF_center_Ys_mat = new float[fea_h * fea_w];

    for (int x = 0; x < fea_w; x++) {
		RF_center_Xs[x] = receptive_field_center_start[scale_id] + receptive_field_stride[scale_id] * x;
	}
	for (int x = 0; x < fea_h; x++) {
		for (int y = 0; y < fea_w; y++) {
			RF_center_Xs_mat[x * fea_w + y] = RF_center_Xs[y];
		}
	}

	for (int x = 0; x < fea_h; x++) {
		RF_center_Ys[x] = receptive_field_center_start[scale_id] + receptive_field_stride[scale_id] * x;
		for (int y = 0; y < fea_w; y++) {
			RF_center_Ys_mat[x * fea_w + y] = RF_center_Ys[x];
		}
	}

	float* x_lt_mat = new float[fea_h * fea_w];
	float* y_lt_mat = new float[fea_h * fea_w];
	float* x_rb_mat = new float[fea_h * fea_w];
	float* y_rb_mat = new float[fea_h * fea_w];



	//x-left-top
	float mid_value = 0;

	float* box_map_ptr = box_map->host<float>();
	int fea_spacial_size = fea_h * fea_w;
	for (int j = 0; j < fea_spacial_size; j++) {
		mid_value = RF_center_Xs_mat[j] - box_map_ptr[0*fea_spacial_size+j] * constant[scale_id];
		x_lt_mat[j] = mid_value < 0 ? 0 : mid_value;
	}
	//y-left-top
	for (int j = 0; j < fea_spacial_size; j++) {
		mid_value = RF_center_Ys_mat[j] - box_map_ptr[1 * fea_spacial_size + j] * constant[scale_id];
		y_lt_mat[j] = mid_value < 0 ? 0 : mid_value;
	}
	//x-right-bottom
	for (int j = 0; j < fea_spacial_size; j++) {
		mid_value = RF_center_Xs_mat[j] - box_map_ptr[2 * fea_spacial_size + j] * constant[scale_id];
		x_rb_mat[j] = mid_value > cols - 1 ? cols - 1 : mid_value;
	}
	//y-right-bottom
	for (int j = 0; j < fea_spacial_size; j++) {
		mid_value = RF_center_Ys_mat[j] - box_map_ptr[3 * fea_spacial_size + j] * constant[scale_id];
		y_rb_mat[j] = mid_value > rows - 1 ? rows - 1 : mid_value;
	}

	float* score_map_ptr = score_map->host<float>();


	for (int k = 0; k < fea_spacial_size; k++) {
		if (score_map_ptr[k] > score_threshold) {
			FaceInfo faceinfo;
			faceinfo.x1 = x_lt_mat[k];
			faceinfo.y1 = y_lt_mat[k];
			faceinfo.x2 = x_rb_mat[k];
			faceinfo.y2 = y_rb_mat[k];
			faceinfo.score = score_map_ptr[k];
			faceinfo.area = (faceinfo.x2 - faceinfo.x1) * (faceinfo.y2 - faceinfo.y1);
			bbox_collection.push_back(faceinfo);
		}
	}

	delete[] RF_center_Xs; RF_center_Xs = NULL;
	delete[] RF_center_Ys; RF_center_Ys = NULL;
	delete[] RF_center_Xs_mat; RF_center_Xs_mat = NULL;
	delete[] RF_center_Ys_mat; RF_center_Ys_mat = NULL;
	delete[] x_lt_mat; x_lt_mat = NULL;
	delete[] y_lt_mat; y_lt_mat = NULL;
	delete[] x_rb_mat; x_rb_mat = NULL;
	delete[] y_rb_mat; y_rb_mat = NULL;
}

void FaceDetection::nms(std::vector<FaceInfo> &input, std::vector<FaceInfo> &output, int type) {
    std::sort(input.begin(), input.end(), [](const FaceInfo &a, const FaceInfo &b) { return a.score > b.score; });

	if (input.empty()) {
		return;
	}
    int box_num = input.size();

    std::vector<int> merged(box_num, 0);

    for (int i = 0; i < box_num; i++) {
        if (merged[i])
            continue;
        std::vector<FaceInfo> buf;

        buf.push_back(input[i]);
        merged[i] = 1;

        float h0 = input[i].y2 - input[i].y1 + 1;
        float w0 = input[i].x2 - input[i].x1 + 1;

        float area0 = h0 * w0;

        for (int j = i + 1; j < box_num; j++) {
            if (merged[j])
                continue;

            float inner_x0 = input[i].x1 > input[j].x1 ? input[i].x1 : input[j].x1;
            float inner_y0 = input[i].y1 > input[j].y1 ? input[i].y1 : input[j].y1;

            float inner_x1 = input[i].x2 < input[j].x2 ? input[i].x2 : input[j].x2;
            float inner_y1 = input[i].y2 < input[j].y2 ? input[i].y2 : input[j].y2;

            float inner_h = inner_y1 - inner_y0 + 1;
            float inner_w = inner_x1 - inner_x0 + 1;

            if (inner_h <= 0 || inner_w <= 0)
                continue;

            float inner_area = inner_h * inner_w;

            float h1 = input[j].y2 - input[j].y1 + 1;
            float w1 = input[j].x2 - input[j].x1 + 1;

            float area1 = h1 * w1;

            float score;

            score = inner_area / (area0 + area1 - inner_area);

            if (score > iou_threshold) {
                merged[j] = 1;
                buf.push_back(input[j]);
            }
        }
        switch (type) {
            case hard_nms: {
                output.push_back(buf[0]);
                break;
            }
            case blending_nms: {
                float total = 0;
                for (int i = 0; i < buf.size(); i++) {
                    total += exp(buf[i].score);
                }
                FaceInfo rects;
                memset(&rects, 0, sizeof(rects));
                for (int i = 0; i < buf.size(); i++) {
                    float rate = exp(buf[i].score) / total;
                    rects.x1 += buf[i].x1 * rate;
                    rects.y1 += buf[i].y1 * rate;
                    rects.x2 += buf[i].x2 * rate;
                    rects.y2 += buf[i].y2 * rate;
                    rects.score += buf[i].score * rate;
                }
                output.push_back(rects);
                break;
            }
            default: {
                printf("wrong type of nms.");
                exit(-1);
            }
        }
    }
}

void FaceDetection::get_topk_bbox(std::vector<FaceInfo> &input, std::vector<FaceInfo> &output, int top_k)
{
	std::sort(input.begin(), input.end(),
		[](const FaceInfo& a, const FaceInfo& b)
		{
			return a.score > b.score;
		});

	if (input.size() > size_t(top_k)) {
		for (int k = 0; k < top_k; k++) {
			output.push_back(input[k]);
		}
	}
	else {
		output = input;
	}
}

void FaceDetection::initVideoStream()
{
    
    num_output_scales = 5;
	num_thread = 4;
    outputTensors.resize(num_output_scales*2);

	if (num_output_scales == 5) {
        mnn_model_file = "../models/symbol_10_320_20L_5scales_v2_deploy.mnn";
		receptive_field_list = { 20, 40, 80, 160, 320 };
		receptive_field_stride = { 4, 8, 16, 32, 64 };
		bbox_small_list = { 10, 20, 40, 80, 160 };
		bbox_large_list = { 20, 40, 80, 160, 320 };
		receptive_field_center_start = { 3, 7, 15, 31, 63 };

		for(size_t i = 0; i < receptive_field_list.size(); i++) {
			constant.push_back(receptive_field_list[i] / 2);
		}

		output_blob_names = { "softmax0","conv8_3_bbox",
		                                  "softmax1","conv11_3_bbox",
										  "softmax2","conv14_3_bbox",
		                                  "softmax3","conv17_3_bbox",
		                                  "softmax4","conv20_3_bbox" };
	}
	else if (num_output_scales == 8) {
        mnn_model_file = "../models/symbol_10_560_25L_8scales_v1_deploy.mnn";
		receptive_field_list = { 15, 20, 40, 70, 110, 250, 400, 560 };
		receptive_field_stride = { 4, 4, 8, 8, 16, 32, 32, 32 };
		bbox_small_list = { 10, 15, 20, 40, 70, 110, 250, 400 };
		bbox_large_list = { 15, 20, 40, 70, 110, 250, 400, 560 };
		receptive_field_center_start = { 3, 3, 7, 7, 15, 31, 31, 31 };

		for(size_t i = 0; i < receptive_field_list.size(); i++) {
			constant.push_back(receptive_field_list[i] / 2);
		}

		output_blob_names = { "softmax0","conv8_3_bbox",
			"softmax1","conv10_3_bbox",
			"softmax2","conv13_3_bbox",
			"softmax3","conv15_3_bbox",
			"softmax4","conv18_3_bbox",
			"softmax5","conv21_3_bbox",
			"softmax6","conv23_3_bbox",
		    "softmax7","conv25_3_bbox" };
	}

	faceDetection_interpreter = std::shared_ptr<MNN::Interpreter>(MNN::Interpreter::createFromFile(mnn_model_file.c_str()));
	MNN::ScheduleConfig config;
	config.type = MNN_FORWARD_CPU;
	config.numThread = num_thread;

	MNN::BackendConfig backendConfig;
	backendConfig.precision = MNN::BackendConfig::Precision_High;
	backendConfig.power = MNN::BackendConfig::Power_High;
	config.backendConfig = &backendConfig;

	faceDetection_session = faceDetection_interpreter->createSession(config);
	input_tensor = faceDetection_interpreter->getSessionInput(faceDetection_session, NULL);
	for(size_t i = 0; i < output_blob_names.size(); i++) {
		outputTensors[i] = faceDetection_interpreter->getSessionOutput(faceDetection_session, output_blob_names[i].c_str());
	}

	::memcpy(img_config.mean, mean_vals, sizeof(mean_vals));
	::memcpy(img_config.normal, norm_vals, sizeof(norm_vals));


	img_config.sourceFormat = (MNN::CV::ImageFormat)2;
	img_config.destFormat = (MNN::CV::ImageFormat)2;

	img_config.filterType = (MNN::CV::Filter)(2);
	img_config.wrap = (MNN::CV::Wrap)(2);

}
