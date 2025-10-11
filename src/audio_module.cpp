#include "audio_module.h"
#include "esp_log.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <functional>
#include <stdarg.h>

// 數學常數
#ifndef PI
#define PI 3.14159265359f
#endif

static const char *TAG = "AudioModule";

/**
 * 調試輸出輔助方法
 */
void AudioCaptureModule::debug_print(const char *message) const
{
    if (debug_enabled && message)
    {
        Serial.println(message);
    }
}

void AudioCaptureModule::debug_printf(const char *format, ...) const
{
    if (debug_enabled && format)
    {
        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        Serial.print(buffer);
    }
}

/**
 * 建構函數
 */
AudioCaptureModule::AudioCaptureModule()
    : processed_buffer(nullptr), normalized_buffer(nullptr), frame_buffer(nullptr), frame_write_pos(0), frame_ready_flag(false), vad_current_state(VAD_SILENCE), speech_frame_count(0), silence_frame_count(0), speech_start_time(0), speech_end_time(0), speech_buffer(nullptr), speech_buffer_length(0), is_initialized(false), is_running(false), debug_enabled(false)
{
    debug_print("AudioCaptureModule 建構函數");
}

/**
 * 解構函數
 */
AudioCaptureModule::~AudioCaptureModule()
{
    deinitialize();
    debug_print("AudioCaptureModule 解構函數");
}

/**
 * 初始化模組
 */
bool AudioCaptureModule::initialize()
{
    debug_print("初始化音訊擷取模組...");

    if (is_initialized)
    {
        debug_print("模組已經初始化");
        return true;
    }

    // 分配記憶體緩衝區
    processed_buffer = new int16_t[AUDIO_BUFFER_SIZE];
    normalized_buffer = new float[AUDIO_FRAME_SIZE];
    frame_buffer = new int16_t[AUDIO_BUFFER_SIZE * 2]; // 支持重疊
    speech_buffer = new float[SPEECH_BUFFER_SIZE];

    if (!processed_buffer || !normalized_buffer || 
        !frame_buffer || !speech_buffer)
    {
        debug_print("記憶體分配失敗！");
        deinitialize();
        return false;
    }

    // 清零緩衝區
    memset(processed_buffer, 0, AUDIO_BUFFER_SIZE * sizeof(int16_t));
    memset(normalized_buffer, 0, AUDIO_FRAME_SIZE * sizeof(float));
    memset(frame_buffer, 0, AUDIO_BUFFER_SIZE * 2 * sizeof(int16_t));
    memset(speech_buffer, 0, SPEECH_BUFFER_SIZE * sizeof(float));

    // 初始化 INMP441 模組
    if (!inmp441.initialize())
    {
        debug_print("INMP441 模組初始化失敗！");
        deinitialize();
        return false;
    }
    
    // 設置 INMP441 回調函數
    inmp441.set_audio_data_callback([this](const int16_t *audio_data, size_t sample_count) {
        this->on_inmp441_audio_data(audio_data, sample_count);
    });
    
    inmp441.set_state_change_callback([this](INMP441State state, const char *message) {
        this->on_inmp441_state_change(state, message);
    });

    // 重置 VAD 狀態
    reset_vad();

    is_initialized = true;
    debug_print("音訊擷取模組初始化成功！");
    return true;
}

/**
 * 初始化模組（使用自定義 INMP441 配置）
 */
bool AudioCaptureModule::initialize(const INMP441Config &inmp441_config)
{
    debug_print("初始化音訊擷取模組（自定義 INMP441 配置）...");

    if (is_initialized)
    {
        debug_print("模組已經初始化");
        return true;
    }

    // 分配記憶體緩衝區
    processed_buffer = new int16_t[AUDIO_BUFFER_SIZE];
    normalized_buffer = new float[AUDIO_FRAME_SIZE];
    frame_buffer = new int16_t[AUDIO_BUFFER_SIZE * 2]; // 支持重疊
    speech_buffer = new float[SPEECH_BUFFER_SIZE];

    if (!processed_buffer || !normalized_buffer || 
        !frame_buffer || !speech_buffer)
    {
        debug_print("記憶體分配失敗！");
        deinitialize();
        return false;
    }

    // 清零緩衝區
    memset(processed_buffer, 0, AUDIO_BUFFER_SIZE * sizeof(int16_t));
    memset(normalized_buffer, 0, AUDIO_FRAME_SIZE * sizeof(float));
    memset(frame_buffer, 0, AUDIO_BUFFER_SIZE * 2 * sizeof(int16_t));
    memset(speech_buffer, 0, SPEECH_BUFFER_SIZE * sizeof(float));

    // 使用自定義配置初始化 INMP441 模組
    if (!inmp441.initialize(inmp441_config))
    {
        debug_print("INMP441 模組初始化失敗！");
        deinitialize();
        return false;
    }
    
    // 設置 INMP441 回調函數
    inmp441.set_audio_data_callback([this](const int16_t *audio_data, size_t sample_count) {
        this->on_inmp441_audio_data(audio_data, sample_count);
    });
    
    inmp441.set_state_change_callback([this](INMP441State state, const char *message) {
        this->on_inmp441_state_change(state, message);
    });

    // 重置 VAD 狀態
    reset_vad();

    is_initialized = true;
    debug_print("音訊擷取模組初始化成功（自定義配置）！");
    return true;
}

/**
 * 去初始化模組
 */
void AudioCaptureModule::deinitialize()
{
    if (is_running)
    {
        stop_capture();
    }

    if (is_initialized)
    {
        inmp441.deinitialize();
        is_initialized = false;
    }

    // 釋放記憶體
    delete[] processed_buffer;
    delete[] normalized_buffer;
    delete[] frame_buffer;
    delete[] speech_buffer;

    processed_buffer = nullptr;
    normalized_buffer = nullptr;
    frame_buffer = nullptr;
    speech_buffer = nullptr;

    debug_print("音訊擷取模組去初始化完成");
}

/**
 * 開始音訊擷取
 */
bool AudioCaptureModule::start_capture()
{
    if (!is_initialized)
    {
        debug_print("模組尚未初始化，無法開始擷取");
        return false;
    }

    if (is_running)
    {
        debug_print("音訊擷取已在運行中");
        return true;
    }

    if (!inmp441.start())
    {
        debug_print("INMP441 啟動失敗");
        return false;
    }

    is_running = true;
    debug_print("音訊擷取已開始");
    return true;
}

/**
 * 停止音訊擷取
 */
void AudioCaptureModule::stop_capture()
{
    if (is_running)
    {
        inmp441.stop();
        is_running = false;
        debug_print("音訊擷取已停止");
    }
}

/**
 * 正規化音訊數據
 */
void AudioCaptureModule::normalize_audio(int16_t *input, float *output, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        output[i] = (float)input[i] / AUDIO_MAX_AMPLITUDE * AUDIO_NORMALIZATION_FACTOR;

        // 限制範圍
        if (output[i] > 1.0f)
            output[i] = 1.0f;
        if (output[i] < -1.0f)
            output[i] = -1.0f;
    }
}

/**
 * 應用窗函數
 */
void AudioCaptureModule::apply_window_function(float *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        float window_val = 0.5f * (1.0f - cos(2.0f * PI * i / (length - 1)));
        data[i] *= window_val;
    }
}

/**
 * 計算 RMS 能量
 */
float AudioCaptureModule::calculate_rms(float *data, size_t length)
{
    float sum = 0.0f;
    for (size_t i = 0; i < length; i++)
    {
        sum += data[i] * data[i];
    }
    return sqrt(sum / length);
}

/**
 * 計算零穿越率
 */
float AudioCaptureModule::calculate_zero_crossing_rate(int16_t *data, size_t length)
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
 * 提取音訊特徵
 */
void AudioCaptureModule::extract_audio_features(float *frame, AudioFeatures *features)
{
    // 計算 RMS 能量
    features->rms_energy = calculate_rms(frame, AUDIO_FRAME_SIZE);

    // 轉換為整數以計算零穿越率
    int16_t temp_samples[AUDIO_FRAME_SIZE];
    for (size_t i = 0; i < AUDIO_FRAME_SIZE; i++)
    {
        temp_samples[i] = (int16_t)(frame[i] * AUDIO_MAX_AMPLITUDE);
    }

    features->zero_crossing_rate = calculate_zero_crossing_rate(temp_samples, AUDIO_FRAME_SIZE);

    // 計算頻譜重心（簡化版）
    float high_freq_energy = 0.0f;
    float total_energy = 0.0f;

    for (size_t i = 0; i < AUDIO_FRAME_SIZE; i++)
    {
        float energy = frame[i] * frame[i];
        total_energy += energy;

        if (i > AUDIO_FRAME_SIZE / 2)
        {
            high_freq_energy += energy;
        }
    }

    features->spectral_centroid = (total_energy > 0) ? (high_freq_energy / total_energy) : 0.0f;

    // 確保特徵值有足夠精度
    if (features->rms_energy < 0.001f && total_energy > 0.0f)
    {
        features->rms_energy = sqrtf(total_energy / AUDIO_FRAME_SIZE);
    }

    // 語音檢測邏輯
    features->is_voice_detected =
        (features->rms_energy > 0.001f && features->rms_energy < 0.8f) &&
        (features->zero_crossing_rate > 0.01f && features->zero_crossing_rate < 0.5f) &&
        (features->spectral_centroid > 0.05f && features->spectral_centroid < 0.95f);
}

/**
 * 檢查幀是否準備好
 */
bool AudioCaptureModule::check_frame_ready(int16_t *new_samples, size_t sample_count)
{
    for (size_t i = 0; i < sample_count; i++)
    {
        frame_buffer[frame_write_pos] = new_samples[i];
        frame_write_pos++;

        if (frame_write_pos >= AUDIO_FRAME_SIZE)
        {
            frame_ready_flag = true;

            // 移動數據以支持重疊處理
            memmove(frame_buffer, frame_buffer + AUDIO_FRAME_SIZE - AUDIO_FRAME_OVERLAP,
                    AUDIO_FRAME_OVERLAP * sizeof(int16_t));
            frame_write_pos = AUDIO_FRAME_OVERLAP;

            return true;
        }
    }
    return false;
}

/**
 * 獲取當前幀
 */
void AudioCaptureModule::get_current_frame(float *frame_output)
{
    if (!frame_ready_flag)
        return;

    int16_t temp_frame[AUDIO_FRAME_SIZE];
    memcpy(temp_frame, frame_buffer, AUDIO_FRAME_SIZE * sizeof(int16_t));

    normalize_audio(temp_frame, frame_output, AUDIO_FRAME_SIZE);
    apply_window_function(frame_output, AUDIO_FRAME_SIZE);

    frame_ready_flag = false;
}

/**
 * 處理語音活動檢測
 */
VADResult AudioCaptureModule::process_vad(const AudioFeatures *features)
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
                vad_current_state = VAD_SPEECH_START;
                speech_start_time = current_time;
                speech_buffer_length = 0;
                result.state = VAD_SPEECH_START;
                result.speech_detected = true;

                debug_print("🎤 語音開始檢測");
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
        // 繼續到 VAD_SPEECH_ACTIVE 處理

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
                speech_end_time = current_time;
                unsigned long duration = speech_end_time - speech_start_time;

                if (duration >= VAD_MIN_SPEECH_DURATION)
                {
                    vad_current_state = VAD_SPEECH_END;
                    result.state = VAD_SPEECH_END;
                    result.speech_complete = true;
                    result.duration_ms = duration;

                    debug_printf("✅ 語音結束 - 持續時間: %lu ms\n", duration);
                }
                else
                {
                    debug_printf("⚠️  語音太短 (%lu ms)，忽略\n", duration);
                    reset_vad();
                }
            }
        }

        // 超時保護
        if ((current_time - speech_start_time) > VAD_MAX_SPEECH_DURATION)
        {
            debug_print("⏰ 語音超時，強制結束");
            vad_current_state = VAD_SPEECH_END;
            result.state = VAD_SPEECH_END;
            result.speech_complete = true;
            result.duration_ms = current_time - speech_start_time;
        }
        break;

    case VAD_SPEECH_END:
        reset_vad();
        result.state = VAD_SILENCE;
        break;
    }

    return result;
}

/**
 * 收集語音數據
 */
bool AudioCaptureModule::collect_speech_data(const float *frame, size_t frame_size)
{
    if (speech_buffer_length + frame_size > SPEECH_BUFFER_SIZE)
    {
        // 緩衝區滿時保留最新數據
        int keep_samples = SPEECH_BUFFER_SIZE * 3 / 4;
        int discard_samples = SPEECH_BUFFER_SIZE - keep_samples;

        memmove(speech_buffer, &speech_buffer[discard_samples], keep_samples * sizeof(float));
        speech_buffer_length = keep_samples;

        static unsigned long last_warning = 0;
        unsigned long now = millis();
        if (now - last_warning > 2000)
        {
            debug_printf("🔄 緩衝區循環使用 - 保留最新 %.1f 秒語音\n", (float)keep_samples / AUDIO_SAMPLE_RATE);
            last_warning = now;
        }
    }

    memcpy(&speech_buffer[speech_buffer_length], frame, frame_size * sizeof(float));
    speech_buffer_length += frame_size;

    return true;
}

/**
 * 處理完整語音段落
 */
void AudioCaptureModule::process_complete_speech_segment()
{
    if (speech_buffer_length == 0) return;

    debug_printf("🔄 處理完整語音段落 - 長度: %d 樣本\n", speech_buffer_length);

    // 調用語音完成回調
    if (speech_complete_callback)
    {
        unsigned long duration = (speech_end_time - speech_start_time);
        speech_complete_callback(speech_buffer, speech_buffer_length, duration);
    }

    // 重置緩衝區
    speech_buffer_length = 0;
}

/**
 * 主要音訊處理循環
 */
void AudioCaptureModule::process_audio_loop()
{
    if (!is_initialized || !is_running) return;

    // 讓 INMP441 模組讀取並處理音訊數據
    // 這會觸發 on_inmp441_audio_data 回調
    inmp441.read_audio_frame();
}

/**
 * 設置回調函數
 */
void AudioCaptureModule::set_audio_frame_callback(AudioFrameCallback callback)
{
    audio_frame_callback = callback;
}

void AudioCaptureModule::set_vad_callback(VADCallback callback)
{
    vad_callback = callback;
}

void AudioCaptureModule::set_speech_complete_callback(SpeechCompleteCallback callback)
{
    speech_complete_callback = callback;
}

/**
 * 重置 VAD 狀態
 */
void AudioCaptureModule::reset_vad()
{
    vad_current_state = VAD_SILENCE;
    speech_frame_count = 0;
    silence_frame_count = 0;
    speech_start_time = 0;
    speech_end_time = 0;
}

/**
 * 清除語音緩衝區
 */
void AudioCaptureModule::clear_speech_buffer()
{
    speech_buffer_length = 0;
    if (speech_buffer)
    {
        memset(speech_buffer, 0, SPEECH_BUFFER_SIZE * sizeof(float));
    }
}

/**
 * 獲取音訊統計信息
 */
AudioCaptureModule::AudioStats AudioCaptureModule::get_audio_stats() const
{
    AudioStats stats = {0};
    
    if (!processed_buffer) return stats;
    
    // 計算當前緩衝區的統計信息
    int32_t min_val = processed_buffer[0];
    int32_t max_val = processed_buffer[0];
    int64_t sum = 0;
    
    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++)
    {
        int16_t sample = processed_buffer[i];
        if (sample < min_val) min_val = sample;
        if (sample > max_val) max_val = sample;
        sum += abs(sample);
    }
    
    stats.min_amplitude = min_val;
    stats.max_amplitude = max_val;
    stats.avg_amplitude = sum / AUDIO_BUFFER_SIZE;
    stats.samples_processed = AUDIO_BUFFER_SIZE;
    stats.last_activity_time = millis();
    
    return stats;
}

/**
 * INMP441 音訊數據回調處理
 */
void AudioCaptureModule::on_inmp441_audio_data(const int16_t *audio_data, size_t sample_count)
{
    if (!is_running || !audio_data || sample_count == 0) return;

    // 將音訊數據複製到處理緩衝區
    size_t samples_to_process = min(sample_count, (size_t)AUDIO_BUFFER_SIZE);
    memcpy(processed_buffer, audio_data, samples_to_process * sizeof(int16_t));

    // 檢查幀是否準備好
    if (check_frame_ready(processed_buffer, samples_to_process))
    {
        // 獲取處理後的音訊幀
        float current_frame[AUDIO_FRAME_SIZE];
        get_current_frame(current_frame);

        // 提取音訊特徵
        AudioFeatures features;
        extract_audio_features(current_frame, &features);

        // 調用音訊幀回調
        if (audio_frame_callback)
        {
            audio_frame_callback(features);
        }

        // 處理 VAD
        VADResult vad_result = process_vad(&features);

        // 調用 VAD 回調
        if (vad_callback)
        {
            vad_callback(vad_result);
        }

        // 在語音進行中收集數據
        if (vad_result.state == VAD_SPEECH_ACTIVE)
        {
            collect_speech_data(current_frame, AUDIO_FRAME_SIZE);
        }

        // 語音完成時處理
        if (vad_result.speech_complete)
        {
            process_complete_speech_segment();
        }
    }
}

/**
 * INMP441 狀態變更回調處理
 */
void AudioCaptureModule::on_inmp441_state_change(INMP441State state, const char *message)
{
    if (debug_enabled)
    {
        debug_printf("🔄 INMP441 狀態變更: %s", inmp441_state_to_string(state));
        if (message)
        {
            debug_printf(" - %s", message);
        }
        debug_print("");
    }

    // 根據 INMP441 狀態調整模組狀態
    if (state == INMP441_ERROR)
    {
        debug_print("⚠️  INMP441 發生錯誤，停止音訊擷取");
        is_running = false;
    }
}

/**
 * 配置 INMP441 模組
 */
bool AudioCaptureModule::configure_inmp441(const INMP441Config &config)
{
    if (is_running)
    {
        debug_print("❌ 無法在運行中配置 INMP441");
        return false;
    }

    return inmp441.set_config(config);
}