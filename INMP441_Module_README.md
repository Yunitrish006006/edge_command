# INMP441 數位麥克風模組使用說明

## 概述

INMP441Module 是專門為 INMP441 數位麥克風設計的獨立模組，負責處理所有與 INMP441 相關的 I2S 通信、音訊數據獲取和狀態管理。這個模組將硬體抽象化，提供簡潔的 API 來操作 INMP441 麥克風。

## 主要特性

### 🎯 專業化設計

- 專門針對 INMP441 數位麥克風優化
- 處理 24-bit 到 16-bit 的數據轉換
- 自動增益控制和信號處理

### 🔧 靈活配置

- 支持自定義引腳配置
- 可調整採樣率、增益等參數
- 多種預設配置選項

### 📊 狀態管理

- 完整的狀態機管理
- 錯誤檢測和自動恢復
- 詳細的統計信息

### 🔄 事件驅動

- 音訊數據回調
- 狀態變更通知
- 非阻塞操作模式

## 硬體連接

### INMP441 引腳對應

```
INMP441    ESP32-S3
---------- ----------
VCC        3.3V
GND        GND
L/R        GND (左聲道)
WS         GPIO42 (可配置)
SCK        GPIO41 (可配置)
SD         GPIO2  (可配置)
```

### 預設引腳配置

- **WS (Word Select)**: GPIO42
- **SCK (Serial Clock)**: GPIO41  
- **SD (Serial Data)**: GPIO2

## API 使用指南

### 1. 基本使用

```cpp
#include "inmp441_module.h"

// 創建 INMP441 模組實例
INMP441Module inmp441;

void setup() {
    // 初始化模組
    if (inmp441.initialize()) {
        Serial.println("INMP441 初始化成功");
        
        // 開始音訊擷取
        inmp441.start();
    }
}

void loop() {
    // 讀取音訊數據
    int16_t audio_buffer[512];
    size_t samples_read = inmp441.read_audio_data(audio_buffer, 512);
    
    if (samples_read > 0) {
        // 處理音訊數據
        process_audio(audio_buffer, samples_read);
    }
}
```

### 2. 使用回調函數

```cpp
// 音訊數據回調
void on_audio_data(const int16_t *audio_data, size_t sample_count) {
    Serial.printf("收到 %zu 個音訊樣本\n", sample_count);
    // 處理音訊數據...
}

// 狀態變更回調
void on_state_change(INMP441State state, const char *message) {
    Serial.printf("INMP441 狀態: %s - %s\n", 
                  inmp441_state_to_string(state), 
                  message ? message : "");
}

void setup() {
    // 設置回調函數
    inmp441.set_audio_data_callback(on_audio_data);
    inmp441.set_state_change_callback(on_state_change);
    
    inmp441.initialize();
    inmp441.start();
}

void loop() {
    // 使用回調模式
    inmp441.read_audio_frame(); // 會觸發回調函數
}
```

### 3. 自定義配置

```cpp
// 創建自定義配置
INMP441Config custom_config = INMP441Module::create_custom_config(
    42,    // WS 引腳
    41,    // SCK 引腳  
    2,     // SD 引腳
    16000  // 採樣率
);

// 或者修改預設配置
INMP441Config config = INMP441Module::create_default_config();
config.sample_rate = 8000;   // 改為 8kHz
config.gain_factor = 8;      // 增加增益

// 使用自定義配置初始化
INMP441Module inmp441(custom_config);
inmp441.initialize();
```

## 配置選項

### INMP441Config 結構

```cpp
struct INMP441Config {
    uint8_t ws_pin;          // WS 引腳
    uint8_t sck_pin;         // SCK 引腳  
    uint8_t sd_pin;          // SD 引腳
    i2s_port_t i2s_port;     // I2S 端口
    uint32_t sample_rate;    // 採樣率
    uint8_t dma_buf_count;   // DMA 緩衝區數量
    uint8_t dma_buf_len;     // DMA 緩衝區長度
    uint16_t buffer_size;    // 讀取緩衝區大小
    uint8_t gain_factor;     // 增益係數
};
```

### 常用採樣率

- **8000 Hz**: 電話品質
- **16000 Hz**: 語音識別標準 (推薦)
- **22050 Hz**: 高品質語音
- **44100 Hz**: CD 品質 (較高 CPU 負載)

## 狀態管理

### INMP441 狀態

```cpp
enum INMP441State {
    INMP441_UNINITIALIZED,  // 未初始化
    INMP441_INITIALIZED,    // 已初始化但未運行
    INMP441_RUNNING,        // 正在運行
    INMP441_ERROR           // 錯誤狀態
};
```

### 狀態查詢方法

```cpp
// 檢查各種狀態
if (inmp441.is_initialized()) {
    Serial.println("模組已初始化");
}

if (inmp441.is_running()) {
    Serial.println("正在擷取音訊");
}

if (inmp441.has_error()) {
    Serial.println("發生錯誤");
    inmp441.clear_errors(); // 清除錯誤
}
```

## 統計信息和調試

### 獲取統計信息

```cpp
INMP441Module::INMP441Stats stats = inmp441.get_statistics();

Serial.printf("總樣本數: %lu\n", stats.total_samples);
Serial.printf("運行時間: %lu ms\n", stats.uptime_ms);
Serial.printf("錯誤計數: %zu\n", stats.error_count);
Serial.printf("採樣率: %.1f samples/sec\n", stats.samples_per_second);
```

### 調試方法

```cpp
// 打印配置信息
inmp441.print_config();

// 打印統計信息
inmp441.print_statistics();

// 自我測試
if (inmp441.self_test()) {
    Serial.println("自我測試通過");
} else {
    Serial.println("自我測試失敗");
}
```

## 與 AudioCaptureModule 集成

AudioCaptureModule 現在內部使用 INMP441Module：

```cpp
#include "audio_module.h"

AudioCaptureModule audio_module;

void setup() {
    // 可選: 配置 INMP441
    INMP441Config inmp441_config = INMP441Module::create_default_config();
    inmp441_config.gain_factor = 8; // 增加增益
    
    // 使用自定義配置初始化
    audio_module.initialize(inmp441_config);
    
    // 或直接訪問 INMP441 模組
    audio_module.get_inmp441_module().print_config();
    
    audio_module.start_capture();
}
```

## 錯誤處理和故障排除

### 常見問題

1. **初始化失敗**

   ```cpp
   if (!inmp441.initialize()) {
       // 檢查引腳配置
       // 檢查 I2S 端口是否被占用
       // 檢查記憶體是否足夠
   }
   ```

2. **無音訊數據**

   ```cpp
   if (inmp441.read_audio_data(buffer, size) == 0) {
       // 檢查麥克風連接
       // 檢查是否已調用 start()
       // 檢查 I2S 配置
   }
   ```

3. **音訊品質問題**

   ```cpp
   // 調整增益係數
   INMP441Config config = inmp441.get_config();
   config.gain_factor = 8; // 增加增益
   inmp441.set_config(config);
   
   // 或檢查採樣率設定
   config.sample_rate = 16000; // 確保正確採樣率
   ```

### 調試技巧

1. **啟用詳細輸出**: 設置狀態變更回調來監控所有狀態變化
2. **檢查統計信息**: 定期查看 `get_statistics()` 來監控性能
3. **使用自我測試**: 調用 `self_test()` 來驗證硬體連接

## 性能考量

### 記憶體使用

- 預設緩衝區: 512 樣本 × 4 bytes = 2KB
- DMA 緩衝區: 8 × 64 × 4 bytes = 2KB
- 總記憶體需求: 約 4KB

### CPU 負載

- 16kHz 採樣率: 輕微負載
- 44.1kHz 採樣率: 中等負載
- 建議使用 16kHz 進行語音應用

## 總結

INMP441Module 提供了完整的 INMP441 數位麥克風控制解決方案，包括：

- 🎛️ 靈活的硬體配置
- 📡 可靠的 I2S 通信
- 🔄 事件驅動架構  
- 📊 完整的狀態管理
- 🛠️ 豐富的調試工具

通過模組化設計，INMP441 的複雜性被完全封裝，讓開發者可以專注於音訊應用邏輯而不需要擔心底層硬體細節。
