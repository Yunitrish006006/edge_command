#include "voice_model.h"
#include <math.h>
#include <string.h>

// 簡單的絕對值函數，避免命名衝突
float abs_f(float x)
{
    return (x >= 0) ? x : -x;
}

// 全局模型實例
VoiceModel voice_model;

VoiceModel::VoiceModel()
{
    reset();
}

void VoiceModel::reset()
{
    // 清空歷史記錄
    memset(history, 0, sizeof(history));
    memset(confidence_history, 0, sizeof(confidence_history));
    history_index = 0;
    frame_count = 0;

    // 重置統計資訊
    running_energy_avg = 0.0f;
    running_zcr_avg = 0.0f;
    total_frames = 0;
}

VoiceModelResult VoiceModel::inference(const AudioFeatures &features)
{
    VoiceModelResult result;

    // 計算各項評分
    result.energy_score = calculate_energy_score(features.rms_energy);
    result.spectral_score = calculate_spectral_score(features.zero_crossing_rate, features.spectral_centroid);
    result.temporal_score = calculate_temporal_score(features.rms_energy, features.zero_crossing_rate);

    // 進行分類
    result.classification = classify_features(result.energy_score, result.spectral_score, result.temporal_score);

    // 計算置信度
    result.confidence = calculate_confidence(features, result.classification);

    // 計算語音機率和活動水準
    result.voice_probability = (result.classification == VOICE_SPEECH) ? result.confidence : (1.0f - result.confidence);
    result.activity_level = result.energy_score;

    // 更新歷史和統計
    add_to_history(result.classification, result.confidence);
    update_running_stats(features);

    frame_count++;

    return result;
}

float VoiceModel::calculate_energy_score(float energy)
{
    // 將能量映射到 0-1 分數
    if (energy <= SILENCE_ENERGY_THRESHOLD)
    {
        return 0.0f;
    }
    else if (energy >= 1.0f)
    {
        return 1.0f;
    }
    else
    {
        // 對數縮放以更好地處理動態範圍
        return log10(energy * 10.0f + 1.0f) / log10(11.0f);
    }
}

float VoiceModel::calculate_spectral_score(float zcr, float spectral_centroid)
{
    // 基於零穿越率和頻譜重心計算頻譜評分
    float zcr_score = 0.0f;
    float centroid_score = 0.0f;

    // 語音的 ZCR 通常在中等範圍
    if (zcr >= SPEECH_ZCR_MIN && zcr <= SPEECH_ZCR_MAX)
    {
        zcr_score = 1.0f - abs_f(zcr - (SPEECH_ZCR_MIN + SPEECH_ZCR_MAX) / 2.0f) / ((SPEECH_ZCR_MAX - SPEECH_ZCR_MIN) / 2.0f);
    }
    else if (zcr > NOISE_ZCR_MIN)
    {
        zcr_score = 0.2f; // 可能是噪音
    }

    // 語音的頻譜重心通常平衡
    if (spectral_centroid >= 0.2f && spectral_centroid <= 0.8f)
    {
        centroid_score = 1.0f - abs_f(spectral_centroid - 0.5f) * 2.0f;
    }

    return (zcr_score + centroid_score) / 2.0f;
}

float VoiceModel::calculate_temporal_score(float energy, float zcr)
{
    // 基於時域特性計算評分
    float consistency_score = 1.0f;

    // 檢查與歷史平均的一致性
    if (total_frames > 5)
    {
        float energy_deviation = abs_f(energy - running_energy_avg) / (running_energy_avg + 0.001f);
        float zcr_deviation = abs_f(zcr - running_zcr_avg) / (running_zcr_avg + 0.001f);

        // 語音通常有適度的變化
        if (energy_deviation < 2.0f && zcr_deviation < 1.0f)
        {
            consistency_score = 1.0f;
        }
        else
        {
            consistency_score = max(0.3f, 1.0f - (energy_deviation + zcr_deviation) / 4.0f);
        }
    }

    return consistency_score;
}

VoiceModelOutput VoiceModel::classify_features(float energy_score, float spectral_score, float temporal_score)
{
    // 綜合評分
    float overall_score = (energy_score + spectral_score + temporal_score) / 3.0f;

    // 靜音檢測（優先級最高）
    if (energy_score < 0.1f)
    {
        return VOICE_SILENCE;
    }

    // 語音檢測
    if (overall_score > 0.6f && spectral_score > 0.5f && energy_score > 0.2f)
    {
        return VOICE_SPEECH;
    }

    // 音樂檢測（低ZCR，高能量）
    if (energy_score > 0.3f && spectral_score < 0.3f)
    {
        return VOICE_MUSIC;
    }

    // 背景噪音（中等能量，各種頻譜特性）
    if (energy_score > 0.1f && energy_score < 0.5f)
    {
        return VOICE_BACKGROUND;
    }

    return VOICE_UNKNOWN;
}

float VoiceModel::calculate_confidence(const AudioFeatures &features, VoiceModelOutput classification)
{
    float confidence = 0.5f; // 基準置信度

    switch (classification)
    {
    case VOICE_SILENCE:
        confidence = 1.0f - features.rms_energy * 10.0f; // 能量越低，靜音置信度越高
        break;

    case VOICE_SPEECH:
        // 語音置信度基於特徵的典型性
        if (features.rms_energy >= SPEECH_ENERGY_MIN && features.rms_energy <= SPEECH_ENERGY_MAX &&
            features.zero_crossing_rate >= SPEECH_ZCR_MIN && features.zero_crossing_rate <= SPEECH_ZCR_MAX)
        {
            confidence = 0.8f + min(0.2f, features.rms_energy * 2.0f);
        }
        else
        {
            confidence = 0.6f;
        }
        break;

    case VOICE_MUSIC:
        confidence = 0.7f + min(0.3f, features.rms_energy);
        break;

    case VOICE_BACKGROUND:
        confidence = 0.6f + min(0.2f, features.rms_energy);
        break;

    default:
        confidence = 0.4f;
        break;
    }

    return max(0.0f, min(1.0f, confidence));
}

void VoiceModel::update_running_stats(const AudioFeatures &features)
{
    float alpha = 0.1f; // 平滑係數

    if (total_frames == 0)
    {
        running_energy_avg = features.rms_energy;
        running_zcr_avg = features.zero_crossing_rate;
    }
    else
    {
        running_energy_avg = running_energy_avg * (1.0f - alpha) + features.rms_energy * alpha;
        running_zcr_avg = running_zcr_avg * (1.0f - alpha) + features.zero_crossing_rate * alpha;
    }

    total_frames++;
}

void VoiceModel::add_to_history(VoiceModelOutput classification, float confidence)
{
    history[history_index] = classification;
    confidence_history[history_index] = confidence;
    history_index = (history_index + 1) % HISTORY_SIZE;
}

VoiceModelOutput VoiceModel::get_smoothed_classification()
{
    // 統計最近的分類結果
    int counts[5] = {0}; // 對應 5 個類別

    int frames_to_check = min(frame_count, HISTORY_SIZE);
    for (int i = 0; i < frames_to_check; i++)
    {
        int idx = (history_index - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
        counts[history[idx]]++;
    }

    // 找出最常見的分類
    VoiceModelOutput most_common = VOICE_UNKNOWN;
    int max_count = 0;

    for (int i = 0; i < 5; i++)
    {
        if (counts[i] > max_count)
        {
            max_count = counts[i];
            most_common = (VoiceModelOutput)i;
        }
    }

    return most_common;
}

float VoiceModel::get_average_confidence()
{
    if (frame_count == 0)
        return 0.0f;

    float sum = 0.0f;
    int frames_to_check = min(frame_count, HISTORY_SIZE);

    for (int i = 0; i < frames_to_check; i++)
    {
        int idx = (history_index - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
        sum += confidence_history[idx];
    }

    return sum / frames_to_check;
}

void VoiceModel::print_model_stats()
{
    Serial.println("\n📈 === VOICE MODEL STATISTICS ===");
    Serial.printf("Total frames processed: %d\n", total_frames);
    Serial.printf("Running energy average: %.4f\n", running_energy_avg);
    Serial.printf("Running ZCR average: %.4f\n", running_zcr_avg);

    VoiceModelOutput smoothed = get_smoothed_classification();
    float avg_confidence = get_average_confidence();

    Serial.printf("Smoothed classification: %s (%.2f confidence)\n",
                  voice_output_to_string(smoothed), avg_confidence);
    Serial.println("===============================\n");
}

// 便利函數實現
const char *voice_output_to_string(VoiceModelOutput output)
{
    switch (output)
    {
    case VOICE_SILENCE:
        return "Silence";
    case VOICE_BACKGROUND:
        return "Background";
    case VOICE_SPEECH:
        return "Speech";
    case VOICE_MUSIC:
        return "Music";
    case VOICE_UNKNOWN:
        return "Unknown";
    default:
        return "Invalid";
    }
}

const char *get_voice_emoji(VoiceModelOutput output)
{
    switch (output)
    {
    case VOICE_SILENCE:
        return "🔇";
    case VOICE_BACKGROUND:
        return "🌫️";
    case VOICE_SPEECH:
        return "🗣️";
    case VOICE_MUSIC:
        return "🎵";
    case VOICE_UNKNOWN:
        return "❓";
    default:
        return "⚠️";
    }
}