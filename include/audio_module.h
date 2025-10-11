#ifndef AUDIO_MODULE_H
#define AUDIO_MODULE_H

#include <Arduino.h>
#include <functional>
#include "inmp441_module.h"
#include "debug_print.h"

// 音訊處理配置常數
#define AUDIO_SAMPLE_RATE 16000  // 16kHz 採樣率
#define AUDIO_BUFFER_SIZE 512    // 每次讀取的樣本數

#define AUDIO_FRAME_SIZE 256            // 每幀音頻樣本數
#define AUDIO_FRAME_OVERLAP 128         // 幀重疊樣本數
#define AUDIO_MAX_AMPLITUDE 32767       // 16-bit 最大振幅
#define AUDIO_NORMALIZATION_FACTOR 0.8f // 正規化係數

// VAD 配置
#define VAD_ENERGY_THRESHOLD 0.015f    // 語音能量閾值
#define VAD_START_FRAMES 2             // 開始檢測需要的連續幀數
#define VAD_END_FRAMES 15              // 結束檢測需要的靜音幀數
#define VAD_MIN_SPEECH_DURATION 300    // 最小語音持續時間 (ms)
#define VAD_MAX_SPEECH_DURATION 4000   // 最大語音持續時間 (ms)

#define SPEECH_BUFFER_SIZE 16384  // 語音緩衝區大小

// 音頻特徵結構體
struct AudioFeatures
{
    float rms_energy;         // RMS 能量
    float zero_crossing_rate; // 零穿越率
    float spectral_centroid;  // 頻譜重心
    bool is_voice_detected;   // 語音檢測標誌
};

// VAD 狀態
enum VADState
{
    VAD_SILENCE,       // 靜音狀態
    VAD_SPEECH_START,  // 語音開始
    VAD_SPEECH_ACTIVE, // 語音進行中
    VAD_SPEECH_END     // 語音結束
};

// VAD 結果結構體
struct VADResult
{
    VADState state;
    bool speech_detected;
    bool speech_complete;
    float energy_level;
    unsigned long duration_ms;
};

// 音訊事件回調函數類型
typedef std::function<void(const AudioFeatures &features)> AudioFrameCallback;
typedef std::function<void(const VADResult &result)> VADCallback;
typedef std::function<void(const float *speech_data, size_t length, unsigned long duration_ms)> SpeechCompleteCallback;

/**
 * 音訊擷取模組類別
 * 封裝所有音訊相關功能到統一介面
 */
class AudioCaptureModule
{
private:
    // INMP441 麥克風模組
    INMP441Module inmp441;

    // 音訊處理緩衝區
    int16_t *processed_buffer;
    float *normalized_buffer;

    // 幀處理相關
    int16_t *frame_buffer;
    size_t frame_write_pos;
    bool frame_ready_flag;

    // VAD 狀態變量
    VADState vad_current_state;
    int speech_frame_count;
    int silence_frame_count;
    unsigned long speech_start_time;
    unsigned long speech_end_time;

    // 語音緩衝系統
    float *speech_buffer;
    int speech_buffer_length;

    // 回調函數
    AudioFrameCallback audio_frame_callback;
    VADCallback vad_callback;
    SpeechCompleteCallback speech_complete_callback;

    // 模組狀態
    bool is_initialized;
    bool is_running;

    // 通用 Debug 模組
    DebugPrint debug;

    // 內部方法
    void normalize_audio(int16_t *input, float *output, size_t length);
    void apply_window_function(float *data, size_t length);
    float calculate_rms(float *data, size_t length);
    float calculate_zero_crossing_rate(int16_t *data, size_t length);
    void extract_audio_features(float *frame, AudioFeatures *features);
    bool check_frame_ready(int16_t *new_samples, size_t sample_count);
    void get_current_frame(float *frame_output);
    VADResult process_vad(const AudioFeatures *features);
    bool collect_speech_data(const float *frame, size_t frame_size);
    void process_complete_speech_segment();
    
    // INMP441 回調方法
    void on_inmp441_audio_data(const int16_t *audio_data, size_t sample_count);
    void on_inmp441_state_change(INMP441State state, const char *message);

public:
    // 建構函數與解構函數
    AudioCaptureModule();
    ~AudioCaptureModule();

    // 基本模組控制
    bool initialize();
    bool initialize(const INMP441Config &inmp441_config);
    void deinitialize();
    bool start_capture();
    void stop_capture();

    // 主要處理方法
    void process_audio_loop();
    
    // INMP441 相關方法
    INMP441Module& get_inmp441_module() { return inmp441; }
    bool configure_inmp441(const INMP441Config &config);

    // 回調註冊方法
    void set_audio_frame_callback(AudioFrameCallback callback);
    void set_vad_callback(VADCallback callback);
    void set_speech_complete_callback(SpeechCompleteCallback callback);

    // 狀態查詢方法
    bool is_module_initialized() const { return is_initialized; }
    bool is_capture_running() const { return is_running; }
    VADState get_current_vad_state() const { return vad_current_state; }
    int get_speech_buffer_length() const { return speech_buffer_length; }

    // 配置方法
    void reset_vad();
    void clear_speech_buffer();

    // 調試控制方法
    void set_debug(bool enable) { debug.set_debug(enable); }
    bool is_debug_enabled() const { return debug.is_debug_enabled(); }
    DebugPrint& get_debug() { return debug; }

    // 音訊統計信息（調試用）
    struct AudioStats
    {
        int32_t min_amplitude;
        int32_t max_amplitude;
        int32_t avg_amplitude;
        size_t samples_processed;
        unsigned long last_activity_time;
    };

    AudioStats get_audio_stats() const;
};

#endif // AUDIO_MODULE_H