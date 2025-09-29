#ifndef INMP441_MODULE_H
#define INMP441_MODULE_H

#include "driver/i2s.h"
#include <Arduino.h>
#include <functional>

// INMP441 硬體配置常數
#define INMP441_WS_PIN 42      // WS (Word Select) 信號 - GPIO42
#define INMP441_SCK_PIN 41     // SCK (Serial Clock) 時鐘信號 - GPIO41
#define INMP441_SD_PIN 2       // SD (Serial Data) 數據信號 - GPIO2

#define INMP441_I2S_PORT I2S_NUM_0
#define INMP441_SAMPLE_RATE 16000    // 16kHz 採樣率，適合語音識別
#define INMP441_BITS_PER_SAMPLE 32   // INMP441 輸出 24-bit，但使用 32-bit 容器
#define INMP441_CHANNELS 1           // 單聲道
#define INMP441_DMA_BUF_COUNT 8      // DMA 緩衝區數量
#define INMP441_DMA_BUF_LEN 64       // 每個 DMA 緩衝區長度

// 數據處理配置
#define INMP441_BUFFER_SIZE 512      // 讀取緩衝區大小（樣本數）
#define INMP441_MAX_AMPLITUDE 32767  // 16-bit 最大振幅
#define INMP441_GAIN_FACTOR 4        // 增益係數（用於調整信號強度）

// INMP441 狀態枚舉
enum INMP441State
{
    INMP441_UNINITIALIZED,  // 未初始化
    INMP441_INITIALIZED,    // 已初始化但未運行
    INMP441_RUNNING,        // 正在運行
    INMP441_ERROR           // 錯誤狀態
};

// INMP441 配置結構
struct INMP441Config
{
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

// 音訊數據回調函數類型
typedef std::function<void(const int16_t *audio_data, size_t sample_count)> AudioDataCallback;
typedef std::function<void(INMP441State state, const char *message)> StateChangeCallback;

/**
 * INMP441 數位麥克風模組類別
 * 專門處理 INMP441 麥克風的 I2S 通信和音訊數據獲取
 */
class INMP441Module
{
private:
    // 硬體配置
    INMP441Config config;
    
    // 緩衝區
    int32_t *raw_buffer;      // 原始 32-bit 數據緩衝區
    int16_t *processed_buffer; // 處理後的 16-bit 數據緩衝區
    
    // 狀態管理
    INMP441State current_state;
    bool i2s_installed;
    
    // 回調函數
    AudioDataCallback audio_data_callback;
    StateChangeCallback state_change_callback;
    
    // 統計信息
    unsigned long total_samples_read;
    unsigned long last_read_time;
    size_t consecutive_errors;
    
    // 內部方法
    bool install_i2s_driver();
    void uninstall_i2s_driver();
    bool configure_i2s_pins();
    void convert_audio_data(const int32_t *raw_data, int16_t *processed_data, size_t length);
    void update_state(INMP441State new_state, const char *message = nullptr);
    
public:
    // 建構函數與解構函數
    INMP441Module();
    explicit INMP441Module(const INMP441Config &custom_config);
    ~INMP441Module();
    
    // 基本控制方法
    bool initialize();
    bool initialize(const INMP441Config &custom_config);
    void deinitialize();
    bool start();
    void stop();
    
    // 數據讀取方法
    size_t read_audio_data(int16_t *output_buffer, size_t max_samples);
    size_t read_raw_audio_data(int32_t *output_buffer, size_t max_samples);
    bool read_audio_frame();  // 讀取一幀數據並調用回調
    
    // 回調註冊方法
    void set_audio_data_callback(AudioDataCallback callback);
    void set_state_change_callback(StateChangeCallback callback);
    
    // 狀態查詢方法
    INMP441State get_state() const { return current_state; }
    bool is_initialized() const { return current_state != INMP441_UNINITIALIZED; }
    bool is_running() const { return current_state == INMP441_RUNNING; }
    bool has_error() const { return current_state == INMP441_ERROR; }
    
    // 配置管理
    INMP441Config get_config() const { return config; }
    bool set_config(const INMP441Config &new_config);
    void reset_to_default_config();
    
    // 統計信息
    struct INMP441Stats
    {
        unsigned long total_samples;     // 總讀取樣本數
        unsigned long uptime_ms;         // 運行時間（毫秒）
        size_t error_count;              // 錯誤計數
        float samples_per_second;        // 每秒樣本數
        unsigned long last_read_time;    // 最後讀取時間
        size_t buffer_size;              // 緩衝區大小
    };
    
    INMP441Stats get_statistics() const;
    void reset_statistics();
    
    // 測試和調試方法
    bool self_test();
    void print_config() const;
    void print_statistics() const;
    
    // 靜態工廠方法
    static INMP441Config create_default_config();
    static INMP441Config create_custom_config(uint8_t ws_pin, uint8_t sck_pin, uint8_t sd_pin, 
                                             uint32_t sample_rate = INMP441_SAMPLE_RATE);
    
    // 錯誤處理
    void clear_errors();
    const char* get_state_string() const;
};

// 便利函數
const char* inmp441_state_to_string(INMP441State state);
INMP441Config inmp441_create_basic_config(uint8_t ws_pin, uint8_t sck_pin, uint8_t sd_pin);

#endif // INMP441_MODULE_H