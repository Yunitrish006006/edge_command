#include "keyword_model.h"
#include <math.h>
#include <string.h>

// 全局關鍵字檢測器實例
KeywordDetector keyword_detector;

// 關鍵字模式定義 (基於常見中英文關鍵字的音頻特徵)
const KeywordPattern keyword_patterns[KEYWORD_COUNT] = {
    // KEYWORD_SILENCE - 精細調整靜音檢測
    {
        .energy_range = {0.0f, 0.008f}, // 更嚴格的靜音能量閾值
        .zcr_range = {0.0f, 0.3f},      // 降低零穿越率上限
        .duration_range = {0.0f, 10.0f},
        .spectral_peak_freq = 0.0f,
        .examples = {"silence", "background", "quiet"}},

    // KEYWORD_UNKNOWN - 未知語音的廣泛範圍
    {
        .energy_range = {0.008f, 0.8f}, // 調整能量範圍
        .zcr_range = {0.02f, 0.45f},    // 縮窄零穿越率範圍
        .duration_range = {0.15f, 3.5f},
        .spectral_peak_freq = 0.35f,
        .examples = {"speech", "talking", "voice"}},

    // KEYWORD_YES ("好的"/"是的"/"OK") - 優化為肯定詞彙
    {
        .energy_range = {0.015f, 0.7f}, // 中等能量範圍
        .zcr_range = {0.08f, 0.35f},    // 中等零穿越率（清晰發音）
        .duration_range = {0.3f, 2.5f}, // 典型單詞長度
        .spectral_peak_freq = 0.42f,    // "好"音的頻譜特徵
        .examples = {"好的", "是的", "OK"}},

    // KEYWORD_NO ("不要"/"不是"/"停止") - 優化為否定詞彙
    {
        .energy_range = {0.02f, 0.75f}, // 稍高能量（通常更強調）
        .zcr_range = {0.1f, 0.4f},      // 較高零穿越率（"不"音特徵）
        .duration_range = {0.25f, 2.8f},
        .spectral_peak_freq = 0.32f, // "不"音的頻譜特徵
        .examples = {"不要", "不是", "停止"}},

    // KEYWORD_HELLO ("你好"/"嗨"/"Hello") - 優化為問候語
    {
        .energy_range = {0.025f, 0.9f}, // 較高能量（友好問候）
        .zcr_range = {0.12f, 0.45f},    // 較高零穿越率（"你"音特徵）
        .duration_range = {0.4f, 3.0f}, // 雙音節詞長度
        .spectral_peak_freq = 0.52f,    // "你好"音的頻譜特徵
        .examples = {"你好", "嗨", "Hello"}},

    // KEYWORD_ON ("開") - 單音節開口音特徵
    {
        .energy_range = {0.025f, 0.7f}, // 中等能量範圍
        .zcr_range = {0.08f, 0.3f},     // 較低零穿越率（開口音特徵）
        .duration_range = {0.2f, 1.5f}, // 單音節長度
        .spectral_peak_freq = 0.38f,    // "開"音的頻譜特徵
        .examples = {"開", "on", "kai"}},

    // KEYWORD_OFF ("關") - 單音節閉口音特徵
    {
        .energy_range = {0.02f, 0.6f},  // 中等能量範圍
        .zcr_range = {0.1f, 0.35f},     // 中等零穿越率
        .duration_range = {0.2f, 1.5f}, // 單音節長度
        .spectral_peak_freq = 0.45f,    // "關"音的頻譜特徵
        .examples = {"關", "off", "guan"}}};

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

    // 調試：顯示所有評分
    Serial.printf("🔍 關鍵字評分 - 靜音:%.2f, 未知:%.2f, 是:%.2f, 否:%.2f, 你好:%.2f, 開:%.2f, 關:%.2f\n",
                  scores[0], scores[1], scores[2], scores[3], scores[4], scores[5], scores[6]);

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
    
    // 調試：顯示最佳結果
    Serial.printf("🏆 最佳匹配: %s (評分:%.2f, 信心度:%.1f%%)\n", 
                  keyword_to_string((KeywordClass)best_class), best_score, *confidence * 100.0f);

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

    // 計算序列的統計特徵
    float avg_energy = 0.0f, max_energy = 0.0f, energy_variance = 0.0f;
    float avg_zcr = 0.0f, max_zcr = 0.0f;
    float avg_spectral = 0.0f;

    // 第一遍：計算平均值和最大值
    for (int i = 0; i < SEQUENCE_LENGTH; i++)
    {
        float energy = features[i * FEATURE_SIZE + 0];
        float zcr = features[i * FEATURE_SIZE + 1];
        float spectral = features[i * FEATURE_SIZE + 2];

        avg_energy += energy;
        avg_zcr += zcr;
        avg_spectral += spectral;

        if (energy > max_energy)
            max_energy = energy;
        if (zcr > max_zcr)
            max_zcr = zcr;
    }

    avg_energy /= SEQUENCE_LENGTH;
    avg_zcr /= SEQUENCE_LENGTH;
    avg_spectral /= SEQUENCE_LENGTH;

    // 第二遍：計算能量變異度
    for (int i = 0; i < SEQUENCE_LENGTH; i++)
    {
        float energy_diff = features[i * FEATURE_SIZE + 0] - avg_energy;
        energy_variance += energy_diff * energy_diff;
    }
    energy_variance /= SEQUENCE_LENGTH;

    // === 關鍵字專用評分邏輯 ===
    if (keyword == KEYWORD_SILENCE)
    {
        // 靜音：低能量 + 低變異度
        if (avg_energy < SILENCE_THRESHOLD)
            score += 8.0f;
        if (max_energy < 0.015f)
            score += 3.0f;
        if (energy_variance < 0.001f)
            score += 2.0f;
    }
    else if (keyword == KEYWORD_YES)
    {
        // "好的"：平衡的檢測條件 - 能檢測到正常關鍵字且避免雜音

        // 放寬的基本條件
        bool energy_ok = (avg_energy >= 0.02f && avg_energy <= 0.7f); // 放寬能量範圍
        bool zcr_ok = (avg_zcr >= 0.08f && avg_zcr <= 0.35f);         // 放寬 ZCR 範圍
        bool max_energy_ok = (max_energy >= 0.04f);                   // 降低最小響亮度要求

        // 頻譜特徵匹配（適度放寬）
        float yes_spectral_match = 1.0f - fabsf(avg_spectral - 0.42f);
        bool spectral_ok = (yes_spectral_match > 0.6f); // 降低頻譜匹配要求

        // 雜音排除（保持但放寬）
        bool not_noise = (max_zcr < 0.6f);                         // 放寬雜音ZCR限制
        bool energy_reasonable = (max_energy < avg_energy * 5.0f); // 放寬能量突兀限制

        // 計分系統 - 部分滿足就可以給分
        if (energy_ok)
            score += 8.0f;
        if (zcr_ok)
            score += 6.0f;
        if (max_energy_ok)
            score += 4.0f;
        if (spectral_ok)
            score += yes_spectral_match * 8.0f;
        if (not_noise)
            score += 3.0f;
        if (energy_reasonable)
            score += 2.0f;

        // 組合獎勵 - 多個條件符合時額外加分
        int conditions_met = energy_ok + zcr_ok + max_energy_ok + spectral_ok + not_noise + energy_reasonable;
        if (conditions_met >= 5)
            score += 15.0f; // 5個以上條件符合
        if (conditions_met == 6)
            score += 10.0f; // 全部符合額外獎勵

        // 品質獎勵
        if (yes_spectral_match > 0.8f && energy_variance < 0.01f)
            score += 8.0f;
    }
    else if (keyword == KEYWORD_NO)
    {
        // "不要"：平衡的檢測條件

        // 放寬的基本條件
        bool energy_ok = (avg_energy >= 0.025f && avg_energy <= 0.8f);
        bool zcr_ok = (avg_zcr >= 0.1f && avg_zcr <= 0.4f);
        bool max_energy_ok = (max_energy >= 0.05f);
        bool emphasis_ok = (max_energy > avg_energy * 1.2f); // 降低強調要求

        // 頻譜特徵匹配（適度放寬）
        float no_spectral_match = 1.0f - fabsf(avg_spectral - 0.32f);
        bool spectral_ok = (no_spectral_match > 0.6f);

        // 雜音排除（放寬）
        bool not_noise = (max_zcr < 0.7f);
        bool clear_speech = (avg_energy > 0.02f); // 降低最小音量要求

        // 計分系統
        if (energy_ok)
            score += 8.0f;
        if (zcr_ok)
            score += 6.0f;
        if (max_energy_ok)
            score += 4.0f;
        if (emphasis_ok)
            score += 5.0f;
        if (spectral_ok)
            score += no_spectral_match * 8.0f;
        if (not_noise)
            score += 3.0f;
        if (clear_speech)
            score += 2.0f;

        // 組合獎勵
        int conditions_met = energy_ok + zcr_ok + max_energy_ok + emphasis_ok + spectral_ok + not_noise + clear_speech;
        if (conditions_met >= 5)
            score += 12.0f;
        if (conditions_met >= 6)
            score += 8.0f;

        // 品質獎勵
        if (no_spectral_match > 0.8f && emphasis_ok)
            score += 10.0f;
    }
    else if (keyword == KEYWORD_HELLO)
    {
        // "你好"：平衡的檢測條件

        // 放寬的基本條件
        bool energy_ok = (avg_energy >= 0.03f && avg_energy <= 0.9f);
        bool zcr_ok = (avg_zcr >= 0.12f && avg_zcr <= 0.45f);
        bool max_energy_ok = (max_energy >= 0.06f);
        bool variance_ok = (energy_variance > 0.002f && energy_variance < 0.02f); // 放寬雙音節變化

        // 頻譜特徵匹配（適度放寬）
        float hello_spectral_match = 1.0f - fabsf(avg_spectral - 0.52f);
        bool spectral_ok = (hello_spectral_match > 0.6f);

        // 雜音排除（放寬）
        bool not_noise = (max_zcr < 0.8f);
        bool proper_duration = (energy_variance > 0.001f);    // 降低音節變化要求
        bool clear_speech = (max_energy > avg_energy * 1.1f); // 降低峰值要求

        // 計分系統
        if (energy_ok)
            score += 8.0f;
        if (zcr_ok)
            score += 6.0f;
        if (max_energy_ok)
            score += 4.0f;
        if (variance_ok)
            score += 5.0f;
        if (spectral_ok)
            score += hello_spectral_match * 8.0f;
        if (not_noise)
            score += 3.0f;
        if (proper_duration)
            score += 2.0f;
        if (clear_speech)
            score += 2.0f;

        // 組合獎勵
        int conditions_met = energy_ok + zcr_ok + max_energy_ok + variance_ok + spectral_ok + not_noise + proper_duration + clear_speech;
        if (conditions_met >= 6)
            score += 12.0f;
        if (conditions_met >= 7)
            score += 8.0f;

        // 品質獎勵
        if (hello_spectral_match > 0.8f && variance_ok)
            score += 10.0f;
    }
    else if (keyword == KEYWORD_ON)
    {
        // "開"：單音節，短促有力的檢測條件
        
        // 基本條件 - "開"音通常短促、清晰
        bool energy_ok = (avg_energy >= 0.025f && avg_energy <= 0.7f);
        bool zcr_ok = (avg_zcr >= 0.08f && avg_zcr <= 0.3f);  // 較低ZCR，因為是開口音
        bool max_energy_ok = (max_energy >= 0.04f);
        bool short_duration = (energy_variance < 0.008f);      // 單音節，變化較小
        
        // 頻譜特徵 - "開"音有特定的頻譜特性
        float on_spectral_match = 1.0f - fabsf(avg_spectral - 0.38f);  // 較低頻譜重心
        bool spectral_ok = (on_spectral_match > 0.55f);
        
        // 雜音排除
        bool not_noise = (max_zcr < 0.6f);
        bool clear_speech = (max_energy > avg_energy * 1.2f);  // 有明顯峰值
        
        // 計分系統
        if (energy_ok) score += 8.0f;
        if (zcr_ok) score += 7.0f;      // ZCR對"開"音很重要
        if (max_energy_ok) score += 4.0f;
        if (short_duration) score += 6.0f;  // 單音節特徵
        if (spectral_ok) score += on_spectral_match * 8.0f;
        if (not_noise) score += 3.0f;
        if (clear_speech) score += 4.0f;
        
        // 組合獎勵
        int conditions_met = energy_ok + zcr_ok + max_energy_ok + short_duration + spectral_ok + not_noise + clear_speech;
        if (conditions_met >= 5) score += 10.0f;
        if (conditions_met >= 6) score += 8.0f;
        
        // 品質獎勵 - 典型"開"音特徵
        if (on_spectral_match > 0.75f && short_duration && clear_speech)
            score += 12.0f;
    }
    else if (keyword == KEYWORD_OFF)
    {
        // "關"：單音節，閉口音的檢測條件
        
        // 基本條件 - "關"音通常有特定的頻譜特性
        bool energy_ok = (avg_energy >= 0.02f && avg_energy <= 0.6f);
        bool zcr_ok = (avg_zcr >= 0.1f && avg_zcr <= 0.35f);
        bool max_energy_ok = (max_energy >= 0.03f);
        bool short_duration = (energy_variance < 0.01f);       // 單音節
        
        // 頻譜特徵 - "關"音的特性
        float off_spectral_match = 1.0f - fabsf(avg_spectral - 0.45f);  // 中等頻譜重心
        bool spectral_ok = (off_spectral_match > 0.55f);
        
        // 雜音排除
        bool not_noise = (max_zcr < 0.7f);
        bool clear_speech = (max_energy > avg_energy * 1.1f);
        
        // 計分系統
        if (energy_ok) score += 8.0f;
        if (zcr_ok) score += 6.0f;
        if (max_energy_ok) score += 4.0f;
        if (short_duration) score += 7.0f;  // 單音節很重要
        if (spectral_ok) score += off_spectral_match * 8.0f;
        if (not_noise) score += 3.0f;
        if (clear_speech) score += 3.0f;
        
        // 組合獎勵
        int conditions_met = energy_ok + zcr_ok + max_energy_ok + short_duration + spectral_ok + not_noise + clear_speech;
        if (conditions_met >= 5) score += 10.0f;
        if (conditions_met >= 6) score += 8.0f;
        
        // 品質獎勵 - 典型"關"音特徵
        if (off_spectral_match > 0.75f && short_duration)
            score += 10.0f;
    }
    else
    {
        // 未知語音：低分數，讓特定關鍵字在高匹配時能達到95%
        if (avg_energy >= 0.008f && avg_energy <= 0.8f)
            score += 1.0f; // 降低未知類別基礎分數
        if (avg_zcr >= 0.02f && avg_zcr <= 0.45f)
            score += 1.0f;
        // 讓未知成為預設選擇，只有極高匹配才能超越
    }

    // 通用特徵匹配加分
    if (avg_energy >= pattern.energy_range[0] && avg_energy <= pattern.energy_range[1])
    {
        score += 1.0f; // 降低通用匹配的權重
    }

    if (avg_zcr >= pattern.zcr_range[0] && avg_zcr <= pattern.zcr_range[1])
    {
        score += 1.0f;
    }

    // 確保分數為正值
    return fmaxf(score, 0.0f);
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
    case KEYWORD_ON:
        return "On/開";
    case KEYWORD_OFF:
        return "Off/關";
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
    case KEYWORD_ON:
        return "🟢";
    case KEYWORD_OFF:
        return "🔴";
    default:
        return "⚠️";
    }
}

bool is_activation_keyword(KeywordClass keyword)
{
    return (keyword == KEYWORD_YES ||
            keyword == KEYWORD_NO ||
            keyword == KEYWORD_ON ||
            keyword == KEYWORD_OFF ||
            keyword == KEYWORD_HELLO);
}