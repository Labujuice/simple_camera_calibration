#!/usr/bin/env python3
import argparse
import json
import os
import glob
import time
import yaml
import cv2
import numpy as np

class CornerTracker:
    def __init__(self, history_len=3):
        self.history = []
        self.history_len = history_len

    def update(self, corners):
        self.history.append(corners.copy())
        if len(self.history) > self.history_len:
            self.history.pop(0)

    def is_stable(self, threshold=2.0):
        if len(self.history) < self.history_len:
            return False
        max_dist = 0.0
        for i in range(len(self.history) - 1):
            diff = self.history[i+1] - self.history[i]
            dists = np.linalg.norm(diff, axis=2)
            max_dist = max(max_dist, np.max(dists))
        return max_dist < threshold

    def clear(self):
        self.history.clear()

def main():
    parser = argparse.ArgumentParser(description="Real-time Camera Calibration Tool")
    parser.add_argument("--camera", type=int, default=0, help="Webcam index (default: 0)")
    parser.add_argument("--width", type=int, default=0, help="Optional: Target webcam resolution width (e.g. 1920)")
    parser.add_argument("--height", type=int, default=0, help="Optional: Target webcam resolution height (e.g. 1080)")
    parser.add_argument("--model", type=str, default="pinhole", choices=["pinhole", "fisheye"], help="Camera model (default: pinhole)")
    parser.add_argument("--output", type=str, default="camera_calibration.yaml", help="Output calibration file (default: camera_calibration.yaml)")
    parser.add_argument("--rows", type=int, default=7, help="Default squares rows (if QR code not scanned)")
    parser.add_argument("--cols", type=int, default=8, help="Default squares columns (if QR code not scanned)")
    parser.add_argument("--size", type=float, default=20.0, help="Default square size in mm (if QR code not scanned)")
    parser.add_argument("--grid", type=int, default=4, help="Coverage grid dimension NxN (default: 4)")
    parser.add_argument("--images", type=str, default="", help="Optional: Directory containing images to calibrate offline instead of webcam")
    
    args = parser.parse_args()
    
    # Grid settings
    grid_sz = args.grid
    coverage = np.zeros((grid_sz, grid_sz), dtype=bool)
    
    # Calibration parameters (can be updated dynamically by QR Code)
    rows = args.rows
    cols = args.cols
    size_mm = args.size
    
    # Internal state helper to calculate object points
    def get_objp(r, c, sz):
        pattern_size = (c - 1, r - 1)
        objp = np.zeros((pattern_size[0] * pattern_size[1], 3), np.float32)
        objp[:, :2] = np.mgrid[0:pattern_size[0], 0:pattern_size[1]].T.reshape(-1, 2) * sz
        return pattern_size, objp

    pattern_size, objp = get_objp(rows, cols, size_mm)
    
    objpoints = [] # 3d points in real world space
    imgpoints = [] # 2d points in image plane
    
    qr_detector = cv2.QRCodeDetector()
    tracker = CornerTracker(history_len=3)
    
    # Status overlay variables
    last_capture_time = 0
    capture_message = ""
    qr_auto_configured = False
    
    # Run offline calibration if images directory is specified
    if args.images:
        print(f"Running offline calibration on images in: {args.images}")
        image_files = []
        for ext in ["*.jpg", "*.jpeg", "*.png", "*.bmp"]:
            image_files.extend(glob.glob(os.path.join(args.images, ext)))
            image_files.extend(glob.glob(os.path.join(args.images, ext.upper())))
            
        if not image_files:
            print(f"Error: No image files found in {args.images}")
            return
            
        print(f"Found {len(image_files)} images. Processing...")
        
        # We need to open the first image to get dimensions
        first_img = cv2.imread(image_files[0])
        if first_img is None:
            print(f"Error: Could not read image {image_files[0]}")
            return
        H, W = first_img.shape[:2]
        
        for idx, img_path in enumerate(image_files):
            img = cv2.imread(img_path)
            if img is None:
                continue
            
            # Check QR code
            val, pts, _ = qr_detector.detectAndDecode(img)
            if val:
                try:
                    data = json.loads(val)
                    if "rows" in data and "cols" in data and "size_mm" in data:
                        new_rows = int(data["rows"])
                        new_cols = int(data["cols"])
                        new_size = float(data["size_mm"])
                        if (new_rows != rows or new_cols != cols or new_size != size_mm):
                            rows, cols, size_mm = new_rows, new_cols, new_size
                            pattern_size, objp = get_objp(rows, cols, size_mm)
                            print(f"QR code decoded in {os.path.basename(img_path)}: Reconfigured to {rows}x{cols} squares ({size_mm}mm). Resetting previous points.")
                            objpoints.clear()
                            imgpoints.clear()
                            coverage.fill(False)
                except Exception:
                    pass
            
            # Detect chessboard
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            ret, corners = cv2.findChessboardCorners(gray, pattern_size, cv2.CALIB_CB_ADAPTIVE_THRESH + cv2.CALIB_CB_NORMALIZE_IMAGE)
            
            if ret:
                criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
                corners = cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
                
                cx = np.mean(corners[:, 0, 0])
                cy = np.mean(corners[:, 0, 1])
                c_idx = int(cx / (W / grid_sz))
                r_idx = int(cy / (H / grid_sz))
                
                if 0 <= r_idx < grid_sz and 0 <= c_idx < grid_sz:
                    objpoints.append(objp)
                    imgpoints.append(corners)
                    coverage[r_idx, c_idx] = True
                    print(f"[{idx+1}/{len(image_files)}] Chessboard found in {os.path.basename(img_path)}. Covered cell ({r_idx}, {c_idx})")
            else:
                print(f"[{idx+1}/{len(image_files)}] Chessboard NOT found in {os.path.basename(img_path)} using pattern size {pattern_size}")
                
        # Perform calibration
        if len(objpoints) < 3:
            print(f"Error: Not enough chessboard images detected for calibration (found {len(objpoints)}, need >= 3).")
            return
            
        print(f"Starting camera calibration using {len(objpoints)} frames...")
        run_calibration(objpoints, imgpoints, (W, H), args.model, args.output)
        return

    # Real-time calibration using webcam
    cap = cv2.VideoCapture(args.camera)
    if not cap.isOpened():
        print(f"Error: Could not open camera with index {args.camera}")
        print("Tip: If you don't have a webcam, you can run offline calibration using pre-captured images by specifying '--images <directory>'")
        return
        
    # Configure custom resolution if requested
    if args.width > 0:
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    if args.height > 0:
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
        
    # Get actual frame dimensions
    ret, frame = cap.read()
    if not ret:
        print("Error: Could not read frame from camera.")
        return
    H, W = frame.shape[:2]
    
    print(f"Camera opened successfully. Actual resolution: {W}x{H}")
    print("Instructions:")
    print("  1. Show the QR code to the camera first to automatically load configuration (optional).")
    print("  2. Move the chessboard corners to cover all cells of the grid.")
    print("  3. Hold the chessboard still to capture. Stable frames are automatically captured.")
    print("  4. Press 'c' to force calibration immediately, 'r' to reset progress, 'q' to quit.")
    
    cv2.namedWindow("Camera Calibration - Antigravity", cv2.WINDOW_NORMAL)
    print("Starting real-time calibration...")
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
            
        display_frame = frame.copy()
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # 1. Scan QR code every 5 frames or if not configured
        val, pts, _ = qr_detector.detectAndDecode(frame)
        if val:
            try:
                data = json.loads(val)
                if "rows" in data and "cols" in data and "size_mm" in data:
                    new_rows = int(data["rows"])
                    new_cols = int(data["cols"])
                    new_size = float(data["size_mm"])
                    if (new_rows != rows or new_cols != cols or new_size != size_mm):
                        rows, cols, size_mm = new_rows, new_cols, new_size
                        pattern_size, objp = get_objp(rows, cols, size_mm)
                        # Reset
                        objpoints.clear()
                        imgpoints.clear()
                        coverage.fill(False)
                        tracker.clear()
                        qr_auto_configured = True
                        last_capture_time = time.time()
                        capture_message = f"Auto-Configured: {rows}x{cols} (size: {size_mm}mm)"
            except Exception:
                pass
                
            # Draw QR Code bounding box (blue)
            if pts is not None:
                pts = pts.astype(int)
                cv2.polylines(display_frame, [pts], True, (255, 0, 0), 2)
                cv2.putText(display_frame, "QR Code", (pts[0][0][0], pts[0][0][1] - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 1)
        
        # 2. Detect chessboard
        ret_corners, corners = cv2.findChessboardCorners(gray, pattern_size, cv2.CALIB_CB_ADAPTIVE_THRESH + cv2.CALIB_CB_FAST_CHECK + cv2.CALIB_CB_NORMALIZE_IMAGE)
        
        if ret_corners:
            # Refine corner locations
            criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
            corners = cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
            
            # Draw chessboard corners
            cv2.drawChessboardCorners(display_frame, pattern_size, corners, ret_corners)
            
            # Draw bounding box around chessboard corners (yellow)
            x_c = corners[:, 0, 0]
            y_c = corners[:, 0, 1]
            min_x, max_x = int(np.min(x_c)), int(np.max(x_c))
            min_y, max_y = int(np.min(y_c)), int(np.max(y_c))
            cv2.rectangle(display_frame, (min_x, min_y), (max_x, max_y), (0, 255, 255), 2)
            cv2.putText(display_frame, f"Chessboard {pattern_size[0]}x{pattern_size[1]} corners", (min_x, min_y - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
            
            # Check stability
            tracker.update(corners)
            is_stable = tracker.is_stable(threshold=2.0)
            
            # Get grid cell index of the chessboard centroid
            cx, cy = np.mean(x_c), np.mean(y_c)
            col_idx = int(cx / (W / grid_sz))
            row_idx = int(cy / (H / grid_sz))
            
            if 0 <= row_idx < grid_sz and 0 <= col_idx < grid_sz:
                if is_stable:
                    if not coverage[row_idx, col_idx]:
                        # Capture stable frame
                        objpoints.append(objp)
                        imgpoints.append(corners)
                        coverage[row_idx, col_idx] = True
                        tracker.clear() # Reset stability tracker
                        last_capture_time = time.time()
                        capture_message = f"Captured Cell ({row_idx}, {col_idx})!"
                    else:
                        cv2.putText(display_frame, "Cell Already Covered", (int(cx) - 80, int(cy)), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
                else:
                    # Show indicator that it is waiting for stability
                    cv2.circle(display_frame, (int(cx), int(cy)), 15, (0, 165, 255), 2) # Orange circle
                    cv2.putText(display_frame, "Hold Still...", (int(cx) - 40, int(cy) - 25), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 165, 255), 1)
        else:
            tracker.clear()
            
        # 3. Draw Coverage Grid Overlay
        overlay = display_frame.copy()
        cell_w = W / grid_sz
        cell_h = H / grid_sz
        
        # Fill covered cells with semi-transparent green
        for r in range(grid_sz):
            for c_idx in range(grid_sz):
                x1 = int(c_idx * cell_w)
                y1 = int(r * cell_h)
                x2 = int((c_idx + 1) * cell_w)
                y2 = int((r + 1) * cell_h)
                
                if coverage[r, c_idx]:
                    cv2.rectangle(overlay, (x1, y1), (x2, y2), (0, 255, 0), -1) # Filled green
                
                # Draw cell borders
                cv2.rectangle(overlay, (x1, y1), (x2, y2), (200, 200, 200), 1)
                
        # Blend overlay
        display_frame = cv2.addWeighted(overlay, 0.35, display_frame, 0.65, 0)
        
        # 4. Draw Capture Notification
        if time.time() - last_capture_time < 1.5:
            # Draw a banner
            cv2.rectangle(display_frame, (0, H // 2 - 30), (W, H // 2 + 10), (0, 0, 0), -1)
            cv2.putText(display_frame, capture_message, (W // 2 - 200, H // 2 - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
            
        # 5. Draw Bottom Status Bar
        covered_count = np.sum(coverage)
        total_cells = grid_sz * grid_sz
        pct = int((covered_count / total_cells) * 100)
        
        # Dark status bar at the bottom
        cv2.rectangle(display_frame, (0, H - 45), (W, H), (40, 40, 40), -1)
        status_text = f"Model: {args.model.upper()} | Target: {rows}x{cols} ({size_mm}mm) | Coverage: {covered_count}/{total_cells} ({pct}%)"
        cv2.putText(display_frame, status_text, (15, H - 28), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1, cv2.LINE_AA)
        
        control_text = "ESC: Quit | R: Reset | C: Force Calibrate"
        cv2.putText(display_frame, control_text, (15, H - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (200, 200, 200), 1, cv2.LINE_AA)
        
        # Show window
        cv2.imshow("Camera Calibration - Antigravity", display_frame)
        
        # Check if fully covered
        if covered_count == total_cells:
            print("Coverage complete! Starting calibration...")
            # Draw "Processing" overlay
            cv2.rectangle(display_frame, (0, 0), (W, H), (0, 0, 0), -1)
            cv2.putText(display_frame, "CALIBRATING... PLEASE WAIT", (W // 2 - 180, H // 2), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
            cv2.imshow("Camera Calibration - Antigravity", display_frame)
            cv2.waitKey(500)
            
            run_calibration(objpoints, imgpoints, (W, H), args.model, args.output)
            break
            
        # Handle keypresses
        key = cv2.waitKey(1) & 0xFF
        if key == 27 or key == ord('q'): # ESC or q
            print("Calibration aborted by user.")
            break
        elif key == ord('r'): # Reset
            print("Resetting calibration progress.")
            objpoints.clear()
            imgpoints.clear()
            coverage.fill(False)
            tracker.clear()
            last_capture_time = time.time()
            capture_message = "Progress Reset"
        elif key == ord('c'): # Force calibrate
            if len(objpoints) >= 3:
                print("Force calibration triggered by user...")
                run_calibration(objpoints, imgpoints, (W, H), args.model, args.output)
                break
            else:
                last_capture_time = time.time()
                capture_message = f"Need >= 3 captured cells (Current: {len(objpoints)})"
                
    cap.release()
    cv2.destroyAllWindows()

def run_calibration(objpoints, imgpoints, size, model, output_path):
    W, H = size
    print(f"Calibrating using {len(objpoints)} frames...")
    
    # Initial guess for camera matrix (focal length ~ max(W, H), center ~ W/2, H/2)
    f_init = max(W, H)
    mtx = np.array([
        [f_init, 0.0, W / 2.0],
        [0.0, f_init, H / 2.0],
        [0.0, 0.0, 1.0]
    ], dtype=np.float64)
    
    if model == "pinhole":
        dist = np.zeros((5, 1), dtype=np.float64)
        flags = cv2.CALIB_USE_INTRINSIC_GUESS
        ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(
            objpoints, imgpoints, (W, H), mtx, dist, flags=flags
        )
        rms = ret
    else: # fisheye
        # Convert inputs to float64 and precise shape (N, 1, 3) / (N, 1, 2)
        objpoints_fe = [np.array(op, dtype=np.float64).reshape(-1, 1, 3) for op in objpoints]
        imgpoints_fe = [np.array(ip, dtype=np.float64).reshape(-1, 1, 2) for ip in imgpoints]
        
        dist = np.zeros((4, 1), dtype=np.float64)
        rvecs = []
        tvecs = []
        flags = cv2.fisheye.CALIB_USE_INTRINSIC_GUESS + cv2.fisheye.CALIB_RECOMPUTE_EXTRINSIC + cv2.fisheye.CALIB_FIX_SKEW
        
        ret, mtx, dist, rvecs, tvecs = cv2.fisheye.calibrate(
            objpoints_fe, imgpoints_fe, (W, H), mtx, dist, rvecs, tvecs, flags
        )
        rms = ret
        
    print(f"Calibration successful! RMS Error: {rms:.4f}")
    print("Camera Matrix:\n", mtx)
    print("Distortion Coefficients:\n", dist.flatten())
    
    # Save results manually in the specified format
    yaml_content = f"""sensor_width: {W}
sensor_height: {H}
camera_model: "{model}"
camera_matrix:
  rows: 3
  cols: 3
  data: [{', '.join(map(str, mtx.flatten().tolist()))}]
distortion_coefficients:
  rows: 1
  cols: {len(dist.flatten())}
  data: [{', '.join(map(str, dist.flatten().tolist()))}]
"""
    
    with open(output_path, 'w') as f:
        f.write(yaml_content)
        
    print(f"Calibration results saved to {output_path}")

if __name__ == "__main__":
    main()
