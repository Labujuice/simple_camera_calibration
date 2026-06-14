#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "camera_calibration.h"
#include "cv_wrapper.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global variables for mouse tracking
static int mouse_u = 0;
static int mouse_v = 0;
static const char* mouse_side = "left"; // "left" or "right"

typedef struct {
    int width;
    int height;
} MouseParams;

static void on_mouse(int event, int x, int y, int flags, void* userdata) {
    (void)event; (void)flags;
    MouseParams* param = (MouseParams*)userdata;
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

static int is_on_screen(double u, double v, int W, int H) {
    return (u >= -50.0 && u < W + 50.0 && v >= -50.0 && v < H + 50.0);
}

static double deg2rad(double deg) {
    return deg * M_PI / 180.0;
}

static double rad2deg(double rad) {
    return rad * 180.0 / M_PI;
}

static void get_undistorted_pixel(const CameraModel* model, double yaw, double pitch, double* u, double* v) {
    double x_prime = tan(yaw);
    double cos_yaw = cos(yaw);
    if (fabs(cos_yaw) < 1e-7) {
        cos_yaw = (cos_yaw >= 0.0) ? 1e-7 : -1e-7;
    }
    double y_prime = -tan(pitch) / cos_yaw;
    
    double K[9];
    camera_model_get_K(model, K);
    *u = K[0] * x_prime + K[2];
    *v = K[4] * y_prime + K[5];
}

static void draw_angle_grid(CvImage* img, const CameraModel* model, int use_distortion, int r, int g, int b) {
    int H = cv_wrapper_image_height(img);
    int W = cv_wrapper_image_width(img);
    
    // Grid lines every 10 degrees
    double yaws[] = {-60, -50, -40, -30, -20, -10, 0, 10, 20, 30, 40, 50, 60};
    double pitches[] = {-40, -30, -20, -10, 0, 10, 20, 30, 40};
    
    // 1. Constant Yaw lines
    for (size_t y_idx = 0; y_idx < sizeof(yaws)/sizeof(yaws[0]); y_idx++) {
        double y_rad = deg2rad(yaws[y_idx]);
        int has_prev = 0;
        double prev_u = 0, prev_v = 0;
        
        for (int p_deg = -45; p_deg <= 45; p_deg += 2) {
            double p_rad = deg2rad(p_deg);
            double u, v;
            if (use_distortion) {
                camera_model_angle_to_pixel(model, y_rad, p_rad, &u, &v);
            } else {
                get_undistorted_pixel(model, y_rad, p_rad, &u, &v);
            }
            
            if (isnan(u) || isnan(v) || fabs(u) > 5.0 * W || fabs(v) > 5.0 * H) {
                has_prev = 0;
                continue;
            }
            
            if (has_prev) {
                if (is_on_screen(prev_u, prev_v, W, H) || is_on_screen(u, v, W, H)) {
                    cv_wrapper_line(img, (int)prev_u, (int)prev_v, (int)u, (int)v, r, g, b, 1);
                }
            }
            prev_u = u;
            prev_v = v;
            has_prev = 1;
        }
        
        // Draw Yaw Label at Pitch = 0 (middle of the line)
        double lbl_u, lbl_v;
        if (use_distortion) {
            camera_model_angle_to_pixel(model, y_rad, 0.0, &lbl_u, &lbl_v);
        } else {
            get_undistorted_pixel(model, y_rad, 0.0, &lbl_u, &lbl_v);
        }
        if (lbl_u >= 10 && lbl_u < W - 10 && lbl_v >= 10 && lbl_v < H - 10) {
            char label[16];
            sprintf(label, "%.0f", yaws[y_idx]);
            cv_wrapper_put_text(img, label, (int)(lbl_u + 3), (int)(lbl_v - 3), 0.35, 0, 0, 0, 2);
            cv_wrapper_put_text(img, label, (int)(lbl_u + 3), (int)(lbl_v - 3), 0.35, 255, 255, 255, 1);
        }
    }
    
    // 2. Constant Pitch lines
    for (size_t p_idx = 0; p_idx < sizeof(pitches)/sizeof(pitches[0]); p_idx++) {
        double p_rad = deg2rad(pitches[p_idx]);
        int has_prev = 0;
        double prev_u = 0, prev_v = 0;
        
        for (int y_deg = -65; y_deg <= 65; y_deg += 2) {
            double y_rad = deg2rad(y_deg);
            double u, v;
            if (use_distortion) {
                camera_model_angle_to_pixel(model, y_rad, p_rad, &u, &v);
            } else {
                get_undistorted_pixel(model, y_rad, p_rad, &u, &v);
            }
            
            if (isnan(u) || isnan(v) || fabs(u) > 5.0 * W || fabs(v) > 5.0 * H) {
                has_prev = 0;
                continue;
            }
            
            if (has_prev) {
                if (is_on_screen(prev_u, prev_v, W, H) || is_on_screen(u, v, W, H)) {
                    cv_wrapper_line(img, (int)prev_u, (int)prev_v, (int)u, (int)v, r, g, b, 1);
                }
            }
            prev_u = u;
            prev_v = v;
            has_prev = 1;
        }
        
        // Draw Pitch Label at Yaw = 0 (middle of the line)
        double lbl_u, lbl_v;
        if (use_distortion) {
            camera_model_angle_to_pixel(model, 0.0, p_rad, &lbl_u, &lbl_v);
        } else {
            get_undistorted_pixel(model, 0.0, p_rad, &lbl_u, &lbl_v);
        }
        if (lbl_u >= 10 && lbl_u < W - 10 && lbl_v >= 10 && lbl_v < H - 10) {
            char label[16];
            sprintf(label, "%.0f", pitches[p_idx]);
            cv_wrapper_put_text(img, label, (int)(lbl_u + 3), (int)(lbl_v - 3), 0.35, 0, 0, 0, 2);
            cv_wrapper_put_text(img, label, (int)(lbl_u + 3), (int)(lbl_v - 3), 0.35, 255, 255, 255, 1);
        }
    }
}

static int select_camera(void) {
    printf("Scanning for available camera devices...\n");
    int available[8];
    int count = 0;
    for (int i = 0; i < 8; i++) {
        CvCapture* cap = cv_wrapper_capture_open(i);
        if (cap) {
            available[count++] = i;
            cv_wrapper_capture_close(cap);
        }
    }
    
    if (count == 0) {
        printf("Error: No available camera devices found.\n");
        return -1;
    }
    
    if (count == 1) {
        printf("Found 1 camera: Index %d. Using it automatically.\n", available[0]);
        return available[0];
    }
    
    printf("\nAvailable camera devices:\n");
    for (int i = 0; i < count; i++) {
        printf("  [%d] Camera Index %d\n", i, available[i]);
    }
    
    printf("Select a camera [0-%d] (default 0): ", count - 1);
    char buf[32];
    if (fgets(buf, sizeof(buf), stdin)) {
        // Trim whitespace
        char* trimmed = buf;
        while (isspace((unsigned char)*trimmed)) trimmed++;
        char* end = trimmed + strlen(trimmed) - 1;
        while (end > trimmed && isspace((unsigned char)*end)) end--;
        end[1] = '\0';
        
        if (strlen(trimmed) == 0) {
            return available[0];
        }
        int choice = atoi(trimmed);
        if (choice >= 0 && choice < count) {
            return available[choice];
        }
    }
    return available[0];
}

int main(int argc, char* argv[]) {
    const char* calibration_path = "camera_calibration.yaml";
    int camera_index = -1;
    const char* image_path = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--calibration") == 0 && i + 1 < argc) {
            calibration_path = argv[++i];
        } else if (strcmp(argv[i], "--camera") == 0 && i + 1 < argc) {
            camera_index = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--image") == 0 && i + 1 < argc) {
            image_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --calibration <path>  Path to calibration YAML (default: camera_calibration.yaml)\n");
            printf("  --camera <index>      Webcam index\n");
            printf("  --image <path>        Path to a single image\n");
            return 0;
        }
    }
    
    printf("Loading camera calibration model: %s\n", calibration_path);
    CameraModel* model = camera_model_create(calibration_path);
    if (!model) {
        printf("Error: Could not load calibration file '%s'\n", calibration_path);
        return 1;
    }
    
    int W = camera_model_get_width(model);
    int H = camera_model_get_height(model);
    printf("Camera Model: %s\n", camera_model_get_type(model));
    printf("Calibrated Resolution: %dx%d\n", W, H);
    
    CvImage* frame = NULL;
    CvCapture* cap = NULL;
    int is_static = 0;
    
    if (image_path) {
        frame = cv_wrapper_imread(image_path);
        if (!frame) {
            printf("Error: Could not read image '%s'\n", image_path);
            camera_model_destroy(model);
            return 1;
        }
        is_static = 1;
    } else {
        if (camera_index < 0) {
            camera_index = select_camera();
            if (camera_index < 0) {
                camera_model_destroy(model);
                return 1;
            }
        }
        cap = cv_wrapper_capture_open(camera_index);
        if (!cap) {
            printf("Error: Could not open camera with index %d\n", camera_index);
            camera_model_destroy(model);
            return 1;
        }
        cv_wrapper_capture_set_size(cap, W, H);
        frame = cv_wrapper_image_create_empty();
        if (!cv_wrapper_capture_read(cap, frame)) {
            printf("Error: Could not read frame from camera.\n");
            cv_wrapper_capture_close(cap);
            cv_wrapper_image_free(frame);
            camera_model_destroy(model);
            return 1;
        }
    }
    
    int img_w = cv_wrapper_image_width(frame);
    int img_h = cv_wrapper_image_height(frame);
    
    const char* window_name = "Calibration Verification (C)";
    cv_wrapper_named_window(window_name);
    cv_wrapper_resize_window(window_name, img_w, img_h / 2);
    
    MouseParams m_params = { img_w, img_h };
    cv_wrapper_set_mouse_callback(window_name, on_mouse, &m_params);
    
    CvImage* undistorted_frame = cv_wrapper_image_create(img_w, img_h, 3);
    CvImage* display_dist = cv_wrapper_image_create(img_w, img_h, 3);
    CvImage* display_undist = cv_wrapper_image_create(img_w, img_h, 3);
    CvImage* combined = cv_wrapper_image_create(img_w * 2, img_h, 3);
    
    int show_grid = 1;
    
    printf("\nInstructions:\n");
    printf("  - Move mouse cursor over the images to see real-time Yaw/Pitch values.\n");
    printf("  - Crosshair on the other image displays the mapped position.\n");
    printf("  - Press 'g' to toggle the angle grid overlay.\n");
    printf("  - Press 'q' or ESC to exit.\n");
    
    while (1) {
        if (!is_static) {
            if (!cv_wrapper_capture_read(cap, frame)) {
                break;
            }
        }
        
        // Handle image resizing if it doesn't match the model
        if (cv_wrapper_image_width(frame) != img_w || cv_wrapper_image_height(frame) != img_h) {
            CvImage* temp = cv_wrapper_image_create(img_w, img_h, 3);
            cv_wrapper_image_resize(frame, temp, img_w, img_h);
            cv_wrapper_image_free(frame);
            frame = temp;
        }
        
        // 1. Undistort the image
        camera_model_undistort(model, frame, undistorted_frame);
        
        // Copy to display matrices
        cv_wrapper_image_copy_to(frame, display_dist);
        cv_wrapper_image_copy_to(undistorted_frame, display_undist);
        
        // 2. Draw Grid
        if (show_grid) {
            draw_angle_grid(display_dist, model, 1, 0, 200, 200); // cyan-yellow
            draw_angle_grid(display_undist, model, 0, 200, 200, 0); // yellow-blue
        }
        
        // 3. Coordinate conversion
        double yaw_rad = 0.0, pitch_rad = 0.0;
        double u_dist = 0.0, v_dist = 0.0;
        double u_undist = 0.0, v_undist = 0.0;
        
        if (strcmp(mouse_side, "left") == 0) {
            camera_model_pixel_to_angle(model, mouse_u, mouse_v, &yaw_rad, &pitch_rad);
            get_undistorted_pixel(model, yaw_rad, pitch_rad, &u_undist, &v_undist);
            u_dist = mouse_u;
            v_dist = mouse_v;
        } else {
            double K[9];
            camera_model_get_K(model, K);
            double x_prime = (mouse_u - K[2]) / K[0];
            double y_prime = (mouse_v - K[5]) / K[4];
            yaw_rad = atan(x_prime);
            pitch_rad = atan2(-y_prime, sqrt(x_prime * x_prime + 1.0));
            
            camera_model_angle_to_pixel(model, yaw_rad, pitch_rad, &u_dist, &v_dist);
            u_undist = mouse_u;
            v_undist = mouse_v;
        }
        
        // Draw crosshair markers
        if (is_on_screen(u_dist, v_dist, img_w, img_h)) {
            cv_wrapper_draw_marker(display_dist, (int)u_dist, (int)v_dist, 0, 165, 255, 1, 15, 2); // cv::MARKER_CROSS is 1
            cv_wrapper_circle(display_dist, (int)u_dist, (int)v_dist, 4, 0, 165, 255, 1);
        }
        
        if (is_on_screen(u_undist, v_undist, img_w, img_h)) {
            cv_wrapper_draw_marker(display_undist, (int)u_undist, (int)v_undist, 0, 165, 255, 1, 15, 2);
            cv_wrapper_circle(display_undist, (int)u_undist, (int)v_undist, 4, 0, 165, 255, 1);
        }
        
        // Stack side by side
        cv_wrapper_hstack(display_dist, display_undist, combined);
        
        // Tooltip box calculation
        int cursor_x = (strcmp(mouse_side, "left") == 0) ? mouse_u : mouse_u + img_w;
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
        CvImage* overlay = cv_wrapper_image_clone(combined);
        cv_wrapper_rectangle(overlay, tx, ty, tx + box_w, ty + box_h, 30, 30, 30, -1, 1);
        cv_wrapper_add_weighted(overlay, 0.75, combined, 0.25, 0.0, combined);
        cv_wrapper_image_free(overlay);
        
        cv_wrapper_rectangle(combined, tx, ty, tx + box_w, ty + box_h, 150, 150, 150, 1, 0);
        
        double yaw_deg = rad2deg(yaw_rad);
        double pitch_deg = rad2deg(pitch_rad);
        
        char text_buf[128];
        sprintf(text_buf, "Pixel (%s): %d, %d", (strcmp(mouse_side, "left") == 0) ? "DIST" : "RECT", mouse_u, mouse_v);
        cv_wrapper_put_text(combined, text_buf, tx + 8, ty + 15, 0.35, 255, 255, 255, 1);
        
        sprintf(text_buf, "Yaw:   %+.2f deg (%+.4f rad)", yaw_deg, yaw_rad);
        cv_wrapper_put_text(combined, text_buf, tx + 8, ty + 31, 0.35, 100, 255, 100, 1);
        
        sprintf(text_buf, "Pitch: %+.2f deg (%+.4f rad)", pitch_deg, pitch_rad);
        cv_wrapper_put_text(combined, text_buf, tx + 8, ty + 47, 0.35, 100, 255, 100, 1);
        
        sprintf(text_buf, "Calib Model: %s", camera_model_get_type(model));
        cv_wrapper_put_text(combined, text_buf, tx + 8, ty + 63, 0.35, 200, 200, 200, 1);
        
        // Add Screen labels
        cv_wrapper_put_text(combined, "ORIGINAL (DISTORTED)", 15, 30, 0.6, 0, 0, 0, 3);
        cv_wrapper_put_text(combined, "ORIGINAL (DISTORTED)", 15, 30, 0.6, 255, 255, 255, 1);
        
        cv_wrapper_put_text(combined, "RECTIFIED (UNDISTORTED)", img_w + 15, 30, 0.6, 0, 0, 0, 3);
        cv_wrapper_put_text(combined, "RECTIFIED (UNDISTORTED)", img_w + 15, 30, 0.6, 255, 255, 255, 1);
        
        cv_wrapper_imshow(window_name, combined);
        
        int key = cv_wrapper_wait_key(30) & 0xFF;
        if (key == 27 || key == 'q') {
            break;
        } else if (key == 'g') {
            show_grid = !show_grid;
            printf("Grid toggled: %s\n", show_grid ? "ON" : "OFF");
        }
    }
    
    // Release resources
    if (cap) {
        cv_wrapper_capture_close(cap);
    }
    cv_wrapper_image_free(frame);
    cv_wrapper_image_free(undistorted_frame);
    cv_wrapper_image_free(display_dist);
    cv_wrapper_image_free(display_undist);
    cv_wrapper_image_free(combined);
    
    camera_model_destroy(model);
    cv_wrapper_destroy_all_windows();
    
    return 0;
}
