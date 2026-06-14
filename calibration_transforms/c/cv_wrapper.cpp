#include "cv_wrapper.h"
#include <opencv2/opencv.hpp>
#include <cstring>
#include <iostream>

struct CvImage {
    cv::Mat mat;
};

struct CvCapture {
    cv::VideoCapture cap;
};

// Image operations
CvImage* cv_wrapper_image_create_empty(void) {
    return new CvImage();
}

CvImage* cv_wrapper_image_create(int width, int height, int channels) {
    CvImage* img = new CvImage();
    int type = (channels == 3) ? CV_8UC3 : ((channels == 1) ? CV_8UC1 : CV_8UC4);
    img->mat = cv::Mat::zeros(height, width, type);
    return img;
}

CvImage* cv_wrapper_imread(const char* filename) {
    CvImage* img = new CvImage();
    img->mat = cv::imread(filename);
    if (img->mat.empty()) {
        delete img;
        return NULL;
    }
    return img;
}

void cv_wrapper_image_free(CvImage* img) {
    if (img) {
        delete img;
    }
}

int cv_wrapper_image_width(const CvImage* img) {
    return img ? img->mat.cols : 0;
}

int cv_wrapper_image_height(const CvImage* img) {
    return img ? img->mat.rows : 0;
}

void cv_wrapper_image_resize(const CvImage* src, CvImage* dst, int width, int height) {
    if (src && dst) {
        cv::resize(src->mat, dst->mat, cv::Size(width, height));
    }
}

void cv_wrapper_image_copy_to(const CvImage* src, CvImage* dst) {
    if (src && dst) {
        src->mat.copyTo(dst->mat);
    }
}

CvImage* cv_wrapper_image_clone(const CvImage* img) {
    if (!img) return NULL;
    CvImage* cln = new CvImage();
    cln->mat = img->mat.clone();
    return cln;
}

// Drawing and Overlay functions
void cv_wrapper_line(CvImage* img, int x1, int y1, int x2, int y2, int r, int g, int b, int thickness) {
    if (img) {
        cv::line(img->mat, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(b, g, r), thickness, cv::LINE_AA);
    }
}

void cv_wrapper_rectangle(CvImage* img, int x1, int y1, int x2, int y2, int r, int g, int b, int thickness, int fill) {
    if (img) {
        int thick = fill ? -1 : thickness;
        cv::rectangle(img->mat, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(b, g, r), thick);
    }
}

void cv_wrapper_circle(CvImage* img, int cx, int cy, int radius, int r, int g, int b, int thickness) {
    if (img) {
        cv::circle(img->mat, cv::Point(cx, cy), radius, cv::Scalar(b, g, r), thickness, cv::LINE_AA);
    }
}

void cv_wrapper_draw_marker(CvImage* img, int x, int y, int r, int g, int b, int marker_type, int size, int thickness) {
    if (img) {
        cv::drawMarker(img->mat, cv::Point(x, y), cv::Scalar(b, g, r), marker_type, size, thickness);
    }
}

void cv_wrapper_put_text(CvImage* img, const char* text, int x, int y, double font_scale, int r, int g, int b, int thickness) {
    if (img) {
        cv::putText(img->mat, text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(b, g, r), thickness, cv::LINE_AA);
    }
}

void cv_wrapper_hstack(const CvImage* img1, const CvImage* img2, CvImage* dst) {
    if (img1 && img2 && dst) {
        cv::hconcat(img1->mat, img2->mat, dst->mat);
    }
}

void cv_wrapper_add_weighted(const CvImage* src1, double alpha, const CvImage* src2, double beta, double gamma, CvImage* dst) {
    if (src1 && src2 && dst) {
        cv::addWeighted(src1->mat, alpha, src2->mat, beta, gamma, dst->mat);
    }
}

// Matrix Operations (for mapping maps caching)
void* cv_wrapper_create_empty_mat(void) {
    return new cv::Mat();
}

void cv_wrapper_free_mat(void* mat) {
    if (mat) {
        delete static_cast<cv::Mat*>(mat);
    }
}

// Undistort maps initialization and remap
void cv_wrapper_init_rectification_maps(
    const double K[9], const double D[8], int D_len,
    const char* model_type, int width, int height,
    void* map1, void* map2
) {
    cv::Mat K_mat = cv::Mat(3, 3, CV_64F, const_cast<double*>(K));
    cv::Mat D_mat = cv::Mat(D_len, 1, CV_64F, const_cast<double*>(D));
    cv::Mat* m1 = static_cast<cv::Mat*>(map1);
    cv::Mat* m2 = static_cast<cv::Mat*>(map2);

    if (strcmp(model_type, "pinhole") == 0) {
        cv::initUndistortRectifyMap(K_mat, D_mat, cv::Mat(), K_mat, cv::Size(width, height), CV_16SC2, *m1, *m2);
    } else {
        // OpenCV fisheye expects a 4-element distortion array
        cv::Mat D_fe = D_mat.clone();
        if (D_fe.total() > 4) {
            D_fe = D_fe.rowRange(0, 4);
        }
        cv::fisheye::initUndistortRectifyMap(K_mat, D_fe, cv::Mat(), K_mat, cv::Size(width, height), CV_16SC2, *m1, *m2);
    }
}

void cv_wrapper_remap(const CvImage* src, CvImage* dst, const void* map1, const void* map2) {
    if (src && dst && map1 && map2) {
        const cv::Mat* m1 = static_cast<const cv::Mat*>(map1);
        const cv::Mat* m2 = static_cast<const cv::Mat*>(map2);
        cv::remap(src->mat, dst->mat, *m1, *m2, cv::INTER_LINEAR);
    }
}

// Window and GUI
void cv_wrapper_named_window(const char* name) {
    cv::namedWindow(name, cv::WINDOW_NORMAL);
}

void cv_wrapper_imshow(const char* name, const CvImage* img) {
    if (img) {
        cv::imshow(name, img->mat);
    }
}

int cv_wrapper_wait_key(int delay) {
    return cv::waitKey(delay);
}

void cv_wrapper_destroy_all_windows(void) {
    cv::destroyAllWindows();
}

// Mouse Callback Wrapper
void cv_wrapper_set_mouse_callback(const char* name, CvMouseCallback callback, void* userdata) {
    struct CallbackThunk {
        CvMouseCallback cb;
        void* ud;
    };
    
    // We can use a static thunk or register it via lambda.
    // Note: Since this callback registers per window, we can capture userdata
    auto cv_cb = [](int event, int x, int y, int flags, void* param) {
        CallbackThunk* t = static_cast<CallbackThunk*>(param);
        if (t && t->cb) {
            t->cb(event, x, y, flags, t->ud);
        }
    };
    
    // Leak the thunk slightly for simplicity of callback registry life-time
    CallbackThunk* thunk = new CallbackThunk{callback, userdata};
    cv::setMouseCallback(name, cv_cb, thunk);
}

// VideoCapture
CvCapture* cv_wrapper_capture_open(int index) {
    CvCapture* cap = new CvCapture();
    cap->cap.open(index);
    if (!cap->cap.isOpened()) {
        delete cap;
        return NULL;
    }
    return cap;
}

void cv_wrapper_capture_close(CvCapture* cap) {
    if (cap) {
        cap->cap.release();
        delete cap;
    }
}

int cv_wrapper_capture_read(CvCapture* cap, CvImage* img) {
    if (cap && img) {
        return cap->cap.read(img->mat) ? 1 : 0;
    }
    return 0;
}

void cv_wrapper_capture_set_size(CvCapture* cap, int width, int height) {
    if (cap) {
        cap->cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
        cap->cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    }
}
