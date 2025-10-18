# -*- coding: utf-8 -*-
"""
快速測試腳本 - 驗證環境是否正確設置
"""

import sys
import os

print("=" * 70)
print("  環境驗證測試")
print("=" * 70)

# 1. Python 版本
print(f"\n✅ Python 版本: {sys.version}")
print(f"✅ Python 執行器: {sys.executable}")

# 2. TensorFlow
try:
    import tensorflow as tf
    print(f"✅ TensorFlow 版本: {tf.__version__}")
    
    # 簡單測試
    test_tensor = tf.constant([1, 2, 3])
    print(f"✅ TensorFlow 運算測試: {test_tensor.numpy()}")
    
except ImportError as e:
    print(f"❌ TensorFlow 導入失敗: {e}")
    sys.exit(1)

# 3. NumPy
try:
    import numpy as np
    print(f"✅ NumPy 版本: {np.__version__}")
except ImportError as e:
    print(f"❌ NumPy 導入失敗: {e}")
    sys.exit(1)

# 4. 檢查目錄
print(f"\n當前工作目錄: {os.getcwd()}")

# 5. 檢查 Git
import subprocess
try:
    result = subprocess.run(['git', '--version'], capture_output=True, text=True)
    print(f"✅ Git: {result.stdout.strip()}")
except:
    print("⚠️  Git 未安裝（下載 TensorFlow 倉庫時需要）")

print("\n" + "=" * 70)
print("  環境檢查完成！可以開始訓練")
print("=" * 70)
print("\n執行訓練腳本:")
print(f"  {sys.executable} train_micro_speech_model_windows.py")
