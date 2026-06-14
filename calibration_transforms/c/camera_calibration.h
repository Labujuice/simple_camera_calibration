#ifndef CAMERA_CALIBRATION_H
#define CAMERA_CALIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CameraModel CameraModel;
typedef struct CvImage CvImage;

/**
 * @brief Create a camera model instance by parsing a calibration YAML file.
 *        Allocates memory on the heap.
 * @param yaml_path Path to the calibration YAML file.
 * @return Pointer to the created CameraModel, or NULL on failure.
 */
CameraModel* camera_model_create(const char* yaml_path);

/**
 * @brief Destroy the camera model instance and free all associated memory.
 * @param model Pointer to the CameraModel to destroy.
 */
void camera_model_destroy(CameraModel* model);

/**
 * @brief Convert pixel coordinate (u, v) to physical angles (yaw, pitch) in radians.
 *        Yaw: positive to the right, negative to the left.
 *        Pitch: positive upwards, negative downwards.
 * @param model Pointer to the CameraModel.
 * @param u Pixel x-coordinate.
 * @param v Pixel y-coordinate.
 * @param yaw Output pointer for yaw in radians.
 * @param pitch Output pointer for pitch in radians.
 * @return 0 on success, non-zero on failure.
 */
int camera_model_pixel_to_angle(const CameraModel* model, double u, double v, double* yaw, double* pitch);

/**
 * @brief Convert physical angles (yaw, pitch) in radians to pixel coordinate (u, v).
 * @param model Pointer to the CameraModel.
 * @param yaw Yaw in radians.
 * @param pitch Pitch in radians.
 * @param u Output pointer for pixel x-coordinate.
 * @param v Output pointer for pixel y-coordinate.
 * @return 0 on success, non-zero on failure.
 */
int camera_model_angle_to_pixel(const CameraModel* model, double yaw, double pitch, double* u, double* v);

/**
 * @brief Apply fast remapping to undistort the input image.
 * @param model Pointer to the CameraModel.
 * @param src Opaque pointer to the source CvImage.
 * @param dst Opaque pointer to the destination CvImage.
 * @return 0 on success, non-zero on failure.
 */
int camera_model_undistort(CameraModel* model, const CvImage* src, CvImage* dst);

/* Getters */
int camera_model_get_width(const CameraModel* model);
int camera_model_get_height(const CameraModel* model);
const char* camera_model_get_type(const CameraModel* model);
void camera_model_get_K(const CameraModel* model, double K_out[9]);
void camera_model_get_D(const CameraModel* model, double D_out[8], int* D_len_out);

#ifdef __cplusplus
}
#endif

#endif // CAMERA_CALIBRATION_H
