# -*- coding: utf-8 -*-
"""
TensorFlow 環境檢查和安裝腳本
"""

import sys
import subprocess
import importlib.util

def check_package(package_name):
    """檢查套件是否已安裝"""
    spec = importlib.util.find_spec(package_name)
    return spec is not None

def install_package(package_name):
    """安裝套件"""
    print(f"\n正在安裝 {package_name}...")
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", package_name])
        return True
    except subprocess.CalledProcessError:
        return False

def main():
    print("=" * 70)
    print("  TensorFlow 環境檢查工具")
    print("=" * 70)
    
    # 檢查 Python 版本
    print(f"\nPython 版本: {sys.version}")
    py_version = sys.version_info
    
    if py_version.major == 3 and py_version.minor >= 11:
        print("\n⚠️  警告: Python 3.11+ 可能與 TensorFlow 2.13 不相容")
        print("建議使用 Python 3.8-3.10")
        print("\n解決方案:")
        print("1. 下載並安裝 Python 3.10: https://www.python.org/downloads/")
        print("2. 或使用虛擬環境:")
        print("   conda create -n tf213 python=3.10")
        print("   conda activate tf213")
        return
    
    # 檢查必要套件
    packages = {
        'tensorflow': 'tensorflow==2.13.0',
        'numpy': 'numpy',
    }
    
    print("\n檢查套件安裝狀態...")
    all_installed = True
    
    for package, install_name in packages.items():
        if check_package(package):
            print(f"✅ {package} 已安裝")
            try:
                mod = __import__(package)
                if hasattr(mod, '__version__'):
                    print(f"   版本: {mod.__version__}")
            except:
                pass
        else:
            print(f"❌ {package} 未安裝")
            all_installed = False
    
    if not all_installed:
        print("\n需要安裝缺少的套件...")
        response = input("是否自動安裝? (y/n): ")
        if response.lower() == 'y':
            for package, install_name in packages.items():
                if not check_package(package):
                    if not install_package(install_name):
                        print(f"❌ {package} 安裝失敗")
                        return
            print("\n✅ 所有套件安裝完成")
        else:
            print("\n請手動安裝:")
            print("pip install tensorflow==2.13.0 numpy")
            return
    
    # 測試 TensorFlow
    print("\n測試 TensorFlow...")
    try:
        import tensorflow as tf
        print(f"✅ TensorFlow 版本: {tf.__version__}")
        print(f"✅ GPU 支援: {len(tf.config.list_physical_devices('GPU')) > 0}")
        
        # 簡單測試
        test_tensor = tf.constant([1, 2, 3])
        print(f"✅ 基本運算測試通過: {test_tensor.numpy()}")
        
        print("\n" + "=" * 70)
        print("  環境檢查完成！可以開始訓練模型")
        print("=" * 70)
        
    except Exception as e:
        print(f"❌ TensorFlow 測試失敗: {e}")
        print("\n建議:")
        print("1. 卸載並重新安裝 TensorFlow:")
        print("   pip uninstall tensorflow")
        print("   pip install tensorflow==2.13.0")
        print("2. 檢查是否有衝突的套件")
        print("3. 考慮使用虛擬環境")

if __name__ == "__main__":
    main()
