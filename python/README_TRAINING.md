# TensorFlow Lite 語音模型訓練指南

## 📋 概述

這個資料夾包含用於訓練 TFLite Micro Speech 模型的腳本，可以訓練自定義關鍵字識別模型並部署到 ESP32。

## 📁 檔案說明

- **`train_micro_speech_model.py`** - 原始 Colab Notebook 轉換檔案（不建議直接使用）
- **`train_micro_speech_model_windows.py`** - Windows 環境適用版本 ✅ **推薦使用**

## 🔧 環境需求

### 1. Python 環境
```bash
python --version  # 建議 Python 3.8-3.10
```

### 2. 安裝 TensorFlow
```bash
pip install tensorflow==2.13.0
pip install numpy
```

### 3. 安裝 Git（用於下載 TensorFlow 倉庫）
- 從 https://git-scm.com/download/win 下載安裝

### 4. 下載訓練資料集

**Google Speech Commands v0.02** 資料集 (~2GB) 不包含在此倉庫中。

📄 **請參閱 [DATASET_LINK.md](../DATASET_LINK.md)** 取得下載連結和說明。

訓練腳本會嘗試自動下載，或您可以手動下載並解壓到 `dataset/` 資料夾。

## 🚀 使用方法

### 快速開始（使用預設英文關鍵字）

```bash
cd c:\Users\yunth\OneDrive\Desktop\edge_command\python
python train_micro_speech_model_windows.py
```

這將訓練識別 "yes" 和 "no" 兩個英文關鍵字的模型。

### 自定義關鍵字

編輯 `train_micro_speech_model_windows.py` 第 17 行：

```python
# 可選的英文關鍵字
WANTED_WORDS = "yes,no,up,down"  # 從這些選項選擇
# 完整選項: yes,no,up,down,left,right,on,off,stop,go
```

### 訓練中文關鍵字

如果要訓練中文關鍵字（如 "你好"、"是"、"否"），需要：

1. **準備數據集**：
   - 每個關鍵字錄製 100-200 個音訊樣本
   - 格式：16kHz, 16-bit, 單聲道 WAV
   - 每個樣本 1 秒長

2. **組織數據集結構**：
   ```
   dataset/
   ├── 你好/
   │   ├── sample_001.wav
   │   ├── sample_002.wav
   │   └── ...
   ├── 是/
   │   └── ...
   └── 否/
       └── ...
   ```

3. **修改腳本**：
   ```python
   WANTED_WORDS = "你好,是,否,開,關"
   DATA_URL = ""  # 留空，使用本地數據集
   ```

## 📊 訓練流程

腳本會自動執行以下步驟：

```
[步驟 1/6] 創建工作目錄
  - dataset/  (音訊數據集)
  - logs/     (訓練日誌)
  - train/    (訓練檢查點)
  - models/   (輸出模型)

[步驟 2/6] 下載 TensorFlow 倉庫
  - 從 GitHub 克隆 tensorflow 倉庫
  - 包含訓練和轉換腳本

[步驟 3/6] 訓練模型
  - 下載 Google Speech Commands 數據集
  - 訓練 tiny_conv 模型
  - 時間：1-2 小時（CPU）

[步驟 4/6] 凍結模型
  - 將訓練結果轉換為推理格式
  - 生成 saved_model

[步驟 5/6] 轉換為 TFLite
  - 生成浮點模型 (float32)
  - 生成量化模型 (int8) - 用於 ESP32
  - 測試模型準確度

[步驟 6/6] 生成 C 陣列
  - 將 .tflite 轉換為 .cc 檔案
  - 可直接用於 ESP32 專案
```

## 📦 輸出檔案

訓練完成後會在 `models/` 目錄生成：

| 檔案 | 說明 | 大小 | 用途 |
|------|------|------|------|
| `float_model.tflite` | 浮點模型 | ~20KB | 測試用 |
| `model.tflite` | 量化模型 | ~18KB | ESP32 部署 |
| `model.cc` | C 陣列 | ~18KB | 微控制器源碼 |

## 🔌 部署到 ESP32

### 1. 複製模型檔案

```bash
# 將生成的 C 陣列複製到你的專案
copy models\model.cc c:\Users\yunth\OneDrive\Desktop\edge_command\src\keyword_model_data.cc
```

### 2. 更新模型設定

編輯 `src/keyword_model.h`：

```cpp
// 更新類別數量
#define CATEGORY_COUNT 4  // silence, unknown, yes, no

// 更新類別標籤
const char* CATEGORY_LABELS[CATEGORY_COUNT] = {
    "silence",
    "unknown", 
    "yes",
    "no"
};
```

### 3. 編譯並上傳

```bash
cd c:\Users\yunth\OneDrive\Desktop\edge_command
pio run -t upload
```

## ⚙️ 訓練參數調整

### 基本參數（在腳本中修改）

```python
# 訓練步數（越多越好，但時間更長）
TRAINING_STEPS = "12000,3000"  # 預設 15000 步

# 學習率
LEARNING_RATE = "0.001,0.0001"  # 分段學習率

# 模型架構
MODEL_ARCHITECTURE = 'tiny_conv'  # 適合微控制器的小型模型
```

### 進階參數

```python
# 音訊參數
SAMPLE_RATE = 16000        # 採樣率 (Hz)
WINDOW_SIZE_MS = 30.0      # 視窗大小 (毫秒)
WINDOW_STRIDE = 20         # 步進 (毫秒)
FEATURE_BIN_COUNT = 40     # 特徵數量

# 數據增強
BACKGROUND_FREQUENCY = 0.8      # 背景噪音頻率
BACKGROUND_VOLUME_RANGE = 0.1   # 背景音量範圍
TIME_SHIFT_MS = 100.0           # 時間偏移 (毫秒)
```

## 🐛 常見問題

### Q1: 訓練時間太長怎麼辦？

**A:** 
- 減少訓練步數：`TRAINING_STEPS = "6000,1500"`
- 使用 GPU 訓練（需要 CUDA 支援）
- 使用 Google Colab（免費 GPU）

### Q2: 記憶體不足錯誤

**A:**
- 關閉其他程式
- 減少批次大小（修改 train.py 的 batch_size）
- 使用較小的數據集

### Q3: 模型準確度太低

**A:**
- 增加訓練步數
- 收集更多訓練數據
- 調整數據增強參數
- 檢查數據集品質

### Q4: 如何在 Google Colab 訓練？

**A:**
1. 上傳原始 `train_micro_speech_model.py` 到 Colab
2. 將檔案重命名為 `.ipynb`
3. 啟用 GPU 加速（Runtime → Change runtime type → GPU）
4. 逐個 cell 執行

### Q5: 中文關鍵字如何訓練？

**A:**
目前 Google Speech Commands 只包含英文。訓練中文需要：
1. 自己錄製中文音訊數據集
2. 或使用開源中文數據集（如 AISHELL-1）
3. 參考上面"訓練中文關鍵字"章節

## 📊 模型性能指標

使用預設參數訓練的模型：

| 指標 | 數值 |
|------|------|
| 模型大小 | ~18KB |
| RAM 使用 | ~30KB |
| 推理時間 | <100ms (ESP32) |
| 準確度 | ~85% (測試集) |
| 輸入大小 | 1960 (40×49) |

## 🔗 參考資源

- [TensorFlow Lite Micro 官方文檔](https://www.tensorflow.org/lite/microcontrollers)
- [Google Speech Commands 數據集](https://ai.googleblog.com/2017/08/launching-speech-commands-dataset.html)
- [ESP32 TFLite Micro 教學](https://www.xpstem.com/article/10476)
- [TFLite Micro GitHub](https://github.com/tensorflow/tflite-micro)

## 📝 版本資訊

- **腳本版本**: 1.0
- **TensorFlow**: 2.13.0
- **目標平台**: ESP32-S3
- **最後更新**: 2025-10-11

## 💡 下一步

訓練完成後，建議：

1. **測試模型**：在 ESP32 上測試實際效果
2. **優化參數**：根據測試結果調整訓練參數
3. **數據增強**：增加更多訓練樣本提升準確度
4. **持續迭代**：不斷優化直到滿意為止

## 📧 支援

如果遇到問題，可以：
- 查看 TensorFlow 官方文檔
- 在 GitHub Issues 提問
- 參考中文教程網站

---

**祝訓練順利！** 🎉
