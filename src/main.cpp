#include <Arduino.h>
#include "TensorFlowLite_ESP32.h"

#include "hello_world_model_data.h"
#include "audio_capture.h"
#include "voice_model.h"
#include "keyword_model.h"

// 測試模式選擇
bool audio_test_mode = true; // 設為 true 來測試 INMP441 麥克風
bool voice_ai_mode = false;  // 關閉語音AI，只保留關鍵字檢測
bool keyword_mode = true;    // 設為 true 來啟用關鍵字檢測

// 外部宣告全域變數（在各自的 .cpp 檔案中定義）
extern VoiceModel voice_model;
extern KeywordDetector keyword_detector;

// 函數宣告
void audio_loop();
void original_tensorflow_loop();

void setup()
{
    // 初始化 USB CDC 串口
    Serial.begin(115200);

    // 等待串口連接 (USB CDC 需要等待)
    unsigned long start_time = millis();
    while (!Serial && (millis() - start_time) < 5000)
    {
        delay(10);
    }

    // 額外延遲確保串口穩定
    delay(2000);

    // 發送多個測試訊息
    Serial.println("\n\n==================================");
    Serial.println("ESP32-S3 BOOT SUCCESSFUL!");
    Serial.println("Serial Communication Test");
    Serial.println("==================================");

    if (audio_test_mode)
    {
        Serial.println("=== 關鍵字檢測模式 ===");
        Serial.println("ESP32-S3 + INMP441 + 關鍵字辨識");

        // 初始化 I2S 音頻捕獲
        if (audio_init())
        {
            Serial.println("INMP441 初始化成功!");
            Serial.println("🎤 正在聆聽中... 請說出關鍵字:");
            Serial.println("👋 \"你好\" | \"Hello\"");
            Serial.println("✅ \"好的\" | \"Yes\"");
            Serial.println("❌ \"不要\" | \"No\"");
            Serial.println("----------------------------------------");
        }
        else
        {
            Serial.println("初始化 INMP441 失敗!");
            audio_test_mode = false;
        }
    }
    else
    {
        Serial.println("=== Basic Serial Communication Test ===");
        Serial.println("ESP32-S3 Serial Port Working!");
        Serial.println("Testing basic output before audio features...");
    }

    Serial.println("Serial communication established!");
    Serial.println("Starting main loop in 2 seconds...");
    delay(2000);
}

void loop()
{
    if (audio_test_mode)
    {
        // 音頻捕獲模式
        audio_loop();
    }
    else
    {
        // 原始 sin(x) 模式
        original_tensorflow_loop();
    }
}

void audio_loop()
{
    static int sample_count = 0;
    static unsigned long last_display = 0;
    static int32_t max_amplitude_seen = 0;
    static int frame_count = 0;

    // 讀取音頻數據
    size_t samples_read = audio_read(audio_buffer, BUFFER_SIZE);

    if (samples_read > 0)
    {
        // 處理音頻數據
        audio_process(audio_buffer, processed_audio, samples_read);

        sample_count++;

        // 計算基本音頻統計信息（先計算，再使用）
        int32_t min_val = processed_audio[0];
        int32_t max_val = processed_audio[0];
        int64_t sum = 0;

        for (size_t i = 0; i < samples_read; i++)
        {
            int16_t sample = processed_audio[i];
            if (sample < min_val)
                min_val = sample;
            if (sample > max_val)
                max_val = sample;
            sum += abs(sample);
        }

        int32_t avg_amplitude = sum / samples_read;

        // 調試：顯示原始音頻數據樣本（只在有顯著音頻活動時）
        if (sample_count % 200 == 0 && avg_amplitude > 50)
        {
            Serial.printf("🔍 音頻數據樣本 - 振幅: %d | Raw[0]: %d | Processed[0]: %d\n",
                          avg_amplitude, audio_buffer[0], processed_audio[0]);
        }
        int32_t peak_amplitude = max(abs(min_val), abs(max_val));

        // 記錄最大振幅
        if (peak_amplitude > max_amplitude_seen)
            max_amplitude_seen = peak_amplitude;

        // ========= 音頻預處理與AI推理 =========

        // 檢查是否有完整的音頻幀準備好
        if (audio_frame_ready(processed_audio, samples_read))
        {
            frame_count++;

            // 獲取處理後的音頻幀
            float current_frame[FRAME_SIZE];
            audio_get_current_frame(current_frame);

            // 提取音頻特徵
            AudioFeatures features;
            audio_extract_features(current_frame, &features);

            // ========= 關鍵字檢測 =========
            if (keyword_mode)
            {
                KeywordResult keyword_result = keyword_detector.detect(features);

                // 只在檢測到關鍵字時顯示詳細信息
                if (keyword_result.detected_keyword != KEYWORD_SILENCE && keyword_result.detected_keyword != KEYWORD_UNKNOWN)
                {
                    Serial.printf("🔊 音頻特徵 - RMS: %.3f, ZCR: %.3f, SC: %.3f\n",
                                  features.rms_energy, features.zero_crossing_rate, features.spectral_centroid);

                    Serial.printf("🎯 關鍵字檢測: %s (信心度: %.1f%%)\n",
                                  keyword_to_string(keyword_result.detected_keyword),
                                  keyword_result.confidence * 100.0f);

                    // 顯示所有類別的機率
                    Serial.printf("📊 機率分佈 - 靜音:%.1f%%, 未知:%.1f%%, 是:%.1f%%, 否:%.1f%%, 你好:%.1f%%\n",
                                  keyword_result.probabilities[0] * 100.0f, keyword_result.probabilities[1] * 100.0f,
                                  keyword_result.probabilities[2] * 100.0f, keyword_result.probabilities[3] * 100.0f,
                                  keyword_result.probabilities[4] * 100.0f);

                    // 顯示檢測到的特定關鍵字
                    switch (keyword_result.detected_keyword)
                    {
                    case KEYWORD_YES:
                        Serial.println("✅ 檢測到: 是的/好的/Yes");
                        break;
                    case KEYWORD_NO:
                        Serial.println("❌ 檢測到: 不要/不是/No");
                        break;
                    case KEYWORD_HELLO:
                        Serial.println("👋 檢測到: 你好/Hello");
                        break;
                    }
                    Serial.println("========================================");
                }
            }
        }

        // 簡化的音頻活動指示器（減少輸出頻率）
        static unsigned long last_activity_time = 0;
        unsigned long current_time = millis();

        if (avg_amplitude > 50 && (current_time - last_activity_time) > 1000)
        {
            Serial.printf("🎤 檢測到聲音活動 (振幅: %d) - 正在分析...\n", avg_amplitude);
            last_activity_time = current_time;
        }
    }
    else
    {
        // 靜默等待音頻數據
        delay(10);
    }
}

void original_tensorflow_loop()
{
    static float x = 0.0f;
    static int counter = 0;

    counter++;

    // 計算預期的 sin(x) 值作為對比
    float expected = sin(x);

    Serial.printf("[%d] Input x = %.3f, Expected sin(x) = %.6f\n", counter, x, expected);

    // 每隔 10 次輸出一個分隔線
    if (counter % 10 == 0)
    {
        Serial.println("----------------------------------------");
    }

    // 更新輸入
    x += 0.1f;
    if (x > 2 * 3.14159f)
    {
        x = 0.0f;
        Serial.println(">>> Cycle complete - Restarting from x=0 <<<");
        Serial.println("");
    }

    delay(1000);
}
