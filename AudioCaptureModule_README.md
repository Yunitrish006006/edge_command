# 音訊擷取模組 (AudioCaptureModule) 使用說明

## 概述

AudioCaptureModule 是一個模組化的音訊擷取系統，將原本分散在各個函數中的音訊處理功能整合到一個統一的類別中。這個模組提供了完整的音訊處理管線，包括：

- I2S 音訊介面初始化和管理
- 即時音訊數據處理和正規化
- 語音活動檢測 (VAD)
- 音訊特徵提取
- 事件驅動的回調系統

## 主要特性

### 1. 模組化設計

- 所有音訊相關功能封裝在單一類別中
- 清晰的初始化/去初始化生命週期
- 統一的狀態管理

### 2. 事件驅動架構

- 音訊幀處理回調
- VAD 狀態變更回調
- 語音完成回調

### 3. 記憶體管理

- 自動記憶體分配和釋放
- 智能緩衝區管理
- 防止記憶體洩漏

### 4. 強健的錯誤處理

- 完整的初始化檢查
- 優雅的錯誤恢復
- 詳細的調試信息

## 使用方法

### 1. 基本初始化

```cpp
#include "audio_module.h"

// 創建音訊模組實例
AudioCaptureModule audio_module;

void setup() {
    // 初始化模組
    if (audio_module.initialize()) {
        Serial.println("音訊模組初始化成功");
        
        // 開始音訊擷取
        audio_module.start_capture();
    }
}
```

### 2. 設置回調函數

```cpp
// 音訊幀回調
void on_audio_frame(const AudioFeatures &features) {
    Serial.printf("RMS: %.3f, ZCR: %.3f\n", 
                  features.rms_energy, 
                  features.zero_crossing_rate);
}

// VAD 狀態回調
void on_vad_event(const VADResult &result) {
    if (result.speech_detected) {
        Serial.println("檢測到語音活動");
    }
}

// 語音完成回調
void on_speech_complete(const float *speech_data, size_t length, unsigned long duration_ms) {
    Serial.printf("語音完成 - 長度: %zu, 持續: %lu ms\n", length, duration_ms);
    // 在這裡進行關鍵字檢測或其他處理
}

void setup() {
    // ... 初始化代碼 ...
    
    // 註冊回調函數
    audio_module.set_audio_frame_callback(on_audio_frame);
    audio_module.set_vad_callback(on_vad_event);
    audio_module.set_speech_complete_callback(on_speech_complete);
}
```

### 3. 主循環處理

```cpp
void loop() {
    // 處理音訊 - 這會調用所有註冊的回調函數
    audio_module.process_audio_loop();
    
    // 其他應用邏輯...
}
```

## API 參考

### 核心方法

- `bool initialize()` - 初始化模組和 I2S 介面
- `void deinitialize()` - 清理資源
- `bool start_capture()` - 開始音訊擷取
- `void stop_capture()` - 停止音訊擷取
- `void process_audio_loop()` - 主要處理循環

### 回調註冊

- `set_audio_frame_callback(AudioFrameCallback callback)`
- `set_vad_callback(VADCallback callback)`
- `set_speech_complete_callback(SpeechCompleteCallback callback)`

### 狀態查詢

- `bool is_module_initialized()` - 檢查模組是否已初始化
- `bool is_capture_running()` - 檢查是否正在擷取
- `VADState get_current_vad_state()` - 獲取當前 VAD 狀態
- `AudioStats get_audio_stats()` - 獲取音訊統計信息

### 配置控制

- `void reset_vad()` - 重置語音活動檢測
- `void clear_speech_buffer()` - 清除語音緩衝區

## 數據結構

### AudioFeatures

音訊特徵結構，包含：

- `float rms_energy` - RMS 能量
- `float zero_crossing_rate` - 零穿越率  
- `float spectral_centroid` - 頻譜重心
- `bool is_voice_detected` - 語音檢測標誌

### VADResult

語音活動檢測結果：

- `VADState state` - VAD 狀態
- `bool speech_detected` - 是否檢測到語音
- `bool speech_complete` - 語音是否完整結束
- `float energy_level` - 能量等級
- `unsigned long duration_ms` - 持續時間

### AudioStats

音訊統計信息：

- `int32_t min_amplitude` - 最小振幅
- `int32_t max_amplitude` - 最大振幅
- `int32_t avg_amplitude` - 平均振幅
- `size_t samples_processed` - 已處理樣本數

## 配置常數

所有配置常數在 `audio_module.h` 中定義：

- `AUDIO_SAMPLE_RATE` - 採樣率 (16kHz)
- `AUDIO_BUFFER_SIZE` - 緩衝區大小 (512 樣本)
- `AUDIO_FRAME_SIZE` - 幀大小 (256 樣本)
- `VAD_ENERGY_THRESHOLD` - VAD 能量閾值
- `SPEECH_BUFFER_SIZE` - 語音緩衝區大小

## 遷移指南

### 從舊系統遷移

舊的使用方式：

```cpp
// 舊方式
audio_init();
audio_read(buffer, size);
audio_process(raw, processed, size);
// ... 複雜的處理邏輯
```

新的使用方式：

```cpp
// 新方式
AudioCaptureModule audio_module;
audio_module.initialize();
audio_module.set_speech_complete_callback(handle_speech);
audio_module.start_capture();

void loop() {
    audio_module.process_audio_loop(); // 一行搞定！
}
```

### 主要改進

1. **簡化使用** - 一個 `process_audio_loop()` 調用處理所有音訊
2. **事件驅動** - 通過回調函數處理音訊事件
3. **更好的記憶體管理** - 自動分配和釋放
4. **模組化** - 獨立的功能單元，易於測試和維護
5. **錯誤處理** - 更強健的錯誤檢查和恢復

## 故障排除

### 常見問題

1. **初始化失敗**
   - 檢查 I2S 引腳配置
   - 確保有足夠的記憶體
   - 查看串口輸出的錯誤信息

2. **沒有音訊數據**
   - 檢查麥克風連接
   - 驗證 I2S 配置
   - 檢查 VAD 閾值設定

3. **記憶體不足**
   - 調整緩衝區大小常數
   - 檢查其他記憶體使用

### 調試技巧

1. 啟用詳細輸出：設置回調函數來監控所有事件
2. 檢查音訊統計：使用 `get_audio_stats()` 監控音訊信號
3. 監控 VAD 狀態：設置 VAD 回調來追蹤語音檢測

## 結論

AudioCaptureModule 提供了一個現代化、模組化的音訊處理解決方案。通過事件驅動的架構和統一的 API，它大大簡化了音訊應用的開發，同時提供了更好的可維護性和擴展性。
