#ifndef CAMERA_CALIBRATION_HPP
#define CAMERA_CALIBRATION_HPP

#include <string>
#include <opencv2/opencv.hpp>

class CameraCalibration {
public:
    /**
     * @brief Constructor that loads camera model parameters from a YAML file.
     * @param yaml_path Path to the calibration YAML file.
     */
    CameraCalibration(const std::string& yaml_path);

    /**
     * @brief Destructor. Cleans up internal structures.
     */
    ~CameraCalibration();

    /**
     * @brief Convert pixel coordinate (u, v) to physical angles (yaw, pitch) in radians.
     * @param u Pixel x-coordinate.
     * @param v Pixel y-coordinate.
     * @param yaw Output reference for yaw in radians.
     * @param pitch Output reference for pitch in radians.
     * @return True on success, false on failure.
     */
    bool pixel_to_angle(double u, double v, double& yaw, double& pitch) const;

    /**
     * @brief Convert physical angles (yaw, pitch) in radians to pixel coordinate (u, v).
     * @param yaw Yaw in radians.
     * @param pitch Pitch in radians.
     * @param u Output reference for pixel x-coordinate.
     * @param v Output reference for pixel y-coordinate.
     * @return True on success, false on failure.
     */
    bool angle_to_pixel(double yaw, double pitch, double& u, double& v) const;

    /**
     * @brief Apply fast remapping to undistort the source image.
     * @param src Input distorted image.
     * @param dst Output undistorted image.
     */
    void undistort(const cv::Mat& src, cv::Mat& dst);

    /* Getters */
    int get_sensor_width() const { return sensor_width; }
    int get_sensor_height() const { return sensor_height; }
    std::string get_camera_model() const { return camera_model; }
    cv::Mat get_K() const { return K; }
    cv::Mat get_D() const { return D; }

private:
    int sensor_width;
    int sensor_height;
    std::string camera_model; // "pinhole" or "fisheye"
    cv::Mat K;                // 3x3 Camera matrix
    cv::Mat D;                // Distortion coefficients Mat
    cv::Mat map1;             // Fast remapping x-map
    cv::Mat map2;             // Fast remapping y-map

    void init_rectification_maps();
};

#endif // CAMERA_CALIBRATION_HPP
