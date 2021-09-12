#pragma once

#include <gst/gst.h>
#include <glib.h>
#include "gstnvdsmeta.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>

// opencv libraries
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

// cuda libraries
#include <cuda.h>
#include <cuda_runtime_api.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#define VIDEO_PATH "test.h264"

#define GST_CAPS_FEATURES_NVMM "memory:NVMM"

// Muxer Resolution
#define MUXER_OUTPUT_WIDTH 1920
#define MUXER_OUTPUT_HEIGHT 1080
#define MUXER_BATCH_TIMEOUT_USEC 40000
#define PGIE_YOLO_DETECTOR_CONFIG_FILE_PATH "models/YOLOv4/config_infer_primary_yolov4.txt"
#define PGIE_YOLO_ENGINE_PATH "models/YOLOv4/yolov4.weights_b1_gpu0_fp32.engine"
#define MAX_DISPLAY_LEN 64

struct ObjCount{
    gint PersonID = 0;
    gint CarID = 2;
    gint PersonCount= 0;
    gint CarCount = 0;
};

