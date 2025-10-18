# 🌐 網路連線問題解決方案

## ❌ 問題診斷

您遇到的錯誤：
```
socket.gaierror: [Errno 11001] getaddrinfo failed
Failed to download: https://storage.googleapis.com/download.tensorflow.org/data/speech_commands_v0.02.tar.gz
```

**原因**：無法連接到 Google Cloud Storage 下載數據集（~2GB）

可能原因：
1. ⚠️ 防火牆阻擋
2. ⚠️ DNS 解析失敗  
3. ⚠️ 網路代理設置
4. ⚠️ 網路不穩定

## ✅ 解決方案

### 🌟 方案 A：使用 Google Colab（最推薦！）

**優點**：
- ✅ 完全避開本機網路問題
- ✅ 免費 GPU 加速（訓練快 10 倍！）
- ✅ 不需下載 2GB 數據集到本機
- ✅ 15-20 分鐘完成訓練（vs 本機 1-2 小時）

**步驟**：

1. **開啟 Google Colab**
   ```
   瀏覽器前往：https://colab.research.google.com/
   ```

2. **新增筆記本**
   - 點選「新增筆記本」

3. **啟用 GPU（重要！）**
   ```
   Runtime → Change runtime type → Hardware accelerator: GPU → Save
   ```

4. **複製訓練代碼**
   - 開啟 `colab_training.py`
   - 全選複製（Ctrl+A, Ctrl+C）
   - 貼到 Colab 第一個 cell

5. **執行訓練**
   - 點選「執行」按鈕（或 Ctrl+Enter）
   - 等待 15-20 分鐘

6. **下載模型**
   ```
   左側選單 Files → models → 
   - 下載 model.cc（用於 ESP32）
   - 下載 model.tflite（測試用）
   ```

7. **部署到 ESP32**
   ```powershell
   # 複製下載的 model.cc 到專案
   copy Downloads\model.cc c:\Users\yunth\OneDrive\Desktop\edge_command\src\
   ```

---

### 方案 B：修復本機網路連線

如果堅持本機訓練，試試這些方法：

#### B1. 修改 DNS 設置

```powershell
# 使用 Google DNS
# 設定 → 網路和網際網路 → 變更介面卡選項 →
# 右鍵 → 內容 → IPv4 → 使用下列 DNS 伺服器位址：
# 慣用: 8.8.8.8
# 其他: 8.8.4.4
```

#### B2. 關閉防火牆/VPN

```powershell
# 暫時關閉 Windows 防火牆
# 設定 → 更新與安全性 → Windows 安全性 → 防火牆與網路保護

# 或關閉 VPN/代理
```

#### B3. 手動下載數據集

```powershell
# 1. 手動下載數據集（使用瀏覽器）
# 網址：https://storage.googleapis.com/download.tensorflow.org/data/speech_commands_v0.02.tar.gz
# 大小：~2GB

# 2. 放到正確位置
mkdir dataset
# 將下載的檔案複製到 dataset/speech_commands_v0.02.tar.gz

# 3. 解壓縮
# （訓練腳本會自動解壓）

# 4. 再次執行訓練
python .\python\train_micro_speech_model_windows.py
```

#### B4. 使用其他網路

- 手機熱點
- 公司/學校網路
- 咖啡廳 WiFi

---

### 方案 C：使用預訓練模型

如果您只是想測試 ESP32，可以使用已訓練好的模型：

```powershell
# 從 TensorFlow 官方下載預訓練模型
# https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/micro/examples/micro_speech
```

---

## 🎯 推薦決策樹

```
需要訓練模型？
├─ 是
│  ├─ 想要快速完成（15-20分鐘）
│  │  └─ → 方案 A: Google Colab ⭐⭐⭐⭐⭐
│  │
│  ├─ 需要訓練中文關鍵字
│  │  └─ → 方案 B: 修復網路 + 準備中文數據集
│  │
│  └─ 本機訓練（1-2小時）
│     └─ → 方案 B: 修復網路連線
│
└─ 否（只想測試功能）
   └─ → 方案 C: 使用預訓練模型
```

## 📝 我的建議

**強烈建議使用 Google Colab（方案 A）**

原因：
1. ✅ 完全避開網路問題
2. ✅ 訓練速度快 10 倍（GPU）
3. ✅ 不佔用本機資源
4. ✅ 完全免費
5. ✅ 操作最簡單

只需：
1. 開啟 Colab（3 秒）
2. 啟用 GPU（5 秒）
3. 複製貼上代碼（10 秒）
4. 執行並等待（15-20 分鐘）
5. 下載 model.cc（5 秒）

總共不到 30 分鐘就能完成！

---

## 🚀 立即行動

**現在就試試 Colab！**

1. 開啟：https://colab.research.google.com/
2. 開啟檔案：`colab_training.py`
3. 複製全部內容
4. 貼到 Colab
5. Runtime → Change runtime type → GPU
6. 按 Ctrl+Enter 執行
7. 等待完成！

---

## 💡 常見問題

### Q: Colab 會不會很複雜？
A: 不會！只要複製貼上代碼，按執行就好。比修復本機網路簡單多了！

### Q: Colab 免費嗎？
A: 完全免費！每天有免費的 GPU 使用額度，足夠訓練模型。

### Q: 訓練真的只要 15-20 分鐘？
A: 是的！使用 GPU 的話。CPU 需要 1-2 小時。

### Q: 我可以訓練中文嗎？
A: Colab 上訓練英文最簡單。訓練中文需要準備自己的數據集。

### Q: 我需要 Google 帳號嗎？
A: 是的，Colab 需要 Google 帳號登入。

---

## 📧 需要幫助？

如果您選擇：
- **方案 A (Colab)**：開啟 `colab_training.py`，按照註解操作
- **方案 B (修復網路)**：檢查防火牆和 DNS 設置
- **方案 C (預訓練)**：從 TensorFlow GitHub 下載

**現在就選一個方案開始吧！** 🎉

建議：**先試 Colab，5 分鐘就能開始訓練！**
