#include "CameraCalibration.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::vector<double> parse_double_array(const std::string& str) {
    std::vector<double> arr;
    size_t start = str.find('[');
    if (start == std::string::npos) start = 0;
    else start++;
    
    size_t end = str.find(']');
    if (end == std::string::npos) end = str.length();
    
    std::string data_str = str.substr(start, end - start);
    std::replace(data_str.begin(), data_str.end(), ',', ' ');
    std::stringstream ss(data_str);
    double val;
    while (ss >> val) {
        arr.push_back(val);
    }
    return arr;
}

CameraCalibration::CameraCalibration(const std::string& yaml_path)
    : sensor_width(0), sensor_height(0) {
    
    std::ifstream infile(yaml_path.c_str());
    if (!infile.is_open()) {
        throw std::runtime_error("Error: Cannot open calibration file '" + yaml_path + "'");
    }
    
    std::string line;
    std::string current_section = "";
    std::vector<double> k_data;
    std::vector<double> d_data;
    
    while (std::getline(infile, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        
        if (trimmed.find("camera_matrix:") == 0) {
            current_section = "camera_matrix";
            continue;
        } else if (trimmed.find("distortion_coefficients:") == 0) {
            current_section = "distortion_coefficients";
            continue;
        }
        
        size_t colon = trimmed.find(':');
        if (colon == std::string::npos) continue;
        
        std::string key = trim(trimmed.substr(0, colon));
        std::string val = trim(trimmed.substr(colon + 1));
        
        if (key == "sensor_width") {
            sensor_width = std::atoi(val.c_str());
        } else if (key == "sensor_height") {
            sensor_height = std::atoi(val.c_str());
        } else if (key == "camera_model") {
            if (!val.empty() && (val.front() == '"' || val.front() == '\'')) val.erase(0, 1);
            if (!val.empty() && (val.back() == '"' || val.back() == '\'')) val.pop_back();
            camera_model = val;
        } else if (key == "data") {
            if (current_section == "camera_matrix") {
                k_data = parse_double_array(val);
            } else if (current_section == "distortion_coefficients") {
                d_data = parse_double_array(val);
            }
        }
    }
    infile.close();
    
    // Construct OpenCV matrices
    if (k_data.size() == 9) {
        K = cv::Mat(3, 3, CV_64F);
        std::copy(k_data.begin(), k_data.end(), K.ptr<double>());
    } else {
        throw std::runtime_error("Error: Camera matrix in calibration file must have exactly 9 elements.");
    }
    
    D = cv::Mat(d_data.size(), 1, CV_64F);
    std::copy(d_data.begin(), d_data.end(), D.ptr<double>());
}

CameraCalibration::~CameraCalibration() {
    // OpenCV Mat handles its own memory deallocation, so no explicit deletes are required here.
}

bool CameraCalibration::pixel_to_angle(double u, double v, double& yaw, double& pitch) const {
    double fx = K.at<double>(0, 0);
    double fy = K.at<double>(1, 1);
    double cx = K.at<double>(0, 2);
    double cy = K.at<double>(1, 2);
    
    if (camera_model == "pinhole") {
        double x_double_prime = (u - cx) / fx;
        double y_double_prime = (v - cy) / fy;
        
        double k1 = D.at<double>(0);
        double k2 = D.at<double>(1);
        double p1 = D.at<double>(2);
        double p2 = D.at<double>(3);
        double k3 = (D.total() > 4) ? D.at<double>(4) : 0.0;
        
        double x = x_double_prime;
        double y = y_double_prime;
        
        for (int iter = 0; iter < 10; iter++) {
            double r2 = x * x + y * y;
            double r4 = r2 * r2;
            double r6 = r4 * r2;
            double cdist = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;
            
            double delta_x = 2.0 * p1 * x * y + p2 * (r2 + 2.0 * x * x);
            double delta_y = p1 * (r2 + 2.0 * y * y) + 2.0 * p2 * x * y;
            
            x = (x_double_prime - delta_x) / cdist;
            y = (y_double_prime - delta_y) / cdist;
        }
        
        yaw = std::atan(x);
        pitch = std::atan2(-y, std::sqrt(x * x + 1.0));
    } else {
        double x_double_prime = (u - cx) / fx;
        double y_double_prime = (v - cy) / fy;
        
        double theta_d = std::sqrt(x_double_prime * x_double_prime + y_double_prime * y_double_prime);
        double theta = theta_d;
        
        double k1 = D.at<double>(0);
        double k2 = D.at<double>(1);
        double k3 = D.at<double>(2);
        double k4 = D.at<double>(3);
        
        for (int iter = 0; iter < 10; iter++) {
            double th2 = theta * theta;
            double th4 = th2 * th2;
            double th6 = th4 * th2;
            double th8 = th4 * th4;
            
            double f = theta * (1.0 + k1 * th2 + k2 * th4 + k3 * th6 + k4 * th8) - theta_d;
            double df = 1.0 + 3.0 * k1 * th2 + 5.0 * k2 * th4 + 7.0 * k3 * th6 + 9.0 * k4 * th8;
            
            if (std::abs(df) < 1e-12) break;
            theta = theta - f / df;
        }
        
        double scale = 1.0;
        if (theta_d > 1e-8) {
            scale = std::tan(theta) / theta_d;
        }
        
        double x = x_double_prime * scale;
        double y = y_double_prime * scale;
        
        yaw = std::atan(x);
        pitch = std::atan2(-y, std::sqrt(x * x + 1.0));
    }
    return true;
}

bool CameraCalibration::angle_to_pixel(double yaw, double pitch, double& u, double& v) const {
    double fx = K.at<double>(0, 0);
    double fy = K.at<double>(1, 1);
    double cx = K.at<double>(0, 2);
    double cy = K.at<double>(1, 2);
    
    double x_prime = std::tan(yaw);
    double cos_yaw = std::cos(yaw);
    if (std::abs(cos_yaw) < 1e-7) {
        cos_yaw = (cos_yaw >= 0.0) ? 1e-7 : -1e-7;
    }
    double y_prime = -std::tan(pitch) / cos_yaw;
    
    if (camera_model == "pinhole") {
        double r2 = x_prime * x_prime + y_prime * y_prime;
        double r4 = r2 * r2;
        double r6 = r4 * r2;
        
        double k1 = D.at<double>(0);
        double k2 = D.at<double>(1);
        double p1 = D.at<double>(2);
        double p2 = D.at<double>(3);
        double k3 = (D.total() > 4) ? D.at<double>(4) : 0.0;
        
        double cdist = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;
        double delta_x = 2.0 * p1 * x_prime * y_prime + p2 * (r2 + 2.0 * x_prime * x_prime);
        double delta_y = p1 * (r2 + 2.0 * y_prime * y_prime) + 2.0 * p2 * x_prime * y_prime;
        
        double x_dist = x_prime * cdist + delta_x;
        double y_dist = y_prime * cdist + delta_y;
        
        u = fx * x_dist + cx;
        v = fy * y_dist + cy;
    } else {
        double r = std::sqrt(x_prime * x_prime + y_prime * y_prime);
        double theta = std::atan(r);
        double theta_d = theta;
        
        if (r > 1e-8) {
            double k1 = D.at<double>(0);
            double k2 = D.at<double>(1);
            double k3 = D.at<double>(2);
            double k4 = D.at<double>(3);
            
            double th2 = theta * theta;
            double th4 = th2 * th2;
            double th6 = th4 * th2;
            double th8 = th4 * th4;
            
            theta_d = theta * (1.0 + k1 * th2 + k2 * th4 + k3 * th6 + k4 * th8);
        }
        
        double x_dist = x_prime;
        double y_dist = y_prime;
        if (r > 1e-8) {
            x_dist = (theta_d / r) * x_prime;
            y_dist = (theta_d / r) * y_prime;
        }
        
        u = fx * x_dist + cx;
        v = fy * y_dist + cy;
    }
    return true;
}

void CameraCalibration::init_rectification_maps() {
    if (camera_model == "pinhole") {
        cv::initUndistortRectifyMap(K, D, cv::Mat(), K, cv::Size(sensor_width, sensor_height), CV_16SC2, map1, map2);
    } else {
        cv::Mat D_fe = D.clone();
        if (D_fe.total() > 4) {
            D_fe = D_fe.rowRange(0, 4);
        }
        cv::fisheye::initUndistortRectifyMap(K, D_fe, cv::Mat(), K, cv::Size(sensor_width, sensor_height), CV_16SC2, map1, map2);
    }
}

void CameraCalibration::undistort(const cv::Mat& src, cv::Mat& dst) {
    if (map1.empty() || map2.empty()) {
        init_rectification_maps();
    }
    cv::remap(src, dst, map1, map2, cv::INTER_LINEAR);
}
