#!/usr/bin/env python3
import argparse
import os
import glob
import numpy as np
import cv2
from camera_model import CameraModel, select_camera

# Global variables for mouse tracking
mouse_u = 0
mouse_v = 0
mouse_side = "left" # "left" or "right"

def is_on_screen(pt, W, H):
    return -50 <= pt[0] < W + 50 and -50 <= pt[1] < H + 50

def get_undistorted_pixel(model, yaw, pitch):
    x_prime = np.tan(yaw)
    cos_yaw = np.cos(yaw)
    if abs(cos_yaw) < 1e-7:
        cos_yaw = 1e-7 if cos_yaw >= 0 else -1e-7
    y_prime = -np.tan(pitch) / cos_yaw
    
    u = model.K[0, 0] * x_prime + model.K[0, 2]
    v = model.K[1, 1] * y_prime + model.K[1, 2]
    return u, v

def draw_angle_grid(img, model, use_distortion, color=(0, 200, 200), thickness=1):
    H, W = img.shape[:2]
    
    # Grid lines every 10 degrees
    yaws = np.arange(-60, 61, 10)
    pitches = np.arange(-40, 41, 10)
    
    # 1. Constant Yaw lines (curves of constant longitude)
    for y_deg in yaws:
        y_rad = np.radians(y_deg)
        pts = []
        # Sample pitch along the line
        for p_deg in range(-45, 46, 2):
            p_rad = np.radians(p_deg)
            if use_distortion:
                u, v = model.angle_to_pixel(y_rad, p_rad)
            else:
                u, v = get_undistorted_pixel(model, y_rad, p_rad)
            
            if np.isnan(u) or np.isnan(v) or abs(u) > 5*W or abs(v) > 5*H:
                continue
            pts.append((int(u), int(v)))
            
        # Draw curve
        for i in range(len(pts) - 1):
            p1, p2 = pts[i], pts[i+1]
            if is_on_screen(p1, W, H) or is_on_screen(p2, W, H):
                cv2.line(img, p1, p2, color, thickness, cv2.LINE_AA)
                
        # Draw Yaw Label at Pitch = 0 (middle of the line)
        if len(pts) > 0:
            mid_idx = len(pts) // 2
            lbl_u, lbl_v = pts[mid_idx]
            if 10 <= lbl_u < W - 10 and 10 <= lbl_v < H - 10:
                label = f"{y_deg}°"
                cv2.putText(img, label, (lbl_u + 3, lbl_v - 3), cv2.FONT_HERSHEY_SIMPLEX, 0.35, (0, 0, 0), 2, cv2.LINE_AA)
                cv2.putText(img, label, (lbl_u + 3, lbl_v - 3), cv2.FONT_HERSHEY_SIMPLEX, 0.35, (255, 255, 255), 1, cv2.LINE_AA)

    # 2. Constant Pitch lines (curves of constant latitude)
    for p_deg in pitches:
        p_rad = np.radians(p_deg)
        pts = []
        # Sample yaw along the line
        for y_deg in range(-65, 66, 2):
            y_rad = np.radians(y_deg)
            if use_distortion:
                u, v = model.angle_to_pixel(y_rad, p_rad)
            else:
                u, v = get_undistorted_pixel(model, y_rad, p_rad)
                
            if np.isnan(u) or np.isnan(v) or abs(u) > 5*W or abs(v) > 5*H:
                continue
            pts.append((int(u), int(v)))
            
        for i in range(len(pts) - 1):
            p1, p2 = pts[i], pts[i+1]
            if is_on_screen(p1, W, H) or is_on_screen(p2, W, H):
                cv2.line(img, p1, p2, color, thickness, cv2.LINE_AA)
                
        # Draw Pitch Label at Yaw = 0 (middle of the line)
        if len(pts) > 0:
            mid_idx = len(pts) // 2
            lbl_u, lbl_v = pts[mid_idx]
            if 10 <= lbl_u < W - 10 and 10 <= lbl_v < H - 10:
                label = f"{p_deg}°"
                cv2.putText(img, label, (lbl_u + 3, lbl_v - 3), cv2.FONT_HERSHEY_SIMPLEX, 0.35, (0, 0, 0), 2, cv2.LINE_AA)
                cv2.putText(img, label, (lbl_u + 3, lbl_v - 3), cv2.FONT_HERSHEY_SIMPLEX, 0.35, (255, 255, 255), 1, cv2.LINE_AA)

def on_mouse(event, x, y, flags, param):
    global mouse_u, mouse_v, mouse_side
    W = param["width"]
    if x < W:
        mouse_u = max(0, min(x, W - 1))
        mouse_v = max(0, min(y, param["height"] - 1))
        mouse_side = "left"
    else:
        mouse_u = max(0, min(x - W, W - 1))
        mouse_v = max(0, min(y, param["height"] - 1))
        mouse_side = "right"

def main():
    parser = argparse.ArgumentParser(description="Calibration Verification & Demo Program")
    parser.add_argument("--calibration", type=str, default="camera_calibration.yaml", help="Path to calibration YAML (default: camera_calibration.yaml)")
    parser.add_argument("--camera", type=int, default=None, help="Webcam index. If not specified, a selection menu will be shown.")
    parser.add_argument("--images", type=str, default="", help="Optional: Directory containing images to verify offline")
    parser.add_argument("--image", type=str, default="", help="Optional: Path to a single image to verify offline")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.calibration):
        print(f"Error: Calibration file '{args.calibration}' not found.")
        print("Please run 'calibrate.py' first to generate a calibration file.")
        return
        
    # Load model
    print(f"Loading camera calibration model: {args.calibration}")
    model = CameraModel(args.calibration)
    print(f"Camera Model: {model.camera_model.upper()}")
    print(f"Calibrated Resolution: {model.sensor_width}x{model.sensor_height}")
    
    # Precompute remapping maps
    model.init_rectification_maps()
    
    # Load input source
    is_static = False
    image_paths = []
    current_img_idx = 0
    cap = None
    
    if args.image:
        if not os.path.exists(args.image):
            print(f"Error: Image file '{args.image}' not found.")
            return
        image_paths = [args.image]
        is_static = True
    elif args.images:
        if not os.path.exists(args.images):
            print(f"Error: Directory '{args.images}' not found.")
            return
        for ext in ["*.jpg", "*.jpeg", "*.png", "*.bmp"]:
            image_paths.extend(glob.glob(os.path.join(args.images, ext)))
            image_paths.extend(glob.glob(os.path.join(args.images, ext.upper())))
        image_paths.sort()
        if not image_paths:
            print(f"Error: No images found in '{args.images}'")
            return
        is_static = True
    else:
        # Camera
        if args.camera is None:
            camera_index = select_camera()
            if camera_index is None:
                return
        else:
            camera_index = args.camera

        cap = cv2.VideoCapture(camera_index)
        if not cap.isOpened():
            print(f"Error: Could not open camera with index {camera_index}")
            print("Tip: You can verify using a saved image by specifying '--image <path>' or '--images <dir>'")
            return
        
        # Check size, set camera size to match calibration if possible
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, model.sensor_width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, model.sensor_height)
        
    # Window setup
    window_name = "Calibration Verification - Antigravity"
    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
    
    # Mouse callback registration parameters
    # We will get frame dimensions from the first frame/image
    if is_static:
        test_img = cv2.imread(image_paths[0])
        H, W = test_img.shape[:2]
    else:
        ret, test_frame = cap.read()
        if not ret:
            print("Error: Could not read frame from camera.")
            return
        H, W = test_frame.shape[:2]
        
    cv2.setMouseCallback(window_name, on_mouse, {"width": W, "height": H})
    
    show_grid = True
    
    print("\nInstructions:")
    print("  - Move mouse cursor over the images to see real-time Yaw/Pitch values.")
    print("  - Crosshair on the other image displays the mapped position.")
    print("  - Press 'g' to toggle the angle grid overlay.")
    if is_static and len(image_paths) > 1:
        print("  - Press Space or Right Arrow to go to the next image.")
        print("  - Press Left Arrow to go to the previous image.")
    print("  - Press 'q' or ESC to exit.")
    
    while True:
        if is_static:
            frame = cv2.imread(image_paths[current_img_idx])
            if frame is None:
                print(f"Error reading image {image_paths[current_img_idx]}")
                break
        else:
            ret, frame = cap.read()
            if not ret:
                break
                
        # Resize frame if it doesn't match calibration size
        # This makes sure mapping is exact
        if frame.shape[1] != W or frame.shape[0] != H:
            frame = cv2.resize(frame, (W, H))
            
        # 1. Undistort the image
        undistorted_frame = model.undistort(frame)
        
        # Keep copy for overlaying grid/cursor
        display_dist = frame.copy()
        display_undist = undistorted_frame.copy()
        
        # 2. Draw Angle Grid Overlay if enabled
        if show_grid:
            # Grid color: cyan-yellow (0, 180, 180) on distorted, (180, 180, 0) on undistorted
            draw_angle_grid(display_dist, model, use_distortion=True, color=(0, 200, 200), thickness=1)
            draw_angle_grid(display_undist, model, use_distortion=False, color=(200, 200, 0), thickness=1)
            
        # 3. Calculate Angles and Mapped Coordinates at mouse position
        if mouse_side == "left":
            # Mouse is on Distorted image (original)
            yaw_rad, pitch_rad = model.pixel_to_angle(mouse_u, mouse_v)
            # Find where this yaw/pitch maps on the undistorted image
            u_mapped, v_mapped = get_undistorted_pixel(model, yaw_rad, pitch_rad)
            u_dist, v_dist = mouse_u, mouse_v
            u_undist, v_undist = u_mapped, v_mapped
        else:
            # Mouse is on Undistorted image
            # Calculate yaw/pitch directly using inverse projection (rectilinear)
            x_prime = (mouse_u - model.K[0, 2]) / model.K[0, 0]
            y_prime = (mouse_v - model.K[1, 2]) / model.K[1, 1]
            yaw_rad = np.arctan(x_prime)
            pitch_rad = np.arctan2(-y_prime, np.sqrt(x_prime**2 + 1))
            # Find where this yaw/pitch maps on the distorted image
            u_mapped, v_mapped = model.angle_to_pixel(yaw_rad, pitch_rad)
            u_dist, v_dist = u_mapped, v_mapped
            u_undist, v_undist = mouse_u, mouse_v
            
        # Draw Crosshair on Distorted Frame
        if is_on_screen((u_dist, v_dist), W, H):
            cv2.drawMarker(display_dist, (int(u_dist), int(v_dist)), (0, 165, 255), cv2.MARKER_CROSS, 15, 2)
            cv2.circle(display_dist, (int(u_dist), int(v_dist)), 4, (0, 165, 255), 1)
            
        # Draw Crosshair on Undistorted Frame
        if is_on_screen((u_undist, v_undist), W, H):
            cv2.drawMarker(display_undist, (int(u_undist), int(v_undist)), (0, 165, 255), cv2.MARKER_CROSS, 15, 2)
            cv2.circle(display_undist, (int(u_undist), int(v_undist)), 4, (0, 165, 255), 1)
            
        # Combine side-by-side
        combined = np.hstack((display_dist, display_undist))
        
        # 4. Draw Tooltip Box on Combined Image
        # Coordinates of the tooltip box
        cursor_x = mouse_u if mouse_side == "left" else mouse_u + W
        cursor_y = mouse_v
        
        # Tooltip size
        box_w, box_h = 220, 75
        tx = cursor_x + 15
        ty = cursor_y + 15
        
        # Adjust tooltip position if it goes off window borders
        if tx + box_w > 2 * W:
            tx = cursor_x - box_w - 15
        if ty + box_h > H:
            ty = cursor_y - box_h - 15
            
        # Draw semi-transparent tooltip background
        tooltip_overlay = combined.copy()
        cv2.rectangle(tooltip_overlay, (int(tx), int(ty)), (int(tx + box_w), int(ty + box_h)), (30, 30, 30), -1)
        combined = cv2.addWeighted(tooltip_overlay, 0.75, combined, 0.25, 0)
        
        # Tooltip content
        cv2.rectangle(combined, (int(tx), int(ty)), (int(tx + box_w), int(ty + box_h)), (150, 150, 150), 1) # border
        
        yaw_deg = np.degrees(yaw_rad)
        pitch_deg = np.degrees(pitch_rad)
        
        # Text settings
        font = cv2.FONT_HERSHEY_SIMPLEX
        font_scale = 0.35
        text_color = (255, 255, 255)
        line_height = 16
        
        # Display Info:
        # 1. Pixel coordinate
        cv2.putText(combined, f"Pixel ({mouse_side.upper()}): {mouse_u}, {mouse_v}", (int(tx + 8), int(ty + 15)), font, font_scale, text_color, 1, cv2.LINE_AA)
        # 2. Yaw (deg & rad)
        cv2.putText(combined, f"Yaw:   {yaw_deg:+.2f} deg ({yaw_rad:+.4f} rad)", (int(tx + 8), int(ty + 15 + line_height)), font, font_scale, (100, 255, 100), 1, cv2.LINE_AA)
        # 3. Pitch (deg & rad)
        cv2.putText(combined, f"Pitch: {pitch_deg:+.2f} deg ({pitch_rad:+.4f} rad)", (int(tx + 8), int(ty + 15 + 2*line_height)), font, font_scale, (100, 255, 100), 1, cv2.LINE_AA)
        # 4. Calibration model
        cv2.putText(combined, f"Calib Model: {model.camera_model.upper()}", (int(tx + 8), int(ty + 15 + 3*line_height)), font, font_scale, (200, 200, 200), 1, cv2.LINE_AA)
        
        # 5. Draw overlay text on top-left of each screen
        cv2.putText(combined, "ORIGINAL (DISTORTED)", (15, 30), font, 0.6, (0, 0, 255), 2, cv2.LINE_AA)
        cv2.putText(combined, "ORIGINAL (DISTORTED)", (15, 30), font, 0.6, (255, 255, 255), 1, cv2.LINE_AA)
        
        cv2.putText(combined, "RECTIFIED (UNDISTORTED)", (W + 15, 30), font, 0.6, (0, 255, 0), 2, cv2.LINE_AA)
        cv2.putText(combined, "RECTIFIED (UNDISTORTED)", (W + 15, 30), font, 0.6, (255, 255, 255), 1, cv2.LINE_AA)
        
        # If static, draw file index info
        if is_static:
            info_str = f"Image {current_img_idx + 1}/{len(image_paths)}: {os.path.basename(image_paths[current_img_idx])}"
            cv2.putText(combined, info_str, (15, H - 15), font, 0.5, (0, 0, 0), 2, cv2.LINE_AA)
            cv2.putText(combined, info_str, (15, H - 15), font, 0.5, (255, 255, 255), 1, cv2.LINE_AA)
            
        cv2.imshow(window_name, combined)
        
        # Handle keypresses
        key = cv2.waitKey(30 if not is_static else 100) & 0xFF
        if key == 27 or key == ord('q'): # ESC or q
            break
        elif key == ord('g'): # Toggle grid
            show_grid = not show_grid
            print(f"Grid toggled: {show_grid}")
        elif is_static and len(image_paths) > 1:
            if key == ord(' ') or key == 83: # Space or Right Arrow (usually 83 in opencv but key codes vary, we support standard keys)
                current_img_idx = (current_img_idx + 1) % len(image_paths)
            elif key == 81: # Left Arrow
                current_img_idx = (current_img_idx - 1) % len(image_paths)
                
    if cap is not None:
        cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
