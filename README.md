# 相機角度校正與座標轉換系統 (Camera Angle Calibration & Projection)

本專案提供一套完整的相機校正與物理角度轉換解決方案。支援 **Pinhole (針孔)** 與 **Fisheye (魚眼)** 相機模型，可將影像像素座標 $(u, v)$ 精準轉換為以相機光軸中心為 $(0,0)$ 的水平角 (Yaw) 與垂直角 (Pitch)，並支援逆向轉換。

---

## 🌟 核心功能

1.  **方格校正圖紙產生器 (`generate_chessboard.py`)**：自動生成包含指定行列數、方格尺寸的 PDF 圖紙，並附帶包含參數資訊的 QR Code 及 10cm 實體校正標尺。
2.  **自動/即時校正工具 (`calibrate.py`)**：
    *   **QR Code 自動配置**：鏡頭掃描圖紙左側的 QR Code 即自動設定方格參數，免手動輸入。
    *   **覆蓋率導引 (Coverage Grid)**：畫面劃分 $N \times N$ 網格，已捕捉的角度區間以綠色半透明標記，引導使用者均勻移動圖紙以提高校正精度。
    *   **防震穩定捕捉**：偵測到棋盤格穩定靜止後自動採樣，避免動態模糊干擾。
    *   **離線批量校正**：除即時 Webcam 校正外，亦支援讀取指定資料夾內的多張照片。
3.  **數學轉換庫 (`camera_model.py`)**：提供精準、雙向的 `pixel_to_angle(u, v)` 與 `angle_to_pixel(yaw, pitch)` 函數（內部角度使用弧度）。
4.  **視覺化驗證工具 (`verify.py`)**：
    *   並排顯示原始影像與去畸變 (Undistorted) 影像。
    *   疊加等角度格線（如每 10° 一條），隨畸變自動彎曲。
    *   滑鼠即時懸停互動：顯示滑鼠位置之像素座標、Yaw / Pitch（度與弧度），並在雙邊畫面上聯動標示十字對應點。

---

## 🛠️ 安裝與相依套件

本專案使用 Python 3 撰寫，需安裝以下套件：

```bash
pip install opencv-python pyyaml reportlab numpy
```

---

## 🚀 使用說明

### 步驟 1. 產生 PDF 校正圖紙
執行以下指令產生 A4 landscape 比例的校正紙：
```bash
python3 generate_chessboard.py --rows 7 --cols 8 --size 20.0 --output calibration_sheet.pdf
```
*   `--rows` 與 `--cols` 代表方格數量。
*   `--size` 代表方格邊長 (mm)。
*   請以 **100% 原始比例 (無縮放)** 列印產出的 PDF。列印後可使用尺測量左下角標尺線，確認是否剛好為 10 cm。

---

### 步驟 2. 進行相機校正
啟動 Webcam 進行即時校正：
```bash
# Pinhole (一般鏡頭)
python3 calibrate.py --model pinhole --output camera_calibration.yaml

# Fisheye (廣角/魚眼鏡頭)
python3 calibrate.py --model fisheye --output camera_calibration.yaml
```

*   **掃描 QR Code**：將圖紙的 QR Code 對準鏡頭，程式會自動讀取規格。
*   **均勻覆蓋**：移動圖紙使棋盤格中心填滿畫面的每個格線格子（變為綠色半透明）。
*   **自動捕捉**：圖紙停頓約 0.5 秒，程式判定穩定後會自動採集。
*   **熱鍵**：
    *   `ESC` 或 `q`：離開。
    *   `r`：重設目前進度。
    *   `c`：強制作業（收集 >= 3 張即可進行計算）。

*（選用）若您已有拍攝好的校正照片，可指定資料夾進行離線校正：*
```bash
python3 calibrate.py --images ./my_calibration_images --model fisheye --output camera_calibration.yaml
```

---

### 步驟 3. 驗證與 Demo 結果
執行驗證程式觀察轉換精度與去畸變效果：
```bash
python3 verify.py --calibration camera_calibration.yaml
```
*   **移動滑鼠**：在畫面上移動滑鼠，即時看到精準的物理角度 (Yaw, Pitch)。
*   **聯動十字架**：游標移到左圖（畸變）時，右圖（去畸變）會同步標示出該角度射線映射的像素點。
*   **角度格線**：預設在畫面上疊加每 10 度一圈的經緯角度格線。按下鍵盤 **`g`** 可開啟或關閉格線。

*（選用）使用靜態圖片進行驗證：*
```bash
python3 verify.py --calibration camera_calibration.yaml --image test.jpg
```

---

## 📐 數學模型與坐標系
詳細的投影方程式、畸變修正模型以及 Yaw & Pitch 的物理定義，請參閱專案文件：
👉 [Calibration_Methodology.md](file:///home/kenny/Git_KennySpace/camera_angle_calibration/Calibration_Methodology.md)
