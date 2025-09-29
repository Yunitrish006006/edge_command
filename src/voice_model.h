#ifndef VOICE_MODEL_H
#define VOICE_MODEL_H

#include "audio_module.h"
#include <Arduino.h>

// 語音模型輸出類別
enum VoiceModelOutput
{
    VOICE_SILENCE = 0,    // 靜音
    VOICE_BACKGROUND = 1, // 背景噪音
    VOICE_SPEECH = 2,     // 語音/說話
    VOICE_MUSIC = 3,      // 音樂
    VOICE_UNKNOWN = 4     // 未知/不確定
};

// 語音模型結果結構體
struct VoiceModelResult
{
    VoiceModelOutput classification; // 分類結果
    float confidence;                // 置信度 (0.0 - 1.0)
    float voice_probability;         // 語音機率
    float activity_level;            // 活動水準

    // 詳細特徵資訊
    float energy_score;   // 能量評分
    float spectral_score; // 頻譜評分
    float temporal_score; // 時域評分
};

// 語音模型類別
class VoiceModel
{
private:
    // 模型參數（可調整的閥值）
    static constexpr float SILENCE_ENERGY_THRESHOLD = 0.005f;
    static constexpr float SPEECH_ENERGY_MIN = 0.01f;
    static constexpr float SPEECH_ENERGY_MAX = 0.7f;
    static constexpr float SPEECH_ZCR_MIN = 0.02f;
    static constexpr float SPEECH_ZCR_MAX = 0.35f;
    static constexpr float MUSIC_ZCR_MAX = 0.15f;
    static constexpr float NOISE_ZCR_MIN = 0.3f;

    // 歷史狀態（用於平滑處理）
    static constexpr int HISTORY_SIZE = 10;
    VoiceModelOutput history[HISTORY_SIZE];
    float confidence_history[HISTORY_SIZE];
    int history_index;
    int frame_count;

    // 統計資訊
    float running_energy_avg;
    float running_zcr_avg;
    int total_frames;

public:
    VoiceModel();

    // 主要推理函數
    VoiceModelResult inference(const AudioFeatures &features);

    // 輔助函數
    void reset();
    VoiceModelOutput get_smoothed_classification();
    float get_average_confidence();

    // 調試和統計函數
    void print_model_stats();
    void calibrate_thresholds(const AudioFeatures &features);

private:
    // 內部計算函數
    float calculate_energy_score(float energy);
    float calculate_spectral_score(float zcr, float spectral_centroid);
    float calculate_temporal_score(float energy, float zcr);
    VoiceModelOutput classify_features(float energy_score, float spectral_score, float temporal_score);
    float calculate_confidence(const AudioFeatures &features, VoiceModelOutput classification);
    void update_running_stats(const AudioFeatures &features);
    void add_to_history(VoiceModelOutput classification, float confidence);
};

// 全局模型實例
extern VoiceModel voice_model;

// 便利函數
const char *voice_output_to_string(VoiceModelOutput output);
const char *get_voice_emoji(VoiceModelOutput output);

#endif // VOICE_MODEL_H