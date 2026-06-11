# 相機角度校正原理與數學模型說明文件 (Calibration Methodology)

本文件說明如何將相機像素座標 $(u, v)$ 轉換為實際物理世界中的水平角 (Yaw) 與垂直角 (Pitch)，以及其逆向轉換的數學原理。本專案支援 **Pinhole (針孔)** 與 **Fisheye (魚眼)** 兩種相機模型。

---

## 1. 座標系定義 (Coordinate System)

我們使用 OpenCV 的相機座標系定義：
*   **相機光軸方向：** 為 $+Z$ 軸（向前）。
*   **水平方向：** 為 $+X$ 軸（向右為正）。
*   **垂直方向：** 為 $+Y$ 軸（向下為正）。
*   **影像像素座標 $(u, v)$：** 左上角為 $(0, 0)$，向右為 $+u$，向下為 $+v$。

### 物理角度定義 (Yaw & Pitch)
使用者要求的角度定義為：**「以中心為 $(0, 0)$，向右為正、向上為正」**：
*   **水平角 $\theta_{\text{yaw}}$ (Yaw)：** 向右為正，向左為負。
*   **垂直角 $\theta_{\text{pitch}}$ (Pitch)：** 向上為正，向下為負。

---

## 2. 角度與歸一化座標的轉換 (Angle <-> Normalized Coordinates)

不論使用何種相機模型，**物理角度僅與相機空間中的 3D 射線方向相關**。
假設三維空間中從光學中心出發的射線向量為 $(X, Y, Z)$，其在影像平面上的歸一化座標（去畸變後）為：
$$x' = \frac{X}{Z}, \quad y' = \frac{Y}{Z}$$

### 2.1 像素座標 $(u,v)$ 轉物理角度 $(\theta_{\text{yaw}}, \theta_{\text{pitch}})$
1.  **去畸變與歸一化：** 將像素座標 $(u, v)$ 透過相機內參及畸變參數轉換為去畸變後的歸一化座標 $(x', y')$。
2.  **計算角度：**
    *   **水平角 (Yaw)：**
        $$\theta_{\text{yaw}} = \text{atan2}(X, Z) = \text{atan}(x')$$
    *   **垂直角 (Pitch)：** 因為相機座標系中 $+Y$ 朝下，而使用者定義向上為正，因此需要取負號：
        $$\theta_{\text{pitch}} = \text{atan2}(-Y, \sqrt{X^2 + Z^2}) = \text{atan}\left(\frac{-y'}{\sqrt{x'^2 + 1}}\right)$$

### 2.2 物理角度 $(\theta_{\text{yaw}}, \theta_{\text{pitch}})$ 轉像素座標 $(u,v)$
給定 $(\theta_{\text{yaw}}, \theta_{\text{pitch}})$ 弧度值：
1.  **求歸一化座標 $(x', y')$：**
    *   由 $\theta_{\text{yaw}} = \text{atan}(x')$ 可得：
        $$x' = \tan(\theta_{\text{yaw}})$$
    *   由 $\theta_{\text{pitch}} = \text{atan2}(-Y, \sqrt{X^2 + Z^2})$ 且令 $Z = 1$：
        $$y' = -\frac{\tan(\theta_{\text{pitch}})}{\cos(\theta_{\text{yaw}})}$$
2.  **套用畸變模型與內參：** 將 $(x', y')$ 代入對應相機模型的畸變公式，求得畸變後的座標 $(x'', y'')$，再乘上內參矩陣即可得到像素座標 $(u, v)$。

---

## 3. 相機模型與畸變模型 (Camera Models & Distortion)

### 3.1 Pinhole 模型 (針孔模型)
針孔模型的畸變包含徑向畸變 (Radial) 與切向畸變 (Tangential)。
設歸一化座標為 $(x', y')$，半徑平方 $r^2 = x'^2 + y'^2$。

#### 畸變公式：
$$x'' = x' (1 + k_1 r^2 + k_2 r^4 + k_3 r^6) + 2 p_1 x' y' + p_2 (r^2 + 2 x'^2)$$
$$y'' = y' (1 + k_1 r^2 + k_2 r^4 + k_3 r^6) + p_1 (r^2 + 2 y'^2) + 2 p_2 x' y'$$

#### 像素投影：
$$u = f_x x'' + c_x$$
$$v = f_y y'' + c_y$$

*   **內參矩陣 (Camera Matrix)：**
    $$K = \begin{bmatrix} f_x & 0 & c_x \\ 0 & f_y & c_y \\ 0 & 0 & 1 \end{bmatrix}$$
*   **畸變係數 (Distortion Coefficients)：** $[k_1, k_2, p_1, p_2, k_3]$

---

### 3.2 Fisheye 模型 (魚眼模型)
魚眼模型採用等距投影模型 (Equidistant projection)。
設歸一化座標為 $(x', y')$，半徑 $r = \sqrt{x'^2 + y'^2}$，入射角 $\theta = \text{atan}(r)$。

#### 畸變公式：
$$\theta_d = \theta (1 + k_1 \theta^2 + k_2 \theta^4 + k_3 \theta^6 + k_4 \theta^8)$$
$$x'' = \frac{\theta_d}{r} x', \quad y'' = \frac{\theta_d}{r} y' \quad (\text{若 } r = 0 \text{ 則 } x''=0, y''=0)$$

#### 像素投影：
$$u = f_x x'' + c_x$$
$$v = f_y y'' + c_y$$

*   **內參矩陣 (Camera Matrix)：** 同 Pinhole。
*   **畸變係數 (Distortion Coefficients)：** $[k_1, k_2, k_3, k_4]$

---

## 4. 實作 API 介面規劃 (API Design)

`camera_model.py` 中實作了 `CameraModel` 類別，提供以下核心 API：

### 4.1 像素轉角度
```python
def pixel_to_angle(self, u: float, v: float) -> tuple[float, float]:
    """
    將像素座標 (u, v) 轉換為物理角度 (yaw, pitch) 弧度值。
    1. 使用 cv2.undistortPoints (Pinhole) 或 cv2.fisheye.undistortPoints (Fisheye) 取得歸一化座標 (x', y')。
    2. 計算 yaw = atan(x')
    3. 計算 pitch = atan(-y' / cos(yaw))
    """
```

### 4.2 角度轉像素
```python
def angle_to_pixel(self, yaw: float, pitch: float) -> tuple[float, float]:
    """
    將物理角度 (yaw, pitch) 弧度值轉換為相機像素座標 (u, v)。
    1. 計算 x' = tan(yaw)
    2. 計算 y' = -tan(pitch) / cos(yaw)
    3. 根據相機模型呼叫 cv2.projectPoints 或 cv2.fisheye.projectPoints 得到 (u, v)
    """
```
