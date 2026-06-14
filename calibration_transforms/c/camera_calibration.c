#include "camera_calibration.h"
#include "cv_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

struct CameraModel {
    int sensor_width;
    int sensor_height;
    char camera_model[16]; // "pinhole" or "fisheye"
    double K[9];           // 3x3 camera matrix
    double D[8];           // Distortion coefficients (up to 8)
    int D_len;             // Number of distortion coefficients
    
    // Opaque pointers to OpenCV maps for rectification
    void* map1;
    void* map2;
};

// Helper to trim whitespace
static char* trim(char* str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Helper to parse double array from a string like "[1154.21, 0.0, 960.5, ...]"
static int parse_double_array(const char* str, double* arr, int max_len) {
    const char* p = strchr(str, '[');
    if (!p) p = str;
    else p++;
    
    int count = 0;
    while (count < max_len) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (*p == '\0' || *p == ']') break;
        
        char* next_p;
        arr[count] = strtod(p, &next_p);
        if (p == next_p) break;
        
        p = next_p;
        count++;
    }
    return count;
}

CameraModel* camera_model_create(const char* yaml_path) {
    FILE* fp = fopen(yaml_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open calibration file '%s'\n", yaml_path);
        return NULL;
    }
    
    CameraModel* model = (CameraModel*)calloc(1, sizeof(CameraModel));
    if (!model) {
        fclose(fp);
        return NULL;
    }
    
    char line[512];
    char current_section[64] = "";
    
    while (fgets(line, sizeof(line), fp)) {
        char* trimmed = trim(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') continue;
        
        // Track the current section for "data" fields
        if (strstr(trimmed, "camera_matrix:") == trimmed) {
            strncpy(current_section, "camera_matrix", sizeof(current_section) - 1);
            continue;
        } else if (strstr(trimmed, "distortion_coefficients:") == trimmed) {
            strncpy(current_section, "distortion_coefficients", sizeof(current_section) - 1);
            continue;
        }
        
        char* colon = strchr(trimmed, ':');
        if (!colon) continue;
        
        *colon = '\0';
        char* key = trim(trimmed);
        char* val = trim(colon + 1);
        
        if (strcmp(key, "sensor_width") == 0) {
            model->sensor_width = atoi(val);
        } else if (strcmp(key, "sensor_height") == 0) {
            model->sensor_height = atoi(val);
        } else if (strcmp(key, "camera_model") == 0) {
            if (val[0] == '"' || val[0] == '\'') val++;
            size_t len = strlen(val);
            if (len > 0 && (val[len - 1] == '"' || val[len - 1] == '\'')) {
                val[len - 1] = '\0';
            }
            strncpy(model->camera_model, val, sizeof(model->camera_model) - 1);
        } else if (strcmp(key, "data") == 0) {
            if (strcmp(current_section, "camera_matrix") == 0) {
                parse_double_array(val, model->K, 9);
            } else if (strcmp(current_section, "distortion_coefficients") == 0) {
                model->D_len = parse_double_array(val, model->D, 8);
            }
        }
    }
    
    fclose(fp);
    
    // Initialize rectification map pointers as NULL
    model->map1 = NULL;
    model->map2 = NULL;
    
    return model;
}

void camera_model_destroy(CameraModel* model) {
    if (!model) return;
    
    if (model->map1) {
        cv_wrapper_free_mat(model->map1);
        model->map1 = NULL;
    }
    if (model->map2) {
        cv_wrapper_free_mat(model->map2);
        model->map2 = NULL;
    }
    
    free(model);
}

int camera_model_pixel_to_angle(const CameraModel* model, double u, double v, double* yaw, double* pitch) {
    if (!model || !yaw || !pitch) return -1;
    
    double fx = model->K[0];
    double fy = model->K[4];
    double cx = model->K[2];
    double cy = model->K[5];
    
    if (strcmp(model->camera_model, "pinhole") == 0) {
        double x_double_prime = (u - cx) / fx;
        double y_double_prime = (v - cy) / fy;
        
        double k1 = model->D[0];
        double k2 = model->D[1];
        double p1 = model->D[2];
        double p2 = model->D[3];
        double k3 = (model->D_len > 4) ? model->D[4] : 0.0;
        
        // Standard iterative undistortion
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
        
        *yaw = atan(x);
        *pitch = atan2(-y, sqrt(x * x + 1.0));
    } else {
        // Fisheye undistortion
        double x_double_prime = (u - cx) / fx;
        double y_double_prime = (v - cy) / fy;
        
        double theta_d = sqrt(x_double_prime * x_double_prime + y_double_prime * y_double_prime);
        double theta = theta_d; // initial guess
        
        double k1 = model->D[0];
        double k2 = model->D[1];
        double k3 = model->D[2];
        double k4 = model->D[3];
        
        for (int iter = 0; iter < 10; iter++) {
            double th2 = theta * theta;
            double th4 = th2 * th2;
            double th6 = th4 * th2;
            double th8 = th4 * th4;
            
            double f = theta * (1.0 + k1 * th2 + k2 * th4 + k3 * th6 + k4 * th8) - theta_d;
            double df = 1.0 + 3.0 * k1 * th2 + 5.0 * k2 * th4 + 7.0 * k3 * th6 + 9.0 * k4 * th8;
            
            if (fabs(df) < 1e-12) break;
            theta = theta - f / df;
        }
        
        double scale = 1.0;
        if (theta_d > 1e-8) {
            scale = tan(theta) / theta_d;
        }
        
        double x = x_double_prime * scale;
        double y = y_double_prime * scale;
        
        *yaw = atan(x);
        *pitch = atan2(-y, sqrt(x * x + 1.0));
    }
    return 0;
}

int camera_model_angle_to_pixel(const CameraModel* model, double yaw, double pitch, double* u, double* v) {
    if (!model || !u || !v) return -1;
    
    double fx = model->K[0];
    double fy = model->K[4];
    double cx = model->K[2];
    double cy = model->K[5];
    
    double x_prime = tan(yaw);
    double cos_yaw = cos(yaw);
    if (fabs(cos_yaw) < 1e-7) {
        cos_yaw = (cos_yaw >= 0.0) ? 1e-7 : -1e-7;
    }
    double y_prime = -tan(pitch) / cos_yaw;
    
    if (strcmp(model->camera_model, "pinhole") == 0) {
        double r2 = x_prime * x_prime + y_prime * y_prime;
        double r4 = r2 * r2;
        double r6 = r4 * r2;
        
        double k1 = model->D[0];
        double k2 = model->D[1];
        double p1 = model->D[2];
        double p2 = model->D[3];
        double k3 = (model->D_len > 4) ? model->D[4] : 0.0;
        
        double cdist = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;
        double delta_x = 2.0 * p1 * x_prime * y_prime + p2 * (r2 + 2.0 * x_prime * x_prime);
        double delta_y = p1 * (r2 + 2.0 * y_prime * y_prime) + 2.0 * p2 * x_prime * y_prime;
        
        double x_dist = x_prime * cdist + delta_x;
        double y_dist = y_prime * cdist + delta_y;
        
        *u = fx * x_dist + cx;
        *v = fy * y_dist + cy;
    } else {
        double r = sqrt(x_prime * x_prime + y_prime * y_prime);
        double theta = atan(r);
        double theta_d = theta;
        
        if (r > 1e-8) {
            double k1 = model->D[0];
            double k2 = model->D[1];
            double k3 = model->D[2];
            double k4 = model->D[3];
            
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
        
        *u = fx * x_dist + cx;
        *v = fy * y_dist + cy;
    }
    return 0;
}

int camera_model_undistort(CameraModel* model, const CvImage* src, CvImage* dst) {
    if (!model || !src || !dst) return -1;
    
    if (!model->map1 || !model->map2) {
        model->map1 = cv_wrapper_create_empty_mat();
        model->map2 = cv_wrapper_create_empty_mat();
        cv_wrapper_init_rectification_maps(
            model->K, model->D, model->D_len,
            model->camera_model, model->sensor_width, model->sensor_height,
            model->map1, model->map2
        );
    }
    
    cv_wrapper_remap(src, dst, model->map1, model->map2);
    return 0;
}

int camera_model_get_width(const CameraModel* model) {
    return model ? model->sensor_width : 0;
}

int camera_model_get_height(const CameraModel* model) {
    return model ? model->sensor_height : 0;
}

const char* camera_model_get_type(const CameraModel* model) {
    return model ? model->camera_model : "";
}

void camera_model_get_K(const CameraModel* model, double K_out[9]) {
    if (model && K_out) {
        memcpy(K_out, model->K, 9 * sizeof(double));
    }
}

void camera_model_get_D(const CameraModel* model, double D_out[8], int* D_len_out) {
    if (model) {
        if (D_out) {
            memcpy(D_out, model->D, model->D_len * sizeof(double));
        }
        if (D_len_out) {
            *D_len_out = model->D_len;
        }
    }
}
