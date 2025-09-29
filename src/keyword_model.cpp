#include "keyword_model.h"
#include <math.h>
#include <string.h>

// 全局關鍵字檢測器實例
KeywordDetector keyword_detector;

// 關鍵字模式定義 (基於常見中英文關鍵字的音頻特徵)
const KeywordPattern keyword_patterns[KEYWORD_COUNT] = {
    // KEYWORD_SILENCE
    {
        .energy_range = {0.0f, 0.01f},
        .zcr_range = {0.0f, 0.5f},
        .duration_range = {0.0f, 10.0f},
        .spectral_peak_freq = 0.0f,
        .examples = {"silence", "background", "quiet"}},

    // KEYWORD_UNKNOWN
    {
        .energy_range = {0.01f, 1.0f},
        .zcr_range = {0.05f, 0.5f},
        .duration_range = {0.2f, 3.0f},
        .spectral_peak_freq = 0.3f,
        .examples = {"speech", "talking", "voice"}},

    // KEYWORD_YES ("好的"/"是的"/"OK")
    {
        .energy_range = {0.1f, 0.8f},
        .zcr_range = {0.08f, 0.25f},
        .duration_range = {0.3f, 1.2f},
        .spectral_peak_freq = 0.4f,
        .examples = {"好的", "是的", "OK"}},

    // KEYWORD_NO ("不要"/"不是"/"停止")
    {
        .energy_range = {0.15f, 0.9f},
        .zcr_range = {0.1f, 0.3f},
        .duration_range = {0.4f, 1.5f},
        .spectral_peak_freq = 0.3f,
        .examples = {"不要", "不是", "停止"}},

    // KEYWORD_HELLO ("你好"/"嗨"/"Hello")
    {
        .energy_range = {0.2f, 0.9f},
        .zcr_range = {0.12f, 0.35f},
        .duration_range = {0.5f, 2.0f},
        .spectral_peak_freq = 0.5f,
        .examples = {"你好", "嗨", "Hello"}}};

KeywordDetector::KeywordDetector()
{
    reset();
}

void KeywordDetector::reset()
{
    memset(feature_buffer, 0, sizeof(feature_buffer));
    buffer_index = 0;
    buffer_full = false;

    last_result.detected_keyword = KEYWORD_SILENCE;
    last_result.confidence = 0.0f;
    last_result.is_activation = false;
    last_result.timestamp = 0;

    last_detection_time = 0;
    total_detections = 0;
    false_positives = 0;
    running_noise_level = 0.01f;
}

KeywordResult KeywordDetector::detect(const AudioFeatures &audio_features)
{
    KeywordResult result;
    result.timestamp = millis();

    // 更新噪音水準校準
    calibrate_noise_level(audio_features);

    // 提取MFCC特徵
    float mfcc_features[FEATURE_SIZE];

    // 簡化的MFCC特徵提取 (基於現有的音頻特徵)
    mfcc_features[0] = audio_features.rms_energy;
    mfcc_features[1] = audio_features.zero_crossing_rate;
    mfcc_features[2] = audio_features.spectral_centroid;

    // 添加一些衍生特徵
    mfcc_features[3] = log10f(audio_features.rms_energy + 1e-10f);                    // 對數能量
    mfcc_features[4] = audio_features.zero_crossing_rate * audio_features.rms_energy; // ZCR-Energy乘積
    mfcc_features[5] = audio_features.spectral_centroid * audio_features.rms_energy;  // SC-Energy乘積

    // 頻譜能量分佈 (模擬不同頻帶的能量)
    for (int i = 6; i < FEATURE_SIZE; i++)
    {
        float freq_weight = (float)(i - 5) / (FEATURE_SIZE - 6);
        mfcc_features[i] = audio_features.rms_energy *
                           sinf(audio_features.spectral_centroid * M_PI * freq_weight);
    }

    // 更新特徵緩衝區
    update_feature_buffer(mfcc_features);

    // 檢查是否有足夠的特徵進行分類
    if (!buffer_full)
    {
        result.detected_keyword = KEYWORD_SILENCE;
        result.confidence = 1.0f;
        result.is_activation = false;
        for (int i = 0; i < KEYWORD_COUNT; i++)
        {
            result.probabilities[i] = (i == KEYWORD_SILENCE) ? 1.0f : 0.0f;
        }
        return result;
    }

    // 扁平化特徵用於分類
    float flattened_features[TOTAL_FEATURES];
    flatten_features(flattened_features);

    // 進行關鍵字分類
    float confidence;
    KeywordClass detected_class = classify_features(flattened_features, &confidence);

    // 填充結果
    result.detected_keyword = detected_class;
    result.confidence = confidence;
    result.is_activation = is_activation_keyword(detected_class) &&
                           confidence > ACTIVATION_THRESHOLD &&
                           !is_in_cooldown();

    // 計算各類別機率 (簡化版softmax)
    for (int i = 0; i < KEYWORD_COUNT; i++)
    {
        float score = get_keyword_score(flattened_features, (KeywordClass)i);
        result.probabilities[i] = expf(score);
    }
    softmax(result.probabilities, KEYWORD_COUNT);

    // 更新統計
    if (result.is_activation)
    {
        last_detection_time = result.timestamp;
        total_detections++;
    }

    last_result = result;
    return result;
}

void KeywordDetector::extract_mfcc_features(const float *audio_frame, float *mfcc_features)
{
    // 這裡實現簡化的MFCC特徵提取
    // 實際應用中可以使用更完整的MFCC算法

    float energy = 0.0f;
    float zcr = 0.0f;

    // 計算能量和零穿越率
    for (int i = 0; i < FRAME_SIZE; i++)
    {
        energy += audio_frame[i] * audio_frame[i];
        if (i > 0 && ((audio_frame[i] >= 0) != (audio_frame[i - 1] >= 0)))
        {
            zcr += 1.0f;
        }
    }

    energy = sqrtf(energy / FRAME_SIZE);
    zcr /= (FRAME_SIZE - 1);

    // 簡化的MFCC係數
    mfcc_features[0] = log10f(energy + 1e-10f);
    mfcc_features[1] = zcr;

    // 其他係數基於頻譜分析
    for (int i = 2; i < FEATURE_SIZE; i++)
    {
        float freq_component = 0.0f;
        for (int j = 0; j < FRAME_SIZE; j++)
        {
            freq_component += audio_frame[j] * cosf(2.0f * M_PI * i * j / FRAME_SIZE);
        }
        mfcc_features[i] = freq_component / FRAME_SIZE;
    }
}

KeywordClass KeywordDetector::classify_features(const float features[TOTAL_FEATURES], float *confidence)
{
    float scores[KEYWORD_COUNT];

    // 計算每個關鍵字的評分
    for (int i = 0; i < KEYWORD_COUNT; i++)
    {
        scores[i] = get_keyword_score(features, (KeywordClass)i);
    }

    // 找出最高評分的類別
    int best_class = 0;
    float best_score = scores[0];

    for (int i = 1; i < KEYWORD_COUNT; i++)
    {
        if (scores[i] > best_score)
        {
            best_score = scores[i];
            best_class = i;
        }
    }

    // 計算置信度 (使用softmax歸一化)
    float exp_sum = 0.0f;
    for (int i = 0; i < KEYWORD_COUNT; i++)
    {
        exp_sum += expf(scores[i]);
    }

    *confidence = expf(best_score) / exp_sum;

    // 如果置信度太低，歸類為UNKNOWN
    if (*confidence < CONFIDENCE_THRESHOLD && best_class != KEYWORD_SILENCE)
    {
        best_class = KEYWORD_UNKNOWN;
        *confidence = 1.0f - *confidence; // 反轉置信度
    }

    return (KeywordClass)best_class;
}

float KeywordDetector::get_keyword_score(const float *features, KeywordClass keyword)
{
    const KeywordPattern &pattern = keyword_patterns[keyword];
    float score = 0.0f;

    // 基於特徵的簡化評分
    // 這裡使用前幾個特徵進行評分

    float avg_energy = 0.0f;
    float avg_zcr = 0.0f;
    float avg_spectral = 0.0f;

    // 計算序列的平均特徵
    for (int i = 0; i < SEQUENCE_LENGTH; i++)
    {
        avg_energy += features[i * FEATURE_SIZE + 0];
        avg_zcr += features[i * FEATURE_SIZE + 1];
        avg_spectral += features[i * FEATURE_SIZE + 2];
    }

    avg_energy /= SEQUENCE_LENGTH;
    avg_zcr /= SEQUENCE_LENGTH;
    avg_spectral /= SEQUENCE_LENGTH;

    // 檢查是否符合模式
    if (avg_energy >= pattern.energy_range[0] && avg_energy <= pattern.energy_range[1])
    {
        score += 2.0f;
    }

    if (avg_zcr >= pattern.zcr_range[0] && avg_zcr <= pattern.zcr_range[1])
    {
        score += 2.0f;
    }

    // 頻譜相似度
    float spectral_diff = fabsf(avg_spectral - pattern.spectral_peak_freq);
    score += (1.0f - spectral_diff) * 2.0f;

    // 特殊處理靜音
    if (keyword == KEYWORD_SILENCE && avg_energy < SILENCE_THRESHOLD)
    {
        score += 5.0f;
    }

    return score;
}

void KeywordDetector::update_feature_buffer(const float *new_features)
{
    // 複製新特徵到緩衝區
    memcpy(feature_buffer[buffer_index], new_features, FEATURE_SIZE * sizeof(float));

    buffer_index = (buffer_index + 1) % SEQUENCE_LENGTH;

    if (buffer_index == 0)
    {
        buffer_full = true;
    }
}

void KeywordDetector::flatten_features(float *output)
{
    int start_idx = buffer_full ? buffer_index : 0;

    for (int i = 0; i < SEQUENCE_LENGTH; i++)
    {
        int actual_idx = (start_idx + i) % SEQUENCE_LENGTH;
        memcpy(&output[i * FEATURE_SIZE],
               feature_buffer[actual_idx],
               FEATURE_SIZE * sizeof(float));
    }
}

void KeywordDetector::softmax(float *input, int size)
{
    float max_val = input[0];
    for (int i = 1; i < size; i++)
    {
        if (input[i] > max_val)
            max_val = input[i];
    }

    float sum = 0.0f;
    for (int i = 0; i < size; i++)
    {
        input[i] = expf(input[i] - max_val);
        sum += input[i];
    }

    for (int i = 0; i < size; i++)
    {
        input[i] /= sum;
    }
}

void KeywordDetector::calibrate_noise_level(const AudioFeatures &features)
{
    float alpha = 0.01f; // 慢速平均
    running_noise_level = running_noise_level * (1.0f - alpha) +
                          features.rms_energy * alpha;
}

bool KeywordDetector::is_in_cooldown()
{
    return (millis() - last_detection_time) < COOLDOWN_MS;
}

void KeywordDetector::print_stats()
{
    Serial.println("\n🔑 === KEYWORD DETECTOR STATS ===");
    Serial.printf("Total detections: %d\n", total_detections);
    Serial.printf("Running noise level: %.4f\n", running_noise_level);
    Serial.printf("Buffer status: %s\n", buffer_full ? "Full" : "Filling");
    Serial.printf("Last detection: %s (%.1f%%)\n",
                  keyword_to_string(last_result.detected_keyword),
                  last_result.confidence * 100.0f);
    Serial.println("================================\n");
}

// 便利函數實現
const char *keyword_to_string(KeywordClass keyword)
{
    switch (keyword)
    {
    case KEYWORD_SILENCE:
        return "Silence";
    case KEYWORD_UNKNOWN:
        return "Unknown";
    case KEYWORD_YES:
        return "Yes/好的";
    case KEYWORD_NO:
        return "No/不要";
    case KEYWORD_HELLO:
        return "Hello/你好";
    default:
        return "Invalid";
    }
}

const char *get_keyword_emoji(KeywordClass keyword)
{
    switch (keyword)
    {
    case KEYWORD_SILENCE:
        return "🔇";
    case KEYWORD_UNKNOWN:
        return "❓";
    case KEYWORD_YES:
        return "✅";
    case KEYWORD_NO:
        return "❌";
    case KEYWORD_HELLO:
        return "👋";
    default:
        return "⚠️";
    }
}

bool is_activation_keyword(KeywordClass keyword)
{
    return (keyword == KEYWORD_YES ||
            keyword == KEYWORD_NO ||
            keyword == KEYWORD_HELLO);
}