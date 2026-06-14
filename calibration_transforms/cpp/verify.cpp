#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <opencv2/opencv.hpp>
#include "CameraCalibration.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global variables for mouse tracking
static int mouse_u = 0;
static int mouse_v = 0;
static std::string mouse_side = "left"; // "left" or "right"

struct MouseParams {
    int width;
    int height;
};

static void on_mouse(int event, int x, int y, int flags, void* userdata) {
    (void)event; (void)flags;
    MouseParams* param = static_cast<MouseParams*>(userdata);
    int W = param->width;
    
    if (x < W) {
        mouse_u = (x < 0) ? 0 : (x >= W ? W - 1 : x);
        mouse_v = (y < 0) ? 0 : (y >= param->height ? param->height - 1 : y);
        mouse_side = "left";
    } else {
        mouse_u = (x - W < 0) ? 0 : (x - W >= W ? W - 1 : x - W);
        mouse_v = (y < 0) ? 0 : (y >= param->height ? param->height - 1 : y);
        mouse_side = "right";
    }
}

static bool is_on_screen(double u, double v, int W, int H) {
    return (u >= -50.0 && u < W + 50.0 && v >= -50.0 && v < H + 50.0);
}

static double deg2rad(double deg) {
    return deg * M_PI / 180.0;
}

static double rad2deg(double rad) {
    return rad * 180.0 / M_PI;
}

static void get_undistorted_pixel(const CameraCalibration& model, double yaw, double pitch, double& u, double& v) {
    double x_prime = std::tan(yaw);
    double cos_yaw = std::cos(yaw);
    if (std::abs(cos_yaw) < 1e-7) {
        cos_yaw = (cos_yaw >= 0.0) ? 1e-7 : -1e-7;
    }
    double y_prime = -std::tan(pitch) / cos_yaw;
    
    cv::Mat K = model.get_K();
    u = K.at<double>(0, 0) * x_prime + K.at<double>(0, 2);
    v = K.at<double>(1, 1) * y_prime + K.at<double>(1, 2);
}

static void draw_angle_grid(cv::Mat& img, const CameraCalibration& model, bool use_distortion, const cv::Scalar& color) {
    int H = img.rows;
    int W = img.cols;
    
    // Grid lines every 10 degrees
    std::vector<double> yaws = {-60, -50, -40, -30, -20, -10, 0, 10, 20, 30, 40, 50, 60};
    std::vector<double> pitches = {-40, -30, -20, -10, 0, 10, 20, 30, 40};
    
    // 1. Constant Yaw lines
    for (double y_deg : yaws) {
        double y_rad = deg2rad(y_deg);
        bool has_prev = false;
        double prev_u = 0, prev_v = 0;
        
        for (int p_deg = -45; p_deg <= 45; p_deg += 2) {
            double p_rad = deg2rad(p_deg);
            double u, v;
            if (use_distortion) {
                model.angle_to_pixel(y_rad, p_rad, u, v);
            } else {
                get_undistorted_pixel(model, y_rad, p_rad, u, v);
            }
            
            if (std::isnan(u) || std::isnan(v) || std::abs(u) > 5.0 * W || std::abs(v) > 5.0 * H) {
                has_prev = false;
                continue;
            }
            
            if (has_prev) {
                if (is_on_screen(prev_u, prev_v, W, H) || is_on_screen(u, v, W, H)) {
                    cv::line(img, cv::Point((int)prev_u, (int)prev_v), cv::Point((int)u, (int)v), color, 1, cv::LINE_AA);
                }
            }
            prev_u = u;
            prev_v = v;
            has_prev = true;
        }
        
        // Draw Yaw Label at Pitch = 0 (middle of the line)
        double lbl_u, lbl_v;
        if (use_distortion) {
            model.angle_to_pixel(y_rad, 0.0, lbl_u, lbl_v);
        } else {
            get_undistorted_pixel(model, y_rad, 0.0, lbl_u, lbl_v);
        }
        if (lbl_u >= 10 && lbl_u < W - 10 && lbl_v >= 10 && lbl_v < H - 10) {
            std::string label = std::to_string((int)y_deg);
            cv::putText(img, label, cv::Point((int)lbl_u + 3, (int)lbl_v - 3), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
            cv::putText(img, label, cv::Point((int)lbl_u + 3, (int)lbl_v - 3), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        }
    }
    
    // 2. Constant Pitch lines
    for (double p_deg : pitches) {
        double p_rad = deg2rad(p_deg);
        bool has_prev = false;
        double prev_u = 0, prev_v = 0;
        
        for (int y_deg = -65; y_deg <= 65; y_deg += 2) {
            double y_rad = deg2rad(y_deg);
            double u, v;
            if (use_distortion) {
                model.angle_to_pixel(y_rad, p_rad, u, v);
            } else {
                get_undistorted_pixel(model, y_rad, p_rad, u, v);
            }
            
            if (std::isnan(u) || std::isnan(v) || std::abs(u) > 5.0 * W || std::abs(v) > 5.0 * H) {
                has_prev = false;
                continue;
            }
            
            if (has_prev) {
                if (is_on_screen(prev_u, prev_v, W, H) || is_on_screen(u, v, W, H)) {
                    cv::line(img, cv::Point((int)prev_u, (int)prev_v), cv::Point((int)u, (int)v), color, 1, cv::LINE_AA);
                }
            }
            prev_u = u;
            prev_v = v;
            has_prev = true;
        }
        
        // Draw Pitch Label at Yaw = 0 (middle of the line)
        double lbl_u, lbl_v;
        if (use_distortion) {
            model.angle_to_pixel(0.0, p_rad, lbl_u, lbl_v);
        } else {
            get_undistorted_pixel(model, 0.0, p_rad, lbl_u, lbl_v);
        }
        if (lbl_u >= 10 && lbl_u < W - 10 && lbl_v >= 10 && lbl_v < H - 10) {
            std::string label = std::to_string((int)p_deg);
            cv::putText(img, label, cv::Point((int)lbl_u + 3, (int)lbl_v - 3), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
            cv::putText(img, label, cv::Point((int)lbl_u + 3, (int)lbl_v - 3), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        }
    }
}

static int select_camera(void) {
    std::cout << "Scanning for available camera devices..." << std::endl;
    std::vector<int> available;
    for (int i = 0; i < 8; i++) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            available.push_back(i);
            cap.release();
        }
    }
    
    if (available.empty()) {
        std::cerr << "Error: No available camera devices found." << std::endl;
        return -1;
    }
    
    if (available.size() == 1) {
        std::cout << "Found 1 camera: Index " << available[0] << ". Using it automatically." << std::endl;
        return available[0];
    }
    
    std::cout << "\nAvailable camera devices:" << std::endl;
    for (size_t i = 0; i < available.size(); i++) {
        std::cout << "  [" << i << "] Camera Index " << available[i] << std::endl;
    }
    
    std::cout << "Select a camera [0-" << available.size() - 1 << "] (default 0): ";
    std::string line;
    if (std::getline(std::cin, line)) {
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return available[0];
        }
        int choice = std::stoi(line.substr(first));
        if (choice >= 0 && choice < (int)available.size()) {
            return available[choice];
        }
    }
    return available[0];
}

int main(int argc, char* argv[]) {
    std::string calibration_path = "camera_calibration.yaml";
    int camera_index = -1;
    std::string image_path = "";
    
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--calibration" && i + 1 < argc) {
            calibration_path = argv[++i];
        } else if (std::string(argv[i]) == "--camera" && i + 1 < argc) {
            camera_index = std::atoi(argv[++i]);
        } else if (std::string(argv[i]) == "--image" && i + 1 < argc) {
            image_path = argv[++i];
        } else if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --calibration <path>  Path to calibration YAML (default: camera_calibration.yaml)" << std::endl;
            std::cout << "  --camera <index>      Webcam index" << std::endl;
            std::cout << "  --image <path>        Path to a single image" << std::endl;
            return 0;
        }
    }
    
    std::cout << "Loading camera calibration model: " << calibration_path << std::endl;
    CameraCalibration* model = nullptr;
    try {
        model = new CameraCalibration(calibration_path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    
    int W = model->get_sensor_width();
    int H = model->get_sensor_height();
    std::cout << "Camera Model: " << model->get_camera_model() << std::endl;
    std::cout << "Calibrated Resolution: " << W << "x" << H << std::endl;
    
    cv::Mat frame;
    cv::VideoCapture cap;
    bool is_static = false;
    
    if (!image_path.empty()) {
        frame = cv::imread(image_path);
        if (frame.empty()) {
            std::cerr << "Error: Could not read image '" << image_path << "'" << std::endl;
            delete model;
            return 1;
        }
        is_static = true;
    } else {
        if (camera_index < 0) {
            camera_index = select_camera();
            if (camera_index < 0) {
                delete model;
                return 1;
            }
        }
        cap.open(camera_index);
        if (!cap.isOpened()) {
            std::cerr << "Error: Could not open camera with index " << camera_index << std::endl;
            delete model;
            return 1;
        }
        cap.set(cv::CAP_PROP_FRAME_WIDTH, W);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, H);
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "Error: Could not read frame from camera." << std::endl;
            delete model;
            return 1;
        }
    }
    
    int img_w = frame.cols;
    int img_h = frame.rows;
    
    std::string window_name = "Calibration Verification (C++)";
    cv::namedWindow(window_name, cv::WINDOW_NORMAL);
    
    MouseParams m_params = { img_w, img_h };
    cv::setMouseCallback(window_name, on_mouse, &m_params);
    
    cv::Mat undistorted_frame;
    cv::Mat display_dist;
    cv::Mat display_undist;
    cv::Mat combined;
    
    bool show_grid = true;
    
    std::cout << "\nInstructions:" << std::endl;
    std::cout << "  - Move mouse cursor over the images to see real-time Yaw/Pitch values." << std::endl;
    std::cout << "  - Crosshair on the other image displays the mapped position." << std::endl;
    std::cout << "  - Press 'g' to toggle the angle grid overlay." << std::endl;
    std::cout << "  - Press 'q' or ESC to exit." << std::endl;
    
    while (true) {
        if (!is_static) {
            cap >> frame;
            if (frame.empty()) break;
        }
        
        // Handle resizing if it doesn't match the model
        if (frame.cols != img_w || frame.rows != img_h) {
            cv::resize(frame, frame, cv::Size(img_w, img_h));
        }
        
        // 1. Undistort the image
        model->undistort(frame, undistorted_frame);
        
        // Copy to display matrices
        display_dist = frame.clone();
        display_undist = undistorted_frame.clone();
        
        // 2. Draw Grid
        if (show_grid) {
            draw_angle_grid(display_dist, *model, true, cv::Scalar(0, 200, 200)); // cyan-yellow
            draw_angle_grid(display_undist, *model, false, cv::Scalar(200, 200, 0)); // yellow-blue
        }
        
        // 3. Coordinate conversion
        double yaw_rad = 0.0, pitch_rad = 0.0;
        double u_dist = 0.0, v_dist = 0.0;
        double u_undist = 0.0, v_undist = 0.0;
        
        if (mouse_side == "left") {
            model->pixel_to_angle(mouse_u, mouse_v, yaw_rad, pitch_rad);
            get_undistorted_pixel(*model, yaw_rad, pitch_rad, u_undist, v_undist);
            u_dist = mouse_u;
            v_dist = mouse_v;
        } else {
            cv::Mat K = model->get_K();
            double x_prime = (mouse_u - K.at<double>(0, 2)) / K.at<double>(0, 0);
            double y_prime = (mouse_v - K.at<double>(1, 2)) / K.at<double>(1, 1);
            yaw_rad = std::atan(x_prime);
            pitch_rad = std::atan2(-y_prime, std::sqrt(x_prime * x_prime + 1.0));
            
            model->angle_to_pixel(yaw_rad, pitch_rad, u_dist, v_dist);
            u_undist = mouse_u;
            v_undist = mouse_v;
        }
        
        // Draw crosshair markers
        if (is_on_screen(u_dist, v_dist, img_w, img_h)) {
            cv::drawMarker(display_dist, cv::Point((int)u_dist, (int)v_dist), cv::Scalar(0, 165, 255), cv::MARKER_CROSS, 15, 2);
            cv::circle(display_dist, cv::Point((int)u_dist, (int)v_dist), 4, cv::Scalar(0, 165, 255), 1);
        }
        
        if (is_on_screen(u_undist, v_undist, img_w, img_h)) {
            cv::drawMarker(display_undist, cv::Point((int)u_undist, (int)v_undist), cv::Scalar(0, 165, 255), cv::MARKER_CROSS, 15, 2);
            cv::circle(display_undist, cv::Point((int)u_undist, (int)v_undist), 4, cv::Scalar(0, 165, 255), 1);
        }
        
        // Stack side by side
        cv::hconcat(display_dist, display_undist, combined);
        
        // Tooltip box calculation
        int cursor_x = (mouse_side == "left") ? mouse_u : mouse_u + img_w;
        int cursor_y = mouse_v;
        int box_w = 220;
        int box_h = 75;
        int tx = cursor_x + 15;
        int ty = cursor_y + 15;
        
        if (tx + box_w > 2 * img_w) {
            tx = cursor_x - box_w - 15;
        }
        if (ty + box_h > img_h) {
            ty = cursor_y - box_h - 15;
        }
        
        // Draw tooltip background
        cv::Mat overlay = combined.clone();
        cv::rectangle(overlay, cv::Point(tx, ty), cv::Point(tx + box_w, ty + box_h), cv::Scalar(30, 30, 30), -1);
        cv::addWeighted(overlay, 0.75, combined, 0.25, 0.0, combined);
        
        cv::rectangle(combined, cv::Point(tx, ty), cv::Point(tx + box_w, ty + box_h), cv::Scalar(150, 150, 150), 1);
        
        double yaw_deg = rad2deg(yaw_rad);
        double pitch_deg = rad2deg(pitch_rad);
        
        std::string text;
        text = "Pixel (" + (mouse_side == "left" ? std::string("DIST") : std::string("RECT")) + "): " + std::to_string(mouse_u) + ", " + std::to_string(mouse_v);
        cv::putText(combined, text, cv::Point(tx + 8, ty + 15), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        
        char buf[128];
        std::sprintf(buf, "Yaw:   %+.2f deg (%+.4f rad)", yaw_deg, yaw_rad);
        cv::putText(combined, buf, cv::Point(tx + 8, ty + 31), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(100, 255, 100), 1, cv::LINE_AA);
        
        std::sprintf(buf, "Pitch: %+.2f deg (%+.4f rad)", pitch_deg, pitch_rad);
        cv::putText(combined, buf, cv::Point(tx + 8, ty + 47), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(100, 255, 100), 1, cv::LINE_AA);
        
        text = "Calib Model: " + model->get_camera_model();
        cv::putText(combined, text, cv::Point(tx + 8, ty + 63), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(200, 200, 200), 1, cv::LINE_AA);
        
        // Add Screen labels
        cv::putText(combined, "ORIGINAL (DISTORTED)", cv::Point(15, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 3, cv::LINE_AA);
        cv::putText(combined, "ORIGINAL (DISTORTED)", cv::Point(15, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        
        cv::putText(combined, "RECTIFIED (UNDISTORTED)", cv::Point(img_w + 15, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 3, cv::LINE_AA);
        cv::putText(combined, "RECTIFIED (UNDISTORTED)", cv::Point(img_w + 15, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        
        cv::imshow(window_name, combined);
        
        int key = cv::waitKey(30) & 0xFF;
        if (key == 27 || key == 'q') {
            break;
        } else if (key == 'g') {
            show_grid = !show_grid;
            std::cout << "Grid toggled: " << (show_grid ? "ON" : "OFF") << std::endl;
        }
    }
    
    delete model;
    cv::destroyAllWindows();
    return 0;
}
