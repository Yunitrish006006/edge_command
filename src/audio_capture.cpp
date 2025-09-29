#include "audio_capture.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

// 數學常數
#ifndef PI
#define PI 3.14159265359f
#endif

static const char *TAG = "AudioCapture";

// 全局變量
int32_t audio_buffer[BUFFER_SIZE];
int16_t processed_audio[BUFFER_SIZE];
float normalized_audio[FRAME_SIZE];
float feature_buffer[FRAME_SIZE];

// 音頻處理狀態變量
static int16_t frame_buffer[BUFFER_SIZE * 2]; // 幀緩衝區（支持重疊）
static size_t frame_write_pos = 0;            // 寫入位置
static bool frame_ready_flag = false;         // 幀就緒標誌

/**
 * 初始化 I2S 介面用於 INMP441 麥克風
 */
bool audio_init()
{
    Serial.println("Initializing I2S for INMP441...");

    // I2S 配置結構
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // 主模式，接收
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,      // 32-bit 容器
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,       // 只使用左聲道
        .communication_format = I2S_COMM_FORMAT_STAND_I2S, // 標準 I2S 格式
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,          // 中斷優先級
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false, // 不使用 APLL
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    // I2S 引腳配置
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,         // 位元時鐘引腳
        .ws_io_num = I2S_WS_PIN,           // 字選引腳
        .data_out_num = I2S_PIN_NO_CHANGE, // 不使用輸出
        .data_in_num = I2S_SD_PIN          // 數據輸入引腳
    };

    // 安裝 I2S 驅動
    esp_err_t ret = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (ret != ESP_OK)
    {
        Serial.printf("Failed to install I2S driver: %s\n", esp_err_to_name(ret));
        return false;
    }

    // 設置 I2S 引腳
    ret = i2s_set_pin(I2S_PORT, &pin_config);
    if (ret != ESP_OK)
    {
        Serial.printf("Failed to set I2S pins: %s\n", esp_err_to_name(ret));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    // 清除 I2S 緩衝區
    ret = i2s_zero_dma_buffer(I2S_PORT);
    if (ret != ESP_OK)
    {
        Serial.printf("Failed to clear I2S buffer: %s\n", esp_err_to_name(ret));
    }

    Serial.println("I2S initialized successfully!");
    return true;
}

/**
 * 從 INMP441 讀取音頻數據
 */
size_t audio_read(int32_t *buffer, size_t buffer_size)
{
    size_t bytes_read = 0;

    esp_err_t ret = i2s_read(I2S_PORT, buffer, buffer_size * sizeof(int32_t),
                             &bytes_read, portMAX_DELAY);

    if (ret != ESP_OK)
    {
        Serial.printf("I2S read error: %s\n", esp_err_to_name(ret));
        return 0;
    }

    return bytes_read / sizeof(int32_t); // 返回樣本數量
}

/**
 * 處理原始音頻數據
 * 將 32-bit 數據轉換為 16-bit，並進行基本的信號處理
 */
void audio_process(int32_t *raw_data, int16_t *processed_data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        // INMP441 輸出 24-bit 數據，位於 32-bit 容器的高 24 位
        // 右移 8 位來獲得 24-bit 值，然後再右移 8 位轉為 16-bit
        int32_t sample = raw_data[i] >> 8; // 獲得 24-bit 有符號值

        // 轉換為 16-bit（右移 8 位）
        processed_data[i] = (int16_t)(sample >> 8);

        // 可選：簡單的增益調整（如果音量太小）
        // processed_data[i] = (int16_t)((sample >> 8) * 2); // 2倍增益
    }
}

/**
 * 去初始化 I2S
 */
void audio_deinit()
{
    i2s_driver_uninstall(I2S_PORT);
    Serial.println("I2S deinitialized");
}

// ======== 音頻預處理函數實作 ========

/**
 * 音頻正規化：將 16-bit 整數轉換為 [-1.0, 1.0] 浮點數
 */
void audio_normalize(int16_t *input, float *output, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        // 轉換為 [-1.0, 1.0] 範圍
        output[i] = (float)input[i] / MAX_AMPLITUDE * NORMALIZATION_FACTOR;

        // 限制範圍
        if (output[i] > 1.0f)
            output[i] = 1.0f;
        if (output[i] < -1.0f)
            output[i] = -1.0f;
    }
}

/**
 * 應用漢寧窗函數來減少頻譜洩漏
 */
void audio_apply_window(float *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        float window_val = 0.5f * (1.0f - cos(2.0f * PI * i / (length - 1)));
        data[i] *= window_val;
    }
}

/**
 * 計算 RMS (Root Mean Square) 能量
 */
float audio_calculate_rms(float *data, size_t length)
{
    float sum = 0.0f;
    for (size_t i = 0; i < length; i++)
    {
        sum += data[i] * data[i];
    }
    return sqrt(sum / length);
}

/**
 * 計算零穿越率（Zero Crossing Rate）
 * 用於區分語音和音樂/噪聲
 */
float audio_calculate_zero_crossing_rate(int16_t *data, size_t length)
{
    int zero_crossings = 0;
    for (size_t i = 1; i < length; i++)
    {
        if ((data[i] >= 0) != (data[i - 1] >= 0))
        {
            zero_crossings++;
        }
    }
    return (float)zero_crossings / (length - 1);
}

/**
 * 檢查是否有新的音頻幀準備好進行處理
 */
bool audio_frame_ready(int16_t *new_samples, size_t sample_count)
{
    // 將新樣本添加到幀緩衝區
    for (size_t i = 0; i < sample_count; i++)
    {
        frame_buffer[frame_write_pos] = new_samples[i];
        frame_write_pos++;

        // 檢查是否有完整的幀
        if (frame_write_pos >= FRAME_SIZE)
        {
            frame_ready_flag = true;

            // 移動數據以支持重疊處理
            // 將後半部分移到前半部分
            memmove(frame_buffer, frame_buffer + FRAME_SIZE - FRAME_OVERLAP,
                    FRAME_OVERLAP * sizeof(int16_t));
            frame_write_pos = FRAME_OVERLAP;

            return true;
        }
    }

    return false;
}

/**
 * 獲取當前音頻幀（正規化並應用窗函數）
 */
void audio_get_current_frame(float *frame_output)
{
    if (!frame_ready_flag)
        return;

    // 從緩衝區複製一個完整幀
    int16_t temp_frame[FRAME_SIZE];
    memcpy(temp_frame, frame_buffer, FRAME_SIZE * sizeof(int16_t));

    // 正規化
    audio_normalize(temp_frame, frame_output, FRAME_SIZE);

    // 應用窗函數
    audio_apply_window(frame_output, FRAME_SIZE);

    frame_ready_flag = false;
}

/**
 * 提取音頻特徵
 */
void audio_extract_features(float *frame, AudioFeatures *features)
{
    // 計算 RMS 能量
    features->rms_energy = audio_calculate_rms(frame, FRAME_SIZE);

    // 為了計算零穿越率，我們需要將浮點數轉回整數
    int16_t temp_samples[FRAME_SIZE];
    for (size_t i = 0; i < FRAME_SIZE; i++)
    {
        temp_samples[i] = (int16_t)(frame[i] * MAX_AMPLITUDE);
    }

    // 計算零穿越率
    features->zero_crossing_rate = audio_calculate_zero_crossing_rate(temp_samples, FRAME_SIZE);

    // 簡化的頻譜重心計算（基於高頻內容）
    float high_freq_energy = 0.0f;
    float total_energy = 0.0f;

    for (size_t i = 0; i < FRAME_SIZE; i++)
    {
        float energy = frame[i] * frame[i];
        total_energy += energy;

        // 簡單地將後半部分視為高頻
        if (i > FRAME_SIZE / 2)
        {
            high_freq_energy += energy;
        }
    }

    features->spectral_centroid = (total_energy > 0) ? (high_freq_energy / total_energy) : 0.0f;

    // 語音檢測邏輯
    // 語音通常有：適中的能量、適中的零穿越率、平衡的頻譜
    features->is_voice_detected =
        (features->rms_energy > 0.01f && features->rms_energy < 0.8f) &&
        (features->zero_crossing_rate > 0.02f && features->zero_crossing_rate < 0.3f) &&
        (features->spectral_centroid > 0.1f && features->spectral_centroid < 0.9f);
}