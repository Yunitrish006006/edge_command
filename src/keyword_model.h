#ifndef KEYWORD_MODEL_H
#define KEY    // 模型參數 - 降低闾值以提高檢測靈敏度
    static constexpr float SILENCE_THRESHOLD = 0.005f;   // 靜音闾值
    static constexpr float ACTIVATION_THRESHOLD = 0.60f; // 降低激活闾值到60%
    static constexpr float CONFIDENCE_THRESHOLD = 0.60f; // 60%以上信心度才檢測tatic constexpr float ACTIVATION_THRESHOLD = 0.80f; // 平衡的激活闾值D_MODEL_H

#include "audio_capture.h"
#include <Arduino.h>

// 關鍵字類別定義
enum KeywordClass
{
    KEYWORD_SILENCE = 0, // 靜音/背景噪音
    KEYWORD_UNKNOWN = 1, // 未知語音
    KEYWORD_YES = 2,     // "好的"/"是的"/"OK"
    KEYWORD_NO = 3,      // "不要"/"不是"/"停止"
    KEYWORD_HELLO = 4,   // "你好"/"嗨"/"Hello"
    KEYWORD_COUNT = 5    // 總類別數
};

// 關鍵字檢測結果
struct KeywordResult
{
    KeywordClass detected_keyword;      // 檢測到的關鍵字
    float confidence;                   // 置信度 (0.0-1.0)
    float probabilities[KEYWORD_COUNT]; // 各類別機率
    bool is_activation;                 // 是否為激活關鍵字
    unsigned long timestamp;            // 檢測時間戳
};

// 特徵向量維度
#define FEATURE_SIZE 13    // MFCC特徵數量
#define SEQUENCE_LENGTH 16 // 時序長度(幀數)
#define TOTAL_FEATURES (FEATURE_SIZE * SEQUENCE_LENGTH)

// 關鍵字檢測器類別
class KeywordDetector
{
private:
    // 模型參數 - 極度保守的檢測閾值
    static constexpr float SILENCE_THRESHOLD = 0.005f;   // 靜音閾值
    static constexpr float ACTIVATION_THRESHOLD = 0.95f; // 極高激活閾值
    static constexpr float CONFIDENCE_THRESHOLD = 0.80f; // 80%以上信心度才檢測

    // 特徵緩衝區
    float feature_buffer[SEQUENCE_LENGTH][FEATURE_SIZE];
    int buffer_index;
    bool buffer_full;

    // 檢測狀態
    KeywordResult last_result;
    unsigned long last_detection_time;
    static constexpr unsigned long COOLDOWN_MS = 1000; // 1秒冷卻時間

    // 統計資訊
    int total_detections;
    int false_positives;
    float running_noise_level;

public:
    KeywordDetector();

    // 主要檢測函數
    KeywordResult detect(const AudioFeatures &audio_features);

    // 特徵提取
    void extract_mfcc_features(const float *audio_frame, float *mfcc_features);

    // 模型推理 (簡化版)
    KeywordClass classify_features(const float features[TOTAL_FEATURES], float *confidence);

    // 輔助函數
    void reset();
    void print_stats();
    bool is_in_cooldown();

    // 調試函數
    void print_feature_buffer();
    void calibrate_noise_level(const AudioFeatures &features);

private:
    // 內部特徵處理
    void update_feature_buffer(const float *new_features);
    void flatten_features(float *output);

    // 簡化的MFCC計算
    void compute_mel_filters(const float *fft_power, float *mel_output);
    void apply_dct(const float *mel_input, float *mfcc_output);

    // 分類器權重 (預訓練的簡化權重)
    float get_keyword_score(const float *features, KeywordClass keyword);
    void softmax(float *input, int size);
};

// 全局關鍵字檢測器實例
extern KeywordDetector keyword_detector;

// 便利函數
const char *keyword_to_string(KeywordClass keyword);
const char *get_keyword_emoji(KeywordClass keyword);
bool is_activation_keyword(KeywordClass keyword);

// 預定義的關鍵字模式 (簡化的特徵模板)
struct KeywordPattern
{
    float energy_range[2];    // 能量範圍 [min, max]
    float zcr_range[2];       // 零穿越率範圍
    float duration_range[2];  // 持續時間範圍 (秒)
    float spectral_peak_freq; // 主要頻譜峰值
    const char *examples[3];  // 範例詞彙
};

// 關鍵字模式定義
extern const KeywordPattern keyword_patterns[KEYWORD_COUNT];

#endif // KEYWORD_MODEL_H