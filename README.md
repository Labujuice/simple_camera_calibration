# 📷 相機角度校正與座標轉換系統 (Camera Angle Calibration & Projection)

本專案提供一套完整的相機校正與物理角度雙向轉換系統。支援 **Pinhole (針孔)** 與 **Fisheye (魚眼)** 相機模型，可將影像像素座標 $(u, v)$ 精準轉換為以相機光學中心為 $(0,0)$、向右為正、向上為正的水平角 (Yaw) 與垂直角 (Pitch) 弧度值，並支援逆向投影轉換。

> [!NOTE]
> 本專案已完全整合 C、C++ 以及 Python 的雙向轉換核心。C/C++ 核心數學公式不依賴任何外部數學庫或 OpenCV 矩陣運算，具備高移植性，適合直接運行在嵌入式 MCU（如 STM32、ESP32）或邊緣設備上。

---

## 📂 專案檔案結構 (Project Directory Structure)

```text
camera_angle_calibration/
├── generate_chessboard.py      # 步驟 1: 生成黑白棋盤格 PDF 圖紙（帶 QR Code 與標尺）
├── calibrate.py                # 步驟 2: 即時/離線相機校正程式（含主點鎖定優化算法）
├── verify.py                   # 步驟 3: Python 雙畫面對照與滑鼠懸停角度顯示驗證程式
├── camera_model.py             # Python 核心庫: 提供外部專案導入使用的 CameraModel 類別
├── calibration_transforms/     # C/C++ 雙向角度/像素轉換與驗證程式
│   ├── c/                      # C 語言版本
│   │   ├── camera_calibration.h   # 純 C 標頭檔 (API 定義)
│   │   ├── camera_calibration.c   # 純 C 數學核心 (YAML 解析 + 雙向公式)
│   │   ├── cv_wrapper.h           # OpenCV 封裝標頭檔 (C 接口)
│   │   ├── cv_wrapper.cpp         # OpenCV 封裝實現 (C++ 編譯)
│   │   ├── verify.c               # C 語言版視覺化驗證程式
│   │   └── Makefile               # 專屬編譯腳本 (輸出 verify_c)
│   └── cpp/                    # C++ 語言版本
│       ├── CameraCalibration.hpp  # C++ 類別定義檔
│       ├── CameraCalibration.cpp  # C++ 雙向公式與 YAML 解析實現
│       ├── verify.cpp             # C++ 語言版視覺化驗證程式
│       └── Makefile               # 專屬編譯腳本 (輸出 verify_cpp)
├── Calibration_Methodology.md  # 技術文件: 座標系定義、投影與畸變校正的詳細數學公式
├── Requirement.md              # 開發需求追蹤與各語言編譯相依說明
└── README.md                   # 本說明文件
```

---

## 🛠️ 安裝與相依套件 (Installation)

### 1. Python 環境需求
本專案基於 **Python 3** 開發，請先安裝影像處理與 PDF 產出套件：
```bash
pip install opencv-python pyyaml reportlab numpy
```

### 2. C/C++ 編譯相依套件 (Linux/Ubuntu 系統)
C/C++ 的驗證程式使用 OpenCV 作為影像去畸變與 UI 繪製工具，請透過以下指令安裝編譯工具與程式庫：
```bash
sudo apt-get update
sudo apt-get install build-essential libopencv-dev pkg-config
```

---

## 🚀 快速開始使用指南 (Usage Guide)

### 步驟 1. 產生 PDF 校正圖紙
執行以下指令產生 A4 landscape 比例的棋盤格校正紙：
```bash
python3 generate_chessboard.py --rows 7 --cols 8 --size 20.0 --output calibration_sheet.pdf
```

> [!IMPORTANT]
> **列印注意事項：**
> * 列印 PDF 時請選擇 **「實際大小 (100% 比例)」**，切勿選取「符合頁面大小」，否則方格實體尺寸會被縮水。
> * 列印後請以實體直尺測量左下角的標尺線，確認其長度是否剛好為 **10 cm (100 mm)**。
> * **平整度要求**：為避免紙張彎曲干涉畸變參數優化，強烈建議將校正紙使用噴膠牢固貼於**壓克力板**、**玻璃**或**硬紙板**上。

| 參數 | 預設值 | 說明 |
| :--- | :--- | :--- |
| `--rows` | `7` | 棋盤格的**方格行數**（7個方格，內部角點數為 6）。 |
| `--cols` | `8` | 棋盤格的**方格列數**（8個方格，內部角點數為 7）。 |
| `--size` | `20.0` | 每個方格的實體邊長 (mm)。 |
| `--output`| `calibration_sheet.pdf` | 產出的 PDF 檔名。 |

---

### 步驟 2. 執行相機校正
將攝影機對準印出的校正紙進行即時取樣。程式支援自動掃描 QR Code 取得方格參數，並有綠色覆蓋導引：
```bash
# 1. 針孔模型 (Pinhole)
python3 calibrate.py --model pinhole --output camera_calibration.yaml

# 2. 魚眼模型 (Fisheye)
python3 calibrate.py --model fisheye --output camera_calibration.yaml

# 3. 指定開啟解析度
python3 calibrate.py --model fisheye --width 1920 --height 1080 --output camera_calibration.yaml
```

> [!TIP]
> **主點鎖定優化算法 (Fix Principal Point)：**
> 本專案已將 `CALIB_FIX_PRINCIPAL_POINT` 整合入校正最佳化演算法中。這會將相機光學中心 $(c_x, c_y)$ 鎖定在影像物理中心 ($W/2$, $H/2$)，防止因取樣不均導致主點漂移，進而確保 **物理 0 度的位置恆落在影像的正中央**。

| 參數 | 預設值 | 說明 |
| :--- | :--- | :--- |
| `--camera` | `None` | 指定 Webcam 索引。若不指定則自動彈出可用攝影機掃描選單。 |
| `--width` | `0` | 目標攝影機寬度解析度（例如 `1920`）。 |
| `--height` | `0` | 目標攝影機高度解析度（例如 `1080`）。 |
| `--model` | `pinhole` | 相機內參模型選擇，可為 `pinhole` 或 `fisheye`。 |
| `--grid` | `4` | 畫面均勻覆蓋導引的網格數 ($N \times N$)，填滿所有綠色網格即可進入計算。 |
| `--images` | `""` | 離線校正路徑。若指定資料夾，程式會改為批次處理該資料夾內的靜態照片。 |

---

### 步驟 3. 雙畫面對比驗證

您可以任選 **Python**, **C**, 或 **C++** 版本的驗證程式來測試校正結果：

#### A. Python 版本驗證
```bash
python3 verify.py --calibration camera_calibration.yaml
```

#### B. C 語言版本編譯與驗證
```bash
cd calibration_transforms/c
make
# 執行 (可用 --image 載入單張測試照片，或不加參數直接啟動相機)
./verify_c --calibration ../../camera_calibration.yaml --image ../../test_image.png
```

#### C. C++ 語言版本編譯與驗證
```bash
cd calibration_transforms/cpp
make
# 執行
./verify_cpp --calibration ../../camera_calibration.yaml --image ../../test_image.png
```

*   **滑鼠聯動游標**：在左側（原始畸變圖）移動滑鼠，右側（去畸變拉直圖）會同步繪製出映射後像素的十字對應點；反之亦然。
*   **懸浮資訊框 (Tooltip)**：實時於滑鼠旁呈現像素位置、對應之 **Yaw 與 Pitch 物理角度**（包含弧度與角度）。
*   **角度格線 Overlay**：實時在畫面上疊加等角度格線（預設每 10 度一條，會隨畸變模型自動彎曲）。按鍵盤 **`g`** 鍵可開啟/關閉格線。
*   **可調整視窗**：視窗預設以合適比例展開，您可以自由拖曳邊框縮放視窗大小。

---

## 💻 整合開發 API 使用範例 (API Integration Guide)

本專案將內參載入、去畸變與像素/角度雙向轉換封裝成極為簡潔的 SDK。

### 1. Python API (`camera_model.py`)
```python
import numpy as np
from camera_model import CameraModel

# 載入校正檔
model = CameraModel("camera_calibration.yaml")

# 像素坐標 (u, v) 轉物理角度 (Yaw, Pitch)
u, v = 960.0, 540.0
yaw_rad, pitch_rad = model.pixel_to_angle(u, v)
print(f"Yaw: {yaw_rad:+.4f} rad ({np.degrees(yaw_rad):+.2f}°)")
print(f"Pitch: {pitch_rad:+.4f} rad ({np.degrees(pitch_rad):+.2f}°)")

# 物理角度轉回像素坐標
u_re, v_re = model.angle_to_pixel(yaw_rad, pitch_rad)
print(f"Reconstructed Pixel: ({u_re:.2f}, {v_re:.2f})")
```

### 2. C 語言 API (`camera_calibration.h`)
```c
#include <stdio.h>
#include "camera_calibration.h"

int main() {
    // 1. 初始化模型並載入校正檔
    CameraModel* model = camera_model_create("camera_calibration.yaml");
    if (!model) return 1;

    double u = 960.0, v = 540.0;
    double yaw_rad = 0.0, pitch_rad = 0.0;

    // 2. 像素轉物理角度 (回傳 0 代表成功)
    if (camera_model_pixel_to_angle(model, u, v, &yaw_rad, &pitch_rad) == 0) {
        printf("Yaw: %+.4f rad, Pitch: %+.4f rad\n", yaw_rad, pitch_rad);
    }

    // 3. 物理角度轉回像素
    double u_re = 0.0, v_re = 0.0;
    if (camera_model_angle_to_pixel(model, yaw_rad, pitch_rad, &u_re, &v_re) == 0) {
        printf("Reconstructed Pixel: (%.2f, %.2f)\n", u_re, v_re);
    }

    // 4. 釋放模型與記憶體，避免洩漏
    camera_model_destroy(model);
    return 0;
}
```

### 3. C++ 語言 API (`CameraCalibration.hpp`)
```cpp
#include <iostream>
#include "CameraCalibration.hpp"

int main() {
    try {
        // 1. 初始化模型 (建構子會自動解析 YAML)
        CameraCalibration model("camera_calibration.yaml");

        double u = 960.0, v = 540.0;
        double yaw = 0.0, pitch = 0.0;

        // 2. 像素轉物理角度
        if (model.pixel_to_angle(u, v, yaw, pitch)) {
            std::cout << "Yaw: " << yaw << " rad, Pitch: " << pitch << " rad" << std::endl;
        }

        // 3. 物理角度轉回像素
        double u_re = 0.0, v_re = 0.0;
        if (model.angle_to_pixel(yaw, pitch, u_re, v_re)) {
            std::cout << "Reconstructed: (" << u_re << ", " << v_re << ")" << std::endl;
        }
        
        // 4. 物件生命週期結束時，解構子會自動且安全地回收所有內部 OpenCV Mat 資源
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

---

## 📐 數學模型與座標系參考
詳細的投影投影矩陣公式、針孔/魚眼畸變校正公式以及 Yaw & Pitch 的物理映射關係，請參閱專案文件：
👉 [Calibration_Methodology.md](file:///home/kenny/Git_KennySpace/camera_angle_calibration/Calibration_Methodology.md)

---

## 📄 授權條款 (License)
本專案採用 [MIT License](file:///home/kenny/Git_KennySpace/camera_angle_calibration/LICENSE) 授權。

