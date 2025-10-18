# -*- coding: utf-8 -*-
"""
TensorFlow Lite Micro Speech Model Training Script
Windows 環境適用版本

這個腳本用於訓練關鍵字語音識別模型，適用於 ESP32 等微控制器
"""

import os
import sys
import subprocess

# 延遲導入 TensorFlow 以先檢查環境
try:
    import tensorflow as tf
    import numpy as np
    TF_AVAILABLE = True
except ImportError as e:
    print("=" * 70)
    print("  ⚠️  TensorFlow 導入失敗")
    print("=" * 70)
    print(f"\n錯誤: {e}")
    print("\n可能的原因:")
    print("1. TensorFlow 未正確安裝")
    print("2. Python 版本不相容（需要 Python 3.8-3.10）")
    print("3. TensorFlow 安裝損壞")
    print("\n解決方案:")
    print("執行以下命令修復:")
    print("  python check_tensorflow.py")
    print("\n或手動修復:")
    print("  pip uninstall tensorflow")
    print("  pip install tensorflow==2.13.0")
    print("=" * 70)
    TF_AVAILABLE = False
    sys.exit(1)

# ============================================================================
# 配置參數 - 可以根據需求修改
# ============================================================================

# 要訓練的關鍵字（逗號分隔）
# 英文選項: yes,no,up,down,left,right,on,off,stop,go
# 中文需要自己準備數據集
WANTED_WORDS = "yes,no"

# 訓練步數和學習率（逗號分隔，對應不同階段）
TRAINING_STEPS = "12000,3000"
LEARNING_RATE = "0.001,0.0001"

# 計算總訓練步數
TOTAL_STEPS = str(sum(map(lambda string: int(string), TRAINING_STEPS.split(","))))

print("=" * 70)
print("訓練配置:")
print(f"  關鍵字: {WANTED_WORDS}")
print(f"  訓練步數: {TRAINING_STEPS}")
print(f"  學習率: {LEARNING_RATE}")
print(f"  總步數: {TOTAL_STEPS}")
print("=" * 70)

# ============================================================================
# 固定參數 - 不建議修改
# ============================================================================

# 計算 'silence' 和 'unknown' 的訓練樣本百分比
number_of_labels = WANTED_WORDS.count(',') + 1
number_of_total_labels = number_of_labels + 2  # 包含 'silence' 和 'unknown'
equal_percentage_of_training_samples = int(100.0 / number_of_total_labels)
SILENT_PERCENTAGE = equal_percentage_of_training_samples
UNKNOWN_PERCENTAGE = equal_percentage_of_training_samples

# 模型架構參數
PREPROCESS = 'micro'
WINDOW_STRIDE = 20
MODEL_ARCHITECTURE = 'tiny_conv'  # 選項: single_fc, conv, low_latency_conv, low_latency_svdf, tiny_embedding_conv

# 訓練參數
VERBOSITY = 'INFO'
EVAL_STEP_INTERVAL = '1000'
SAVE_STEP_INTERVAL = '1000'

# 目錄路徑
DATASET_DIR = 'dataset/'
LOGS_DIR = 'logs/'
TRAIN_DIR = 'train/'
MODELS_DIR = 'models'
TF_DIR = 'tensorflow'

# 模型檔案路徑
MODEL_TF = os.path.join(MODELS_DIR, 'model.pb')
MODEL_TFLITE = os.path.join(MODELS_DIR, 'model.tflite')
FLOAT_MODEL_TFLITE = os.path.join(MODELS_DIR, 'float_model.tflite')
MODEL_TFLITE_MICRO = os.path.join(MODELS_DIR, 'model.cc')
SAVED_MODEL = os.path.join(MODELS_DIR, 'saved_model')

# 量化參數
QUANT_INPUT_MIN = 0.0
QUANT_INPUT_MAX = 26.0
QUANT_INPUT_RANGE = QUANT_INPUT_MAX - QUANT_INPUT_MIN

# 音訊處理參數
SAMPLE_RATE = 16000
CLIP_DURATION_MS = 1000
WINDOW_SIZE_MS = 30.0
FEATURE_BIN_COUNT = 40
BACKGROUND_FREQUENCY = 0.8
BACKGROUND_VOLUME_RANGE = 0.1
TIME_SHIFT_MS = 100.0

DATA_URL = 'https://storage.googleapis.com/download.tensorflow.org/data/speech_commands_v0.02.tar.gz'
VALIDATION_PERCENTAGE = 10
TESTING_PERCENTAGE = 10

# ============================================================================
# 工具函數
# ============================================================================

def run_command(cmd, description):
    """執行命令並顯示結果"""
    print(f"\n[執行] {description}")
    print(f"[命令] {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    try:
        result = subprocess.run(cmd, shell=True, check=True, capture_output=True, text=True)
        if result.stdout:
            print(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"[錯誤] {e}")
        if e.stderr:
            print(e.stderr)
        return False

def create_directories():
    """創建必要的目錄"""
    dirs = [MODELS_DIR, DATASET_DIR, LOGS_DIR, TRAIN_DIR]
    for d in dirs:
        if not os.path.exists(d):
            os.makedirs(d)
            print(f"[創建] 目錄: {d}")

def check_tensorflow_repo():
    """檢查 TensorFlow 倉庫是否存在"""
    if not os.path.exists(TF_DIR):
        print("\n[下載] TensorFlow 倉庫...")
        return run_command(
            f"git clone --depth 1 https://github.com/tensorflow/tensorflow {TF_DIR}",
            "下載 TensorFlow 源碼"
        )
    else:
        print(f"[存在] TensorFlow 倉庫已存在: {TF_DIR}")
        return True

# ============================================================================
# 主要訓練流程
# ============================================================================

def train_model():
    """執行模型訓練"""
    train_script = os.path.join(TF_DIR, "tensorflow", "examples", "speech_commands", "train.py")
    
    if not os.path.exists(train_script):
        print(f"[錯誤] 找不到訓練腳本: {train_script}")
        return False
    
    # 使用當前 Python 執行器（支援虛擬環境）
    python_exe = sys.executable
    
    cmd = [
        python_exe, train_script,
        f"--data_dir={DATASET_DIR}",
        f"--wanted_words={WANTED_WORDS}",
        f"--silence_percentage={SILENT_PERCENTAGE}",
        f"--unknown_percentage={UNKNOWN_PERCENTAGE}",
        f"--preprocess={PREPROCESS}",
        f"--window_stride={WINDOW_STRIDE}",
        f"--model_architecture={MODEL_ARCHITECTURE}",
        f"--how_many_training_steps={TRAINING_STEPS}",
        f"--learning_rate={LEARNING_RATE}",
        f"--train_dir={TRAIN_DIR}",
        f"--summaries_dir={LOGS_DIR}",
        f"--verbosity={VERBOSITY}",
        f"--eval_step_interval={EVAL_STEP_INTERVAL}",
        f"--save_step_interval={SAVE_STEP_INTERVAL}"
    ]
    
    return run_command(cmd, "訓練模型")

def freeze_model():
    """凍結模型為推理格式"""
    freeze_script = os.path.join(TF_DIR, "tensorflow", "examples", "speech_commands", "freeze.py")
    
    if not os.path.exists(freeze_script):
        print(f"[錯誤] 找不到凍結腳本: {freeze_script}")
        return False
    
    checkpoint = os.path.join(TRAIN_DIR, f"{MODEL_ARCHITECTURE}.ckpt-{TOTAL_STEPS}")
    
    # 使用當前 Python 執行器（支援虛擬環境）
    python_exe = sys.executable
    
    cmd = [
        python_exe, freeze_script,
        f"--wanted_words={WANTED_WORDS}",
        f"--window_stride_ms={WINDOW_STRIDE}",
        f"--preprocess={PREPROCESS}",
        f"--model_architecture={MODEL_ARCHITECTURE}",
        f"--start_checkpoint={checkpoint}",
        "--save_format=saved_model",
        f"--output_file={SAVED_MODEL}"
    ]
    
    return run_command(cmd, "凍結模型")

def convert_to_tflite():
    """將模型轉換為 TFLite 格式（含量化）"""
    print("\n[轉換] 生成 TensorFlow Lite 模型...")
    
    try:
        # 添加路徑以導入 TensorFlow 的語音處理模組
        tf_speech_path = os.path.join(TF_DIR, "tensorflow", "examples", "speech_commands")
        if tf_speech_path not in sys.path:
            sys.path.append(tf_speech_path)
        
        import input_data
        import models
        
        # 準備模型設置
        model_settings = models.prepare_model_settings(
            len(input_data.prepare_words_list(WANTED_WORDS.split(','))),
            SAMPLE_RATE, CLIP_DURATION_MS, WINDOW_SIZE_MS,
            WINDOW_STRIDE, FEATURE_BIN_COUNT, PREPROCESS
        )
        
        # 創建音訊處理器
        audio_processor = input_data.AudioProcessor(
            DATA_URL, DATASET_DIR,
            SILENT_PERCENTAGE, UNKNOWN_PERCENTAGE,
            WANTED_WORDS.split(','), VALIDATION_PERCENTAGE,
            TESTING_PERCENTAGE, model_settings, LOGS_DIR
        )
        
        with tf.compat.v1.Session() as sess:
            # 生成浮點模型
            print("[轉換] Float 模型...")
            float_converter = tf.lite.TFLiteConverter.from_saved_model(SAVED_MODEL)
            float_tflite_model = float_converter.convert()
            float_tflite_model_size = open(FLOAT_MODEL_TFLITE, "wb").write(float_tflite_model)
            print(f"[完成] Float 模型大小: {float_tflite_model_size} bytes")
            
            # 生成量化模型
            print("[轉換] 量化模型 (int8)...")
            converter = tf.lite.TFLiteConverter.from_saved_model(SAVED_MODEL)
            converter.optimizations = [tf.lite.Optimize.DEFAULT]
            converter.inference_input_type = tf.int8
            converter.inference_output_type = tf.int8
            
            # 代表性數據集生成器
            def representative_dataset_gen():
                for i in range(100):
                    data, _ = audio_processor.get_data(
                        1, i*1, model_settings,
                        BACKGROUND_FREQUENCY,
                        BACKGROUND_VOLUME_RANGE,
                        TIME_SHIFT_MS,
                        'testing',
                        sess
                    )
                    flattened_data = np.array(data.flatten(), dtype=np.float32).reshape(1, 1960)
                    yield [flattened_data]
            
            converter.representative_dataset = representative_dataset_gen
            tflite_model = converter.convert()
            tflite_model_size = open(MODEL_TFLITE, "wb").write(tflite_model)
            print(f"[完成] 量化模型大小: {tflite_model_size} bytes")
            
            return audio_processor, model_settings
            
    except Exception as e:
        print(f"[錯誤] TFLite 轉換失敗: {e}")
        import traceback
        traceback.print_exc()
        return None, None

def test_tflite_model(audio_processor, model_settings):
    """測試 TFLite 模型準確度"""
    if audio_processor is None or model_settings is None:
        print("[跳過] 無法測試模型（轉換失敗）")
        return
    
    print("\n[測試] TFLite 模型準確度...")
    
    def run_inference(tflite_model_path, model_type="Float"):
        """執行推理測試"""
        try:
            # 載入測試數據
            np.random.seed(0)
            with tf.compat.v1.Session() as sess:
                test_data, test_labels = audio_processor.get_data(
                    -1, 0, model_settings, BACKGROUND_FREQUENCY, BACKGROUND_VOLUME_RANGE,
                    TIME_SHIFT_MS, 'testing', sess
                )
            test_data = np.expand_dims(test_data, axis=1).astype(np.float32)
            
            # 初始化解釋器
            interpreter = tf.lite.Interpreter(
                tflite_model_path,
                experimental_op_resolver_type=tf.lite.experimental.OpResolverType.BUILTIN_REF
            )
            interpreter.allocate_tensors()
            
            input_details = interpreter.get_input_details()[0]
            output_details = interpreter.get_output_details()[0]
            
            # 量化模型需要手動量化輸入數據
            if model_type == "Quantized":
                input_scale, input_zero_point = input_details["quantization"]
                test_data = test_data / input_scale + input_zero_point
                test_data = test_data.astype(input_details["dtype"])
            
            # 執行推理
            correct_predictions = 0
            for i in range(len(test_data)):
                interpreter.set_tensor(input_details["index"], test_data[i])
                interpreter.invoke()
                output = interpreter.get_tensor(output_details["index"])[0]
                top_prediction = output.argmax()
                correct_predictions += (top_prediction == test_labels[i])
            
            accuracy = (correct_predictions * 100) / len(test_data)
            print(f'[結果] {model_type} 模型準確度: {accuracy:.2f}% (測試樣本數={len(test_data)})')
            
        except Exception as e:
            print(f"[錯誤] {model_type} 模型測試失敗: {e}")
    
    # 測試兩種模型
    run_inference(FLOAT_MODEL_TFLITE, "Float")
    run_inference(MODEL_TFLITE, "Quantized")

def convert_to_c_array():
    """將 TFLite 模型轉換為 C 語言陣列（用於微控制器）"""
    print("\n[轉換] 生成 C 語言陣列...")
    
    try:
        # 讀取 TFLite 模型
        with open(MODEL_TFLITE, 'rb') as f:
            tflite_binary = f.read()
        
        # 生成 C 語言格式的十六進制陣列
        hex_array = ', '.join([f'0x{b:02x}' for b in tflite_binary])
        
        # 生成 C 語言源文件
        c_code = f"""// 自動生成的模型文件
// 訓練關鍵字: {WANTED_WORDS}

#include <stdint.h>

// 模型數據（{len(tflite_binary)} bytes）
alignas(8) const unsigned char g_model[] = {{
  {hex_array}
}};

const int g_model_len = {len(tflite_binary)};
"""
        
        with open(MODEL_TFLITE_MICRO, 'w') as f:
            f.write(c_code)
        
        print(f"[完成] C 陣列已生成: {MODEL_TFLITE_MICRO}")
        print(f"[大小] {len(tflite_binary)} bytes")
        
        return True
        
    except Exception as e:
        print(f"[錯誤] C 陣列生成失敗: {e}")
        return False

# ============================================================================
# 主程序
# ============================================================================

def main():
    """主執行流程"""
    print("\n")
    print("=" * 70)
    print("  TensorFlow Lite Micro Speech 模型訓練工具")
    print("  適用於 ESP32 等微控制器的關鍵字識別")
    print("=" * 70)
    
    # 步驟 1: 創建目錄
    print("\n[步驟 1/6] 創建工作目錄...")
    create_directories()
    
    # 步驟 2: 檢查 TensorFlow 倉庫
    print("\n[步驟 2/6] 檢查 TensorFlow 倉庫...")
    if not check_tensorflow_repo():
        print("[失敗] 無法下載 TensorFlow 倉庫")
        return
    
    # 步驟 3: 訓練模型
    print("\n[步驟 3/6] 開始訓練模型...")
    print("[提示] 這可能需要 1-2 小時，請耐心等待...")
    if not train_model():
        print("[失敗] 模型訓練失敗")
        return
    
    # 步驟 4: 凍結模型
    print("\n[步驟 4/6] 凍結模型...")
    if not freeze_model():
        print("[失敗] 模型凍結失敗")
        return
    
    # 步驟 5: 轉換為 TFLite
    print("\n[步驟 5/6] 轉換為 TFLite 格式...")
    audio_processor, model_settings = convert_to_tflite()
    if audio_processor is None:
        print("[失敗] TFLite 轉換失敗")
        return
    
    # 步驟 5.5: 測試模型準確度
    test_tflite_model(audio_processor, model_settings)
    
    # 步驟 6: 轉換為 C 陣列
    print("\n[步驟 6/6] 生成 C 語言陣列...")
    if not convert_to_c_array():
        print("[失敗] C 陣列生成失敗")
        return
    
    # 完成
    print("\n" + "=" * 70)
    print("  訓練完成！")
    print("=" * 70)
    print(f"\n生成的文件:")
    print(f"  - 浮點模型: {FLOAT_MODEL_TFLITE}")
    print(f"  - 量化模型: {MODEL_TFLITE}")
    print(f"  - C 陣列: {MODEL_TFLITE_MICRO}")
    print(f"\n部署到 ESP32:")
    print(f"  1. 複製 {MODEL_TFLITE_MICRO} 到你的 ESP32 專案")
    print(f"  2. 替換原有的 model.cc 或 hello_world_model_data.cc")
    print(f"  3. 更新 kCategoryCount 和 kCategoryLabels")
    print(f"  4. 重新編譯並上傳到 ESP32")
    print("\n" + "=" * 70)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n[中斷] 用戶取消操作")
    except Exception as e:
        print(f"\n[錯誤] 未預期的錯誤: {e}")
        import traceback
        traceback.print_exc()
