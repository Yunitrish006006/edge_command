@echo off
echo ========================================
echo TensorFlow 環境修復工具
echo ========================================
echo.

echo [步驟 1/4] 檢查 Python 版本...
python --version
echo.

echo [步驟 2/4] 卸載舊的 TensorFlow...
pip uninstall -y tensorflow tensorflow-cpu tensorflow-gpu tf-nightly
echo.

echo [步驟 3/4] 清除 pip 快取...
pip cache purge
echo.

echo [步驟 4/4] 重新安裝 TensorFlow 2.13.0...
pip install --no-cache-dir tensorflow==2.13.0
echo.

echo ========================================
echo 驗證安裝...
echo ========================================
python -c "import tensorflow as tf; print('TensorFlow 版本:', tf.__version__); print('GPU 支援:', tf.config.list_physical_devices('GPU'))"
echo.

echo ========================================
echo 完成！
echo ========================================
pause
