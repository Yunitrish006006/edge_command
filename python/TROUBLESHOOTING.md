# TensorFlow 安裝問題解決指南

## 🔴 錯誤訊息

```
ModuleNotFoundError: No module named 'tensorflow.python'
```

## 🔍 問題診斷

您遇到的這個錯誤通常由以下原因造成：

### 1. **Python 版本不相容** ⚠️ 最常見原因

您使用的是 **Python 3.11**，但 TensorFlow 2.13.0 只支援 **Python 3.8-3.10**。

```
您的 Python: Python 3.11 (從錯誤路徑可以看出)
TensorFlow 需要: Python 3.8-3.10
```

### 2. TensorFlow 安裝損壞

可能安裝過程中出現問題，導致模組不完整。

### 3. 多個 Python 環境衝突

系統可能有多個 Python 版本，pip 安裝到了錯誤的位置。

## ✅ 解決方案

### 方案 A: 安裝 Python 3.10（推薦）

#### 步驟 1: 下載 Python 3.10

1. 前往 https://www.python.org/downloads/release/python-31011/
2. 下載 **Windows installer (64-bit)**
3. 安裝時勾選 "Add Python to PATH"

#### 步驟 2: 安裝 TensorFlow

開啟新的 PowerShell 視窗：

```powershell
# 確認 Python 版本
python --version  # 應該顯示 Python 3.10.x

# 升級 pip
python -m pip install --upgrade pip

# 安裝 TensorFlow
pip install tensorflow==2.13.0 numpy

# 驗證安裝
python -c "import tensorflow as tf; print(tf.__version__)"
```

### 方案 B: 使用 Conda 虛擬環境（推薦進階使用者）

如果您有 Anaconda 或 Miniconda：

```powershell
# 創建 Python 3.10 環境
conda create -n tf213 python=3.10 -y

# 啟動環境
conda activate tf213

# 安裝 TensorFlow
pip install tensorflow==2.13.0 numpy

# 在此環境中運行訓練腳本
python train_micro_speech_model_windows.py
```

### 方案 C: 修復現有安裝（如果您想繼續使用 Python 3.11）

⚠️ 注意：TensorFlow 官方不支援 Python 3.11，可能會有問題

```powershell
# 完全卸載
pip uninstall -y tensorflow tensorflow-cpu tensorflow-gpu tf-nightly

# 清除快取
pip cache purge

# 嘗試安裝最新版本（可能支援 Python 3.11）
pip install tensorflow

# 或嘗試 TensorFlow 2.15+
pip install tensorflow==2.15.0
```

### 方案 D: 使用 Google Colab（最簡單）

如果本地環境問題太多，可以直接使用 Google Colab：

1. 前往 https://colab.research.google.com/
2. 上傳原始的 `train_micro_speech_model.py`
3. 在 Colab 中執行（已預裝 TensorFlow）
4. 啟用 GPU 加速：Runtime → Change runtime type → GPU
5. 訓練完成後下載 `models/model.cc`

## 🔧 快速診斷工具

我已經為您創建了診斷腳本：

```powershell
# 執行環境檢查
python check_tensorflow.py
```

或執行批次腳本（Windows）：

```powershell
.\fix_tensorflow.bat
```

## 📋 檢查清單

在運行訓練腳本前，請確認：

- [ ] Python 版本是 3.8-3.10（執行 `python --version`）
- [ ] TensorFlow 已正確安裝（執行 `pip list | findstr tensorflow`）
- [ ] 可以導入 TensorFlow（執行 `python -c "import tensorflow"`）
- [ ] 有足夠的硬碟空間（至少 10GB 用於數據集和模型）

## 🎯 推薦方案總結

| 方案 | 難度 | 時間 | 推薦度 |
|------|------|------|--------|
| 安裝 Python 3.10 | ⭐⭐ | 30分鐘 | ⭐⭐⭐⭐⭐ 最推薦 |
| 使用 Conda 環境 | ⭐⭐⭐ | 20分鐘 | ⭐⭐⭐⭐ |
| 使用 Google Colab | ⭐ | 5分鐘 | ⭐⭐⭐⭐⭐ 最簡單 |
| 修復現有安裝 | ⭐⭐⭐⭐ | 不定 | ⭐⭐ 不推薦 |

## 💻 詳細操作步驟（方案 A）

### 1. 安裝 Python 3.10

```powershell
# 1. 下載 Python 3.10.11
# https://www.python.org/ftp/python/3.10.11/python-3.10.11-amd64.exe

# 2. 執行安裝程式
# - 勾選 "Add Python 3.10 to PATH"
# - 選擇 "Customize installation"
# - 確保勾選 "pip" 和 "py launcher"

# 3. 驗證安裝
python --version
# 應該顯示: Python 3.10.11
```

### 2. 創建專案虛擬環境（可選但推薦）

```powershell
cd C:\Users\yunth\OneDrive\Desktop\edge_command\python

# 創建虛擬環境
python -m venv tf_env

# 啟動虛擬環境
.\tf_env\Scripts\Activate.ps1

# 如果遇到權限問題，執行：
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### 3. 安裝依賴

```powershell
# 升級 pip
python -m pip install --upgrade pip

# 安裝 TensorFlow 和相關套件
pip install tensorflow==2.13.0
pip install numpy

# 驗證安裝
python -c "import tensorflow as tf; print('✅ TensorFlow', tf.__version__)"
```

### 4. 執行訓練腳本

```powershell
python train_micro_speech_model_windows.py
```

## 🔍 驗證步驟

執行以下命令確認一切正常：

```powershell
# 1. 檢查 Python 版本
python --version

# 2. 檢查已安裝的套件
pip list

# 3. 測試 TensorFlow
python -c "import tensorflow as tf; print(tf.__version__); print(tf.constant([1,2,3]))"

# 4. 執行環境檢查腳本
python check_tensorflow.py
```

預期輸出：
```
Python 3.10.11
tensorflow                 2.13.0
tf.Tensor([1 2 3], shape=(3,), dtype=int32)
```

## 📞 還是不行？

如果以上方法都不行，請提供以下資訊：

```powershell
# 執行以下命令並複製輸出
python --version
pip list
python -c "import sys; print(sys.executable)"
python -c "import site; print(site.getsitepackages())"
```

## 🎓 學習資源

- [TensorFlow 官方安裝指南](https://www.tensorflow.org/install)
- [Python 虛擬環境教學](https://docs.python.org/3/tutorial/venv.html)
- [Google Colab 使用教學](https://colab.research.google.com/)

---

**下一步**: 解決環境問題後，參考 `README_TRAINING.md` 開始訓練您的模型！
