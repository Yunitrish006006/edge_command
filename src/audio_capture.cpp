#include "audio_capture.h"
#include "esp_log.h"
#include <Arduino.h>
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
        // 修正：直接右移 8 位來獲得 24-bit 值，然後再右移 8 位轉為 16-bit
        // 但是要保持足夠的精度
        int32_t sample = raw_data[i];

        // 第一步：獲得 24-bit 有符號值 (右移 8 位)
        sample = sample >> 8;

        // 第二步：轉換為 16-bit，但要保持更多精度 (右移 4 位而不是 8 位)
        // 這樣可以保持更多的音頻信號強度
        processed_data[i] = (int16_t)(sample >> 4);

        // 調試：增加增益以確保信號可見
        processed_data[i] = (int16_t)(processed_data[i] * 4); // 4倍增益用於調試
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

    // 調試：確保特徵值不會太小被忽略
    if (features->rms_energy < 0.001f && total_energy > 0.0f)
    {
        features->rms_energy = sqrtf(total_energy / FRAME_SIZE);
    }

    // 語音檢測邏輯 (放寬條件)
    // 語音通常有：適中的能量、適中的零穿越率、平衡的頻譜
    features->is_voice_detected =
        (features->rms_energy > 0.001f && features->rms_energy < 0.8f) &&                // 降低最小能量閾值
        (features->zero_crossing_rate > 0.01f && features->zero_crossing_rate < 0.5f) && // 放寬零穿越率範圍
        (features->spectral_centroid > 0.05f && features->spectral_centroid < 0.95f);    // 放寬頻譜範圍
}

// ========== 語音活動檢測 (VAD) 系統 ==========

// VAD 狀態變量
static VADState vad_current_state = VAD_SILENCE;
static int speech_frame_count = 0;      // 連續語音幀計數
static int silence_frame_count = 0;     // 連續靜音幀計數
static unsigned long speech_start_time = 0;
static unsigned long speech_end_time = 0;

// 語音緩衝系統
float speech_buffer[SPEECH_BUFFER_SIZE];
int speech_buffer_length = 0;

/**
 * 語音活動檢測主處理函數
 */
VADResult audio_vad_process(const AudioFeatures *features)
{
    VADResult result;
    result.state = vad_current_state;
    result.speech_detected = false;
    result.speech_complete = false;
    result.energy_level = features->rms_energy;
    result.duration_ms = 0;

    unsigned long current_time = millis();
    bool is_speech_energy = (features->rms_energy > VAD_ENERGY_THRESHOLD);
    
    switch (vad_current_state)
    {
    case VAD_SILENCE:
        if (is_speech_energy && features->is_voice_detected)
        {
            speech_frame_count++;
            silence_frame_count = 0;
            
            if (speech_frame_count >= VAD_START_FRAMES)
            {
                // 語音開始
                vad_current_state = VAD_SPEECH_START;
                speech_start_time = current_time;
                speech_buffer_length = 0;  // 清空語音緩衝
                result.state = VAD_SPEECH_START;
                result.speech_detected = true;
                
                Serial.println("🎤 語音開始檢測");
            }
        }
        else
        {
            speech_frame_count = 0;
        }
        break;

    case VAD_SPEECH_START:
        vad_current_state = VAD_SPEECH_ACTIVE;
        result.state = VAD_SPEECH_ACTIVE;
        // 繼續到 SPEECH_ACTIVE 處理
        
    case VAD_SPEECH_ACTIVE:
        if (is_speech_energy || features->is_voice_detected)
        {
            silence_frame_count = 0;
            result.speech_detected = true;
        }
        else
        {
            silence_frame_count++;
            
            if (silence_frame_count >= VAD_END_FRAMES)
            {
                // 語音結束
                speech_end_time = current_time;
                unsigned long duration = speech_end_time - speech_start_time;
                
                if (duration >= VAD_MIN_SPEECH_DURATION)
                {
                    // 有效的語音段落
                    vad_current_state = VAD_SPEECH_END;
                    result.state = VAD_SPEECH_END;
                    result.speech_complete = true;
                    result.duration_ms = duration;
                    
                    Serial.printf("✅ 語音結束 - 持續時間: %lu ms\n", duration);
                }
                else
                {
                    // 太短，回到靜音狀態
                    Serial.printf("⚠️  語音太短 (%lu ms)，忽略\n", duration);
                    audio_vad_reset();
                }
            }
        }
        
        // 超時保護
        if ((current_time - speech_start_time) > VAD_MAX_SPEECH_DURATION)
        {
            Serial.println("⏰ 語音超時，強制結束");
            vad_current_state = VAD_SPEECH_END;
            result.state = VAD_SPEECH_END;
            result.speech_complete = true;
            result.duration_ms = current_time - speech_start_time;
        }
        break;

    case VAD_SPEECH_END:
        // 處理完整語音後重置到靜音狀態
        audio_vad_reset();
        result.state = VAD_SILENCE;
        break;
    }

    return result;
}

/**
 * 重置 VAD 狀態
 */
void audio_vad_reset()
{
    vad_current_state = VAD_SILENCE;
    speech_frame_count = 0;
    silence_frame_count = 0;
    speech_start_time = 0;
    speech_end_time = 0;
}

/**
 * 收集語音段落到緩衝區（智能管理）
 */
bool audio_collect_speech_segment(const float *frame, size_t frame_size)
{
    // 檢查緩衝區是否有足夠空間
    if (speech_buffer_length + frame_size > SPEECH_BUFFER_SIZE)
    {
        // 緩衝區滿時的策略：保留最後75%的數據，丟棄前面25%
        int keep_samples = SPEECH_BUFFER_SIZE * 3 / 4;  // 保留75%
        int discard_samples = SPEECH_BUFFER_SIZE - keep_samples;
        
        // 將後面的數據移到前面
        memmove(speech_buffer, &speech_buffer[discard_samples], keep_samples * sizeof(float));
        speech_buffer_length = keep_samples;
        
        // 靜默處理，減少警告頻率
        static unsigned long last_warning = 0;
        unsigned long now = millis();
        if (now - last_warning > 2000) {
            Serial.printf("🔄 緩衝區循環使用 - 保留最新 %.1f 秒語音\n", (float)keep_samples / SAMPLE_RATE);
            last_warning = now;
        }
    }
    
    // 將音頻幀複製到語音緩衝區
    memcpy(&speech_buffer[speech_buffer_length], frame, frame_size * sizeof(float));
    speech_buffer_length += frame_size;
    
    return true;
}

/**
 * 處理完整的語音段落
 */
void audio_process_complete_speech()
{
    if (speech_buffer_length == 0)
    {
        Serial.println("❌ 沒有語音數據可處理");
        return;
    }
    
    Serial.printf("🔄 處理完整語音段落 - 長度: %d 樣本\n", speech_buffer_length);
    
    // 這裡會在後面與關鍵字檢測整合
    // 現在先重置緩衝區
    speech_buffer_length = 0;
}