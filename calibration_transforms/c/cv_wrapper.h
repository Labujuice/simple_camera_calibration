#ifndef CV_WRAPPER_H
#define CV_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CvImage CvImage;
typedef struct CvCapture CvCapture;

// Image operations
CvImage* cv_wrapper_image_create_empty(void);
CvImage* cv_wrapper_image_create(int width, int height, int channels);
CvImage* cv_wrapper_imread(const char* filename);
void cv_wrapper_image_free(CvImage* img);
int cv_wrapper_image_width(const CvImage* img);
int cv_wrapper_image_height(const CvImage* img);
void cv_wrapper_image_resize(const CvImage* src, CvImage* dst, int width, int height);
void cv_wrapper_image_copy_to(const CvImage* src, CvImage* dst);
CvImage* cv_wrapper_image_clone(const CvImage* img);

// Drawing and Overlay functions
void cv_wrapper_line(CvImage* img, int x1, int y1, int x2, int y2, int r, int g, int b, int thickness);
void cv_wrapper_rectangle(CvImage* img, int x1, int y1, int x2, int y2, int r, int g, int b, int thickness, int fill);
void cv_wrapper_circle(CvImage* img, int cx, int cy, int radius, int r, int g, int b, int thickness);
void cv_wrapper_draw_marker(CvImage* img, int x, int y, int r, int g, int b, int marker_type, int size, int thickness);
void cv_wrapper_put_text(CvImage* img, const char* text, int x, int y, double font_scale, int r, int g, int b, int thickness);
void cv_wrapper_hstack(const CvImage* img1, const CvImage* img2, CvImage* dst);
void cv_wrapper_add_weighted(const CvImage* src1, double alpha, const CvImage* src2, double beta, double gamma, CvImage* dst);

// Matrix Operations (for mapping maps caching)
void* cv_wrapper_create_empty_mat(void);
void cv_wrapper_free_mat(void* mat);

// Undistort maps initialization and remap
void cv_wrapper_init_rectification_maps(
    const double K[9], const double D[8], int D_len,
    const char* model_type, int width, int height,
    void* map1, void* map2
);
void cv_wrapper_remap(const CvImage* src, CvImage* dst, const void* map1, const void* map2);

// Window and GUI
void cv_wrapper_named_window(const char* name);
void cv_wrapper_resize_window(const char* name, int width, int height);
void cv_wrapper_imshow(const char* name, const CvImage* img);
int cv_wrapper_wait_key(int delay);
void cv_wrapper_destroy_all_windows(void);

// Mouse Callback Wrapper
typedef void (*CvMouseCallback)(int event, int x, int y, int flags, void* userdata);
void cv_wrapper_set_mouse_callback(const char* name, CvMouseCallback callback, void* userdata);

// VideoCapture
CvCapture* cv_wrapper_capture_open(int index);
void cv_wrapper_capture_close(CvCapture* cap);
int cv_wrapper_capture_read(CvCapture* cap, CvImage* img);
void cv_wrapper_capture_set_size(CvCapture* cap, int width, int height);

#ifdef __cplusplus
}
#endif

#endif // CV_WRAPPER_H
