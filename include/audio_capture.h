#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include "driver/i2s.h"
#include <Arduino.h>

// I2S 引腳定義 (使用安全的 GPIO 引腳)
#define I2S_WS_PIN 42  // WS (Word Select) 信號 - GPIO42
#define I2S_SCK_PIN 41 // SCK (Serial Clock) 時鐘信號 - GPIO41
#define I2S_SD_PIN 2   // SD (Serial Data) 數據信號 - GPIO2

// I2S 配置參數
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 16000  // 16kHz 採樣率，適合語音識別
#define BITS_PER_SAMPLE 32 // INMP441 輸出 24-bit，但使用 32-bit 容器
#define CHANNELS 1         // 單聲道
#define BUFFER_SIZE 512    // 每次讀取的樣本數
#define DMA_BUF_COUNT 8    // DMA 緩衝區數量
#define DMA_BUF_LEN 64     // 每個 DMA 緩衝區長度

// 音頻預處理參數
#define FRAME_SIZE 256            // 每幀音頻樣本數（適合模型輸入）
#define FRAME_OVERLAP 128         // 幀重疊樣本數（50%重疊）
#define NOISE_FLOOR -40           // 噪聲底線（dB）
#define MAX_AMPLITUDE 32767       // 16-bit 最大振幅
#define NORMALIZATION_FACTOR 0.8f // 正規化係數

// 音頻緩衝區
extern int32_t audio_buffer[BUFFER_SIZE];
extern int16_t processed_audio[BUFFER_SIZE];
extern float normalized_audio[FRAME_SIZE]; // 正規化後的音頻幀
extern float feature_buffer[FRAME_SIZE];   // 特徵緩衝區

// 基本音頻處理函數
bool audio_init();
size_t audio_read(int32_t *buffer, size_t buffer_size);
void audio_process(int32_t *raw_data, int16_t *processed_data, size_t length);
void audio_deinit();

// 音頻預處理函數
void audio_normalize(int16_t *input, float *output, size_t length);
void audio_apply_window(float *data, size_t length);
float audio_calculate_rms(float *data, size_t length);
float audio_calculate_zero_crossing_rate(int16_t *data, size_t length);
bool audio_frame_ready(int16_t *new_samples, size_t sample_count);
void audio_get_current_frame(float *frame_output);

// 音頻特徵結構體
struct AudioFeatures
{
    float rms_energy;         // RMS 能量
    float zero_crossing_rate; // 零穿越率
    float spectral_centroid;  // 頻譜重心（簡化版）
    bool is_voice_detected;   // 語音檢測標誌
};

// 語音活動檢測 (VAD) 狀態
enum VADState
{
    VAD_SILENCE,      // 靜音狀態
    VAD_SPEECH_START, // 語音開始
    VAD_SPEECH_ACTIVE,// 語音進行中
    VAD_SPEECH_END    // 語音結束
};

// VAD 結果結構體
struct VADResult
{
    VADState state;
    bool speech_detected;
    bool speech_complete;  // 完整語音段落結束
    float energy_level;
    unsigned long duration_ms;
};

// VAD 參數設定  
#define VAD_ENERGY_THRESHOLD 0.015f   // 語音能量閾值（降低以提高靈敏度）
#define VAD_START_FRAMES 2            // 連續2幀就開始（更快反應）
#define VAD_END_FRAMES 15             // 連續15幀靜音才結束（避免語音中間停頓）
#define VAD_MIN_SPEECH_DURATION 300   // 最小語音持續時間 (ms)
#define VAD_MAX_SPEECH_DURATION 4000  // 最大語音持續時間 (ms)

// 語音緩衝系統
#define SPEECH_BUFFER_SIZE 16384  // 語音緩衝區大小 (樣本數) - 支持約1.0秒@16kHz
extern float speech_buffer[SPEECH_BUFFER_SIZE];
extern int speech_buffer_length;

// 特徵提取函數
void audio_extract_features(float *frame, AudioFeatures *features);

// VAD 相關函數
VADResult audio_vad_process(const AudioFeatures *features);
void audio_vad_reset();
bool audio_collect_speech_segment(const float *frame, size_t frame_size);
void audio_process_complete_speech();

#endif // AUDIO_CAPTURE_H