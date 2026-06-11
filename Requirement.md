## Requirement
- 產出一個function可以將相機解析度座標轉換成實際的角度物理量
  - 角度以中心為(0,0), 向右為正、向上為正
  - YAW and PITCH
  - 使用弧度, 驗證程式可以協助轉為角度提供驗證
  - 也請同步提供角度轉實際解析度的function
- 校正手法必須簡單, 最好是 6x7/7x8 黑白方格段暫採相自動拍照後就可以計算, 校正紙需要產生PDF直接印出, 帶有QRCODE 可以識別是多少乘多少以及方格邊長
  - QR Code 的內容格式：（JSON 字串：`{"rows": 7, "cols": 8, "size_mm": 20.0}`）。
  - 放在紙張左側中間
  - 請劃上標尺線供驗證使用
- 需要撰寫一個驗證程式, 使用 python3 撰寫, 可以demo結果
  *   **去畸變 (Undistort) 即時顯示：** 可以開啟 WebCam，並排顯示「原始影像」與「去畸變後的影像」。
  *   **角度格線疊加 (Angle Grid Overlay)：** 在畫面上疊加等角度的格線（例如每 10 度一條線），方便視覺化驗證角度分佈。
  *   **滑鼠互動：** 滑鼠在畫面上移動時，即時顯示該像素對應的實際角度 `(Yaw, Pitch)`。
- 校正結果必須使用 yaml 裝起來, 並且有 pinhole and fisheye模型
  *   **YAML 格式範例：** 建議在需求中預先制定好 YAML 格式，例如：
    ```yaml
    sensor_width: 1920
    sensor_height: 1080
    camera_model: "fisheye" # or "pinhole"
    camera_matrix:
      rows: 3
      cols: 3
      data: [f_x, 0.0, c_x, 0.0, f_y, c_y, 0.0, 0.0, 1.0]
    distortion_coefficients:
      rows: 1
      cols: 4
      data: [k1, k2, k3, k4]
    ```
- 自動校正程式
  - 會即時抓取目前的校正圖紙位置, 必須即時顯示識別出來的圖紙位置加上匡
  - 畫面每個角落都需要被覆蓋到, 已經抓取過的角度可以使用綠色半透明識別
  - 當整張畫面都集滿綠色半透明的標示之後就會結束校正程式採樣, 進入計算
  - 結果直接以檔案儲存

## steps
- [ ] 校正方式研究與文件撰寫(包含格式制定)
- [ ] 方格圖片產生器
- [ ] 校正流程程式撰寫
- [ ] 驗證程式撰寫
- [ ] Readme.md 撰寫