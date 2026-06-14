# 📷 相機角度校正與座標轉換系統 (Camera Angle Calibration & Projection)

本專案提供一套完整的相機校正與物理角度雙向轉換系統。支援 **Pinhole (針孔)** 與 **Fisheye (魚眼)** 相機模型，可將影像像素座標 $(u, v)$ 精準轉換為以相機光學中心為 $(0,0)$、向右為正、向上為正的水平角 (Yaw) 與垂直角 (Pitch) 弧度值，並支援逆向投影轉換。

---

## 📂 專案檔案結構 (Project Directory Structure)

```text
camera_angle_calibration/
├── generate_chessboard.py  # 步驟 1: 生成黑白棋盤格 PDF 圖紙（帶 QR Code 與尺規）
├── calibrate.py            # 步驟 2: 即時/離線相機校正程式（帶網格覆蓋導引與自動抓取）
├── verify.py               # 步驟 3: Python 雙畫面對照與滑鼠懸停角度顯示驗證程式
├── camera_model.py         # Python 核心庫: 提供外部專案導入使用的 CameraModel 類別
├── calibration_transforms/ # C/C++ 雙向角度/像素轉換程式與驗證程式
│   ├── c/                  # C 語言版（純 C 數學轉換核心 + OpenCV GUI 包裝）
│   └── cpp/                # C++ 語言版（物件導向設計 + OpenCV 直接載入與轉換）
├── Calibration_Methodology.md # 技術文件: 座標系定義、投影與畸變校正的詳細數學公式
├── Requirement.md          # 專案原始開發需求文件與開發進度
└── README.md               # 本說明文件
```

---

## 🛠️ 安裝與相依套件 (Installation)

本專案基於 **Python 3** 開發，僅需安裝以下標準影像處理與排版套件：

```bash
pip install opencv-python pyyaml reportlab numpy
```

---

## 🚀 快速開始使用指南 (Usage Guide)

### 步驟 1. 產生 PDF 校正圖紙
執行以下指令產生 A4 landscape 比例的校正紙：
```bash
python3 generate_chessboard.py --rows 7 --cols 8 --size 20.0 --output calibration_sheet.pdf
```
*   **參數說明**：
    *   `--rows` 與 `--cols`：設定棋盤格的**方格數量**（若為 7x8 個方格，內部角點數則為 6x7）。
    *   `--size`：設定每個方格邊長 (mm)。
    *   `--output`：產出的 PDF 檔名。
*   **⚠️ 重要列印注意事項**：
    *   列印 PDF 時請選擇 **「實際大小 (100% 比例)」**，切勿選擇「符合頁面大小」，否則方格實體尺寸會被縮小。
    *   列印完成後，請用實體直尺測量左下角的標尺線，確認其長度是否剛好為 **10 cm (100 mm)**。

---

### 步驟 2. 執行相機校正
啟動 Webcam 進行即時互動式校正：
```bash
# 1. 針孔模型 (預設 Webcam 模式)
python3 calibrate.py --model pinhole --output camera_calibration.yaml

# 2. 魚眼模型
python3 calibrate.py --model fisheye --output camera_calibration.yaml

# 3. 指定開啟的高解析度模式（如 1080p，程式會自動開啟 Webcam 最接近的支援模式）
python3 calibrate.py --model fisheye --width 1920 --height 1080 --output camera_calibration.yaml
```

*   **相機與解析度選擇選單**：若執行時未指定 `--camera` 參數，程式會先引導您選擇鏡頭，接著詢問要使用的解析度（提供預設、720p、1080p 或自訂解析度選單）。若已在命令列指定 `--width` 與 `--height`，則會自動套用該解析度而不重複詢問。
*   **自動配置參數**：將圖紙左側的 **QR Code 對準鏡頭**，校正程式會自動解析並加載棋盤格的尺寸與行列數，無須手動設定。
*   **網格覆蓋導引**：畫面會被均勻劃分網格，移動圖紙使棋盤格中心經過每個網格，成功採樣的格子會轉為**綠色半透明**。
*   **防震動自動捕捉**：當程式偵測到棋盤格靜止約 0.5 秒後，會發出成功採樣提示並記錄該幀，避免晃動造成的動態模糊。
*   **校正熱鍵**：
    *   `q` 或 `ESC`：結束程式並放棄。
    *   `r`：清空進度重來。
    *   `c`：強制作業（已收集 >= 3 張即可強制進行校正計算）。
*   **離線批量校正（選用）**：若您已有拍攝好的棋盤格照片，可指定資料夾進行離線校正：
    ```bash
    python3 calibrate.py --images ./my_images --model fisheye --output camera_calibration.yaml
    ```

---

### 步驟 3. 雙畫面對比驗證
載入校正 YAML 檔，啟動 Webcam 進行即時去畸變與角度轉換驗證：
```bash
python3 verify.py --calibration camera_calibration.yaml
```
*   **去畸變對照**：左邊為原始相機畫面（帶有畸變），右邊為去畸變校正後的畫面。
*   **聯動十字游標**：當滑鼠在左圖移動時，右圖會顯示對應去畸變後像素的十字對應點；反之亦然。
*   **懸停資訊框 (Tooltip)**：即時顯示滑鼠位置對應的**像素座標**、**Yaw / Pitch 物理角度**（同時提供度數與弧度）。
*   **角度格線 Overlay**：預設會在畫面上疊加每 10 度一條的角度格線，格線會隨畸變模型自動彎曲。可按鍵盤 **`g`** 鍵開啟或關閉格線。

---

## 💻 Python 整合開發指南 (API Integration)

您可以非常容易地將本專案的核心轉換模組 `camera_model.py` 整合到您自己的 Python 開發環境中：

```python
import numpy as np
from camera_model import CameraModel

# 1. 初始化並載入 YAML 校正檔
model = CameraModel("camera_calibration.yaml")
print(f"相機模型: {model.camera_model.upper()}")
print(f"校正解析度: {model.sensor_width}x{model.sensor_height}")

# 2. 影像像素座標轉物理角度 (Yaw, Pitch) - 單位: 弧度 (Radians)
# 定義：向右為 Yaw 正、向上為 Pitch 正，(0,0) 為光軸正中心
u, v = 400.0, 300.0
yaw_rad, pitch_rad = model.pixel_to_angle(u, v)

print(f"像素 ({u}, {v}) 對應之物理角度：")
print(f"  水平角 (Yaw)  : {yaw_rad:+.4f} rad ({np.degrees(yaw_rad):+.2f}°)")
print(f"  垂直角 (Pitch): {pitch_rad:+.4f} rad ({np.degrees(pitch_rad):+.2f}°)")

# 3. 物理角度 (Yaw, Pitch) 轉回影像像素座標 (u, v)
u_reconstructed, v_reconstructed = model.angle_to_pixel(yaw_rad, pitch_rad)
print(f"還原像素座標: ({u_reconstructed:.2f}, {v_reconstructed:.2f})")
```

---

## 🎯 獲得高精度校正的黃金法則 (Calibration Best Practices)

由於相機的**焦距、光學中心與畸變參數在數學上具有高度相關性**，若採樣數據不夠豐富，容易導致計算結果不穩定（例如每次校正出來的 `(0,0)` 光學中心位置均不同）。

本專案已在 `calibrate.py` 中引入 `CALIB_USE_INTRINSIC_GUESS` 來鎖定初始物理約束，但若要獲得極高精度的校正，採樣時請遵循以下「黃金法則」：
1.  **3D 傾斜角度（最關鍵）**：圖紙**切勿**一直與鏡頭保持平行。請刻意將圖紙向前仰、後傾、左旋、右旋（角度約 20° ~ 35°），為優化器提供足夠的透視資訊以解開焦距參數。
2.  **近與遠的距離變化**：有些照片圖紙要離鏡頭極近（佔滿畫面 70%），有些要稍微拉遠（佔畫面 30%）。
3.  **填滿邊緣與四角**：鏡頭的畸變在畫面邊緣與角落最為明顯，務必移動圖紙讓棋盤格經過影像的四個最外側角落，如此才能精準解出畸變參數。
4.  **足夠的樣本數**：建議將校正網格設定為 5x5 以上，均勻採集至少 25 張照片：
    ```bash
    python3 calibrate.py --model pinhole --grid 5
    ```

---

## 📄 YAML 校正檔格式說明 (YAML Format)

輸出的 `camera_calibration.yaml` 採用簡潔、易讀且好解析的結構：

```yaml
sensor_width: 640        # 校正時的影像寬度 (Pixels)
sensor_height: 480       # 校正時的影像高度 (Pixels)
camera_model: "pinhole"  # 相機模型 ("pinhole" 或 "fisheye")
camera_matrix:           # 相機內參矩陣 K (3x3)
  rows: 3
  cols: 3
  data: [f_x, 0.0, c_x, 0.0, f_y, c_y, 0.0, 0.0, 1.0] # f 為焦距，c 為光學中心
distortion_coefficients: # 畸變參數
  rows: 1
  cols: 5 # 針孔為 5 個參數 [k1, k2, p1, p2, k3]，魚眼為 4 個參數 [k1, k2, k3, k4]
  data: [k1, k2, p1, p2, k3]
```

---

## 💻 C/C++ 整合開發與驗證指南 (C/C++ Integration & Verification Guide)

本專案在 `calibration_transforms/` 資料夾下，為 C 與 C++ 語言分別提供了獨立的座標/角度轉換模組與視窗驗證程式。

### 核心設計亮點
1. **目錄分開放置**：C 語言位於 [c/](file:///home/kenny/Git_KennySpace/camera_angle_calibration/calibration_transforms/c)；C++ 語言位於 [cpp/](file:///home/kenny/Git_KennySpace/camera_angle_calibration/calibration_transforms/cpp)。
2. **雙向精準轉換**：不依賴 OpenCV Math 函數，核心數學計算（包含針孔/魚眼的投影與迭代法去畸變）使用標準 C 寫成，適合移植至嵌入式 MCU。
3. **無記憶體洩漏**：全面實現建構與解構的資源配對設計（C++ 類別解構子，C 語言 `create` / `destroy` 函數配對），確保重複執行不會造成記憶體累積。
4. **同等驗證程式**：使用 OpenCV 提供與 Python 相同的雙畫面聯動滑鼠懸停顯示與角度格線疊加功能。
5. **支援自訂 YAML 檔案**：可透過參數 `--calibration` 載入任意校正結果檔。

### 編譯與執行方法

#### 1. C 語言版本 (Pure C core + C++ OpenCV GUI wrapper)
*   **API 使用說明**：
    包含 [camera_calibration.h](file:///home/kenny/Git_KennySpace/camera_angle_calibration/calibration_transforms/c/camera_calibration.h) 以呼叫相關函數。
*   **編譯與執行**：
    ```bash
    cd calibration_transforms/c
    make
    # 執行驗證程式 (預設讀取 camera_calibration.yaml，亦可透過 --image 讀取測試圖檔)
    ./verify_c --calibration ../../camera_calibration.yaml --image ../../test_image.png
    ```

#### 2. C++ 語言版本 (C++ Object-Oriented)
*   **API 使用說明**：
    包含 [CameraCalibration.hpp](file:///home/kenny/Git_KennySpace/camera_angle_calibration/calibration_transforms/cpp/CameraCalibration.hpp) 並實例化 `CameraCalibration` 類別。
*   **編譯與執行**：
    ```bash
    cd calibration_transforms/cpp
    make
    # 執行驗證程式
    ./verify_cpp --calibration ../../camera_calibration.yaml --image ../../test_image.png
    ```

---

## 📐 數學模型與座標系參考
詳細的投影投影矩陣公式、針孔/魚眼畸變校正公式以及 Yaw & Pitch 的物理映射關係，請參閱專案文件：
👉 [Calibration_Methodology.md](file:///home/kenny/Git_KennySpace/camera_angle_calibration/Calibration_Methodology.md)
