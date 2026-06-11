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

import os
