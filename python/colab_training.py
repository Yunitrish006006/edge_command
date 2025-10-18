# TensorFlow Lite Micro Speech 模型訓練 - Google Colab 版本
# 直接複製到 Colab 執行：https://colab.research.google.com/

# ============================================================================
# 步驟 1: 啟用 GPU（重要！訓練速度快 10 倍）
# ============================================================================
# Runtime → Change runtime type → Hardware accelerator: GPU → Save

# ============================================================================
# 步驟 2: 配置參數
# ============================================================================

# 訓練關鍵字（可修改）
WANTED_WORDS = "yes,no"  # 選項: yes,no,up,down,left,right,on,off,stop,go

# 訓練步數
TRAINING_STEPS = "12000,3000"  # 總共 15000 步，約 15-20 分鐘（GPU）
LEARNING_RATE = "0.001,0.0001"

print(f"訓練關鍵字: {WANTED_WORDS}")
print(f"訓練步數: {TRAINING_STEPS}")

# ============================================================================
# 步驟 3: 下載 TensorFlow 倉庫
# ============================================================================

!git clone -q --depth 1 https://github.com/tensorflow/tensorflow
print("✅ TensorFlow 倉庫下載完成")

# ============================================================================
# 步驟 4: 開始訓練
# ============================================================================

!python tensorflow/tensorflow/examples/speech_commands/train.py \
--data_dir=dataset/ \
--wanted_words={WANTED_WORDS} \
--silence_percentage=25 \
--unknown_percentage=25 \
--preprocess=micro \
--window_stride=20 \
--model_architecture=tiny_conv \
--how_many_training_steps={TRAINING_STEPS} \
--learning_rate={LEARNING_RATE} \
--train_dir=train/ \
--summaries_dir=logs/ \
--verbosity=INFO \
--eval_step_interval=1000 \
--save_step_interval=1000

print("\n✅ 訓練完成！")

# ============================================================================
# 步驟 5: 凍結模型
# ============================================================================

TOTAL_STEPS = str(sum(map(lambda x: int(x), TRAINING_STEPS.split(","))))

!python tensorflow/tensorflow/examples/speech_commands/freeze.py \
--wanted_words={WANTED_WORDS} \
--window_stride_ms=20 \
--preprocess=micro \
--model_architecture=tiny_conv \
--start_checkpoint=train/tiny_conv.ckpt-{TOTAL_STEPS} \
--save_format=saved_model \
--output_file=models/saved_model

print("✅ 模型凍結完成")

# ============================================================================
# 步驟 6: 轉換為 TFLite 並測試
# ============================================================================

import sys
import tensorflow as tf
import numpy as np

# 添加路徑
sys.path.append("/content/tensorflow/tensorflow/examples/speech_commands/")
import input_data
import models as model_utils

# 準備模型設置
SAMPLE_RATE = 16000
CLIP_DURATION_MS = 1000
WINDOW_SIZE_MS = 30.0
WINDOW_STRIDE = 20
FEATURE_BIN_COUNT = 40
PREPROCESS = 'micro'

DATA_URL = 'https://storage.googleapis.com/download.tensorflow.org/data/speech_commands_v0.02.tar.gz'
VALIDATION_PERCENTAGE = 10
TESTING_PERCENTAGE = 10
SILENT_PERCENTAGE = 25
UNKNOWN_PERCENTAGE = 25

model_settings = model_utils.prepare_model_settings(
    len(input_data.prepare_words_list(WANTED_WORDS.split(','))),
    SAMPLE_RATE, CLIP_DURATION_MS, WINDOW_SIZE_MS,
    WINDOW_STRIDE, FEATURE_BIN_COUNT, PREPROCESS
)

audio_processor = input_data.AudioProcessor(
    DATA_URL, 'dataset/',
    SILENT_PERCENTAGE, UNKNOWN_PERCENTAGE,
    WANTED_WORDS.split(','), VALIDATION_PERCENTAGE,
    TESTING_PERCENTAGE, model_settings, 'logs/'
)

# 創建目錄
!mkdir -p models

# 生成浮點模型
print("\n[轉換] 浮點模型...")
with tf.compat.v1.Session() as sess:
    float_converter = tf.lite.TFLiteConverter.from_saved_model('models/saved_model')
    float_tflite_model = float_converter.convert()
    with open('models/float_model.tflite', 'wb') as f:
        f.write(float_tflite_model)
    print(f"✅ 浮點模型: {len(float_tflite_model)} bytes")

# 生成量化模型（int8）
print("\n[轉換] 量化模型...")
with tf.compat.v1.Session() as sess:
    converter = tf.lite.TFLiteConverter.from_saved_model('models/saved_model')
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    
    def representative_dataset_gen():
        for i in range(100):
            data, _ = audio_processor.get_data(
                1, i*1, model_settings,
                0.8, 0.1, 100.0, 'testing', sess
            )
            flattened_data = np.array(data.flatten(), dtype=np.float32).reshape(1, 1960)
            yield [flattened_data]
    
    converter.representative_dataset = representative_dataset_gen
    tflite_model = converter.convert()
    
    with open('models/model.tflite', 'wb') as f:
        f.write(tflite_model)
    print(f"✅ 量化模型: {len(tflite_model)} bytes")

# ============================================================================
# 步驟 7: 測試模型準確度
# ============================================================================

def test_model(tflite_path, model_type="Float"):
    print(f"\n[測試] {model_type} 模型準確度...")
    
    np.random.seed(0)
    with tf.compat.v1.Session() as sess:
        test_data, test_labels = audio_processor.get_data(
            -1, 0, model_settings, 0.8, 0.1, 100.0, 'testing', sess
        )
    test_data = np.expand_dims(test_data, axis=1).astype(np.float32)
    
    interpreter = tf.lite.Interpreter(
        tflite_path,
        experimental_op_resolver_type=tf.lite.experimental.OpResolverType.BUILTIN_REF
    )
    interpreter.allocate_tensors()
    
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    
    if model_type == "Quantized":
        input_scale, input_zero_point = input_details["quantization"]
        test_data = test_data / input_scale + input_zero_point
        test_data = test_data.astype(input_details["dtype"])
    
    correct = 0
    for i in range(len(test_data)):
        interpreter.set_tensor(input_details["index"], test_data[i])
        interpreter.invoke()
        output = interpreter.get_tensor(output_details["index"])[0]
        if output.argmax() == test_labels[i]:
            correct += 1
    
    accuracy = (correct * 100) / len(test_data)
    print(f"✅ {model_type} 準確度: {accuracy:.2f}% (測試樣本: {len(test_data)})")

test_model('models/float_model.tflite', 'Float')
test_model('models/model.tflite', 'Quantized')

# ============================================================================
# 步驟 8: 生成 C 陣列
# ============================================================================

print("\n[轉換] 生成 C 陣列...")

with open('models/model.tflite', 'rb') as f:
    tflite_binary = f.read()

hex_array = ', '.join([f'0x{b:02x}' for b in tflite_binary])

c_code = f"""// 自動生成的模型文件
// 訓練關鍵字: {WANTED_WORDS}
// 模型大小: {len(tflite_binary)} bytes

#include <stdint.h>

// 模型數據
alignas(8) const unsigned char g_model[] = {{
  {hex_array}
}};

const int g_model_len = {len(tflite_binary)};
"""

with open('models/model.cc', 'w') as f:
    f.write(c_code)

print(f"✅ C 陣列已生成: models/model.cc ({len(tflite_binary)} bytes)")

# ============================================================================
# 步驟 9: 下載模型檔案
# ============================================================================

print("\n" + "="*70)
print("  訓練完成！🎉")
print("="*70)
print("\n生成的檔案:")
print("  - models/float_model.tflite (浮點模型)")
print("  - models/model.tflite (量化模型)")
print("  - models/model.cc (C 陣列，用於 ESP32)")
print("\n下載方式:")
print("  左側選單 Files → models → 右鍵下載")
print("\n部署到 ESP32:")
print("  1. 下載 model.cc")
print("  2. 複製到您的 ESP32 專案 src/ 目錄")
print("  3. 替換原有的模型檔案")
print("  4. 更新類別數量和標籤")
print("  5. 重新編譯並上傳")
print("="*70)

# 顯示檔案列表
!ls -lh models/
