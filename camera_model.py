import yaml
import cv2
import numpy as np

class CameraModel:
    def __init__(self, yaml_path):
        if not os.path.exists(yaml_path):
            raise FileNotFoundError(f"Calibration file not found: {yaml_path}")
            
        with open(yaml_path, 'r') as f:
            data = yaml.safe_load(f)
            
        self.sensor_width = int(data["sensor_width"])
        self.sensor_height = int(data["sensor_height"])
        self.camera_model = data["camera_model"]
        
        # Parse camera matrix
        k_data = data["camera_matrix"]["data"]
        self.K = np.array(k_data, dtype=np.float64).reshape(3, 3)
        
        # Parse distortion coefficients
        d_data = data["distortion_coefficients"]["data"]
        self.D = np.array(d_data, dtype=np.float64)
        
    def pixel_to_angle(self, u, v):
        """
        Convert pixel coordinate (u, v) to physical angles (yaw, pitch) in radians.
        Yaw: positive to the right, negative to the left.
        Pitch: positive upwards, negative downwards.
        """
        pts = np.array([[[u, v]]], dtype=np.float64)
        if self.camera_model == "pinhole":
            pts_undistort = cv2.undistortPoints(pts, self.K, self.D)
        else: # fisheye
            # cv2.fisheye.undistortPoints requires D of shape (4,) or (4,1)
            D_fe = self.D.reshape(-1)
            pts_undistort = cv2.fisheye.undistortPoints(pts, self.K, D_fe)
            
        x_prime = pts_undistort[0, 0, 0]
        y_prime = pts_undistort[0, 0, 1]
        
        yaw = np.arctan(x_prime)
        # pitch is computed relative to the horizontal plane (above is positive)
        pitch = np.arctan2(-y_prime, np.sqrt(x_prime**2 + 1))
        
        return yaw, pitch

    def angle_to_pixel(self, yaw, pitch):
        """
        Convert physical angles (yaw, pitch) in radians to pixel coordinate (u, v).
        """
        # Reconstruct 3D ray in camera space with Z=1
        x_prime = np.tan(yaw)
        # Avoid division by zero or negative cosine values
        cos_yaw = np.cos(yaw)
        if abs(cos_yaw) < 1e-7:
            cos_yaw = 1e-7 if cos_yaw >= 0 else -1e-7
            
        y_prime = -np.tan(pitch) / cos_yaw
        
        pts_3d = np.array([[[x_prime, y_prime, 1.0]]], dtype=np.float64)
        rvec = np.zeros(3, dtype=np.float64)
        tvec = np.zeros(3, dtype=np.float64)
        
        if self.camera_model == "pinhole":
            img_pts, _ = cv2.projectPoints(pts_3d, rvec, tvec, self.K, self.D)
        else: # fisheye
            D_fe = self.D.reshape(-1)
            img_pts, _ = cv2.fisheye.projectPoints(pts_3d, rvec, tvec, self.K, D_fe)
            
        u = img_pts[0, 0, 0]
        v = img_pts[0, 0, 1]
        
        return u, v

    def init_rectification_maps(self):
        """
        Precompute maps for fast remapping (undistortion).
        """
        if self.camera_model == "pinhole":
            self.map1, self.map2 = cv2.initUndistortRectifyMap(
                self.K, self.D, None, self.K, (self.sensor_width, self.sensor_height), cv2.CV_16SC2
            )
        else: # fisheye
            D_fe = self.D.reshape(-1)
            self.map1, self.map2 = cv2.fisheye.initUndistortRectifyMap(
                self.K, D_fe, None, self.K, (self.sensor_width, self.sensor_height), cv2.CV_16SC2
            )
            
    def undistort(self, img):
        """
        Apply fast remapping to undistort the image.
        """
        if not hasattr(self, 'map1'):
            self.init_rectification_maps()
        return cv2.remap(img, self.map1, self.map2, cv2.INTER_LINEAR)

def select_camera(max_tested=8):
    """
    Scans for available camera devices, lists them, and prompts the user to select one.
    If only one is found, it selects it automatically.
    Returns the camera index (int), or None if aborted or no cameras found.
    """
    print("Scanning for available camera devices...")
    available = []
    for i in range(max_tested):
        cap = cv2.VideoCapture(i)
        if cap.isOpened():
            ret, frame = cap.read()
            if ret:
                h, w = frame.shape[:2]
                available.append((i, w, h))
            cap.release()
            
    if not available:
        print("Error: No available camera devices found.")
        return None
        
    if len(available) == 1:
        idx, w, h = available[0]
        print(f"Found 1 camera: Index {idx} ({w}x{h}). Using it automatically.")
        return idx
        
    print("\nAvailable camera devices:")
    for idx, (c_idx, w, h) in enumerate(available):
        print(f"  [{idx}] Camera Index {c_idx} (Default Resolution: {w}x{h})")
        
    while True:
        try:
            choice = input(f"Select a camera [0-{len(available)-1}] (default 0): ").strip()
            if choice == "":
                return available[0][0]
            choice_idx = int(choice)
            if 0 <= choice_idx < len(available):
                return available[choice_idx][0]
            else:
                print(f"Invalid selection. Enter a number between 0 and {len(available)-1}.")
        except ValueError:
            print("Invalid input. Please enter a number.")
        except (KeyboardInterrupt, SystemExit):
            print("\nCamera selection aborted.")
            return None

def select_camera_and_resolution(max_tested=8, default_w=0, default_h=0):
    """
    Scans for cameras, prompts user to select one, and then prompts for target resolution if not specified.
    Returns (camera_index, width, height) or None.
    """
    camera_index = select_camera(max_tested)
    if camera_index is None:
        return None
        
    # If resolution is already specified on command line, use it
    if default_w > 0 and default_h > 0:
        return camera_index, default_w, default_h
        
    print("\nSelect target resolution:")
    print("  [0] Camera Default")
    print("  [1] 1280x720 (HD / 720p)")
    print("  [2] 1920x1080 (Full HD / 1080p)")
    print("  [3] Custom resolution...")
    
    while True:
        try:
            res_choice = input("Select resolution [0-3] (default 0): ").strip()
            if res_choice == "" or res_choice == "0":
                return camera_index, 0, 0
            elif res_choice == "1":
                return camera_index, 1280, 720
            elif res_choice == "2":
                return camera_index, 1920, 1080
            elif res_choice == "3":
                w_str = input("Enter target width: ").strip()
                h_str = input("Enter target height: ").strip()
                return camera_index, int(w_str), int(h_str)
            else:
                print("Invalid selection. Please enter a number between 0 and 3.")
        except ValueError:
            print("Invalid input. Please enter numbers for custom width/height.")
        except (KeyboardInterrupt, SystemExit):
            print("\nResolution selection aborted.")
            return None

import os
