# ⚠️ 重要：TensorFlow 環境問題

## 問題原因

您的 **Python 版本是 3.11**，但 TensorFlow 2.13 只支援 **Python 3.8-3.10**。

```
您的環境: Python 3.11.9
TensorFlow 需要: Python 3.8-3.10
```

## 🎯 最簡單的解決方案（3選1）

### 方案 1: 使用 Google Colab ⭐⭐⭐⭐⭐ 最推薦

**完全不用安裝任何東西，5 分鐘搞定！**

1. 開啟瀏覽器前往: https://colab.research.google.com/
2. 新增筆記本
3. 複製以下命令並執行：

```python
# 下載訓練腳本
!git clone --depth 1 https://github.com/tensorflow/tensorflow
!pip install tensorflow

# 設定參數並開始訓練
WANTED_WORDS = "yes,no"
TRAINING_STEPS = "12000,3000"
# ... (複製訓練腳本內容)
```

4. 啟用 GPU: Runtime → Change runtime type → GPU
5. 訓練完成後下載 `models/model.cc`

### 方案 2: 安裝 Python 3.10 ⭐⭐⭐⭐

1. **下載 Python 3.10.11**:
   - 前往: https://www.python.org/ftp/python/3.10.11/python-3.10.11-amd64.exe
   - 執行安裝，**務必勾選 "Add Python to PATH"**

2. **重新開啟 PowerShell** 並驗證：
   ```powershell
   python --version  # 應該顯示 Python 3.10.11
   ```

3. **安裝 TensorFlow**:
   ```powershell
   pip install tensorflow==2.13.0 numpy
   ```

4. **執行訓練腳本**:
   ```powershell
   cd C:\Users\yunth\OneDrive\Desktop\edge_command
   python .\python\train_micro_speech_model_windows.py
   ```

### 方案 3: 使用虛擬環境 ⭐⭐⭐

如果不想動到系統 Python，可以用虛擬環境：

```powershell
# 使用 py launcher 指定 Python 3.10（如果已安裝）
py -3.10 -m venv tf_env

# 或者先安裝 virtualenv
pip install virtualenv
python -m virtualenv -p python3.10 tf_env

# 啟動環境
.\tf_env\Scripts\Activate.ps1

# 安裝 TensorFlow
pip install tensorflow==2.13.0 numpy

# 執行訓練
python train_micro_speech_model_windows.py
```

## 📋 快速驗證

執行以下命令確認環境正確：

```powershell
python .\python\check_tensorflow.py
```

如果看到 ✅ 就代表環境正常！

## 🆘 我該選哪個？

| 情境 | 推薦方案 |
|------|---------|
| 我只是想快速訓練模型 | **方案 1: Colab** |
| 我以後會常用 TensorFlow | **方案 2: 安裝 Python 3.10** |
| 我不想改變系統 Python | **方案 3: 虛擬環境** |
| 我的電腦沒有 GPU | **方案 1: Colab（免費 GPU）** |

## 📞 需要幫助？

檢查 `TROUBLESHOOTING.md` 獲得更詳細的說明。
