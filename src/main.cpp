#include <Arduino.h>
#include "TensorFlowLite_ESP32.h"

#include "hello_world_model_data.h"
#include "audio_capture.h"
#include "voice_model.h"

// 測試模式選擇
bool audio_test_mode = true; // 設為 true 來測試 INMP441 麥克風
bool voice_ai_mode = true;   // 設為 true 來啟用語音AI模型推理

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
        if (voice_ai_mode)
        {
            Serial.println("=== VOICE AI INFERENCE MODE ===");
            Serial.println("ESP32-S3 + INMP441 + Voice Model");
        }
        else
        {
            Serial.println("=== INMP441 Audio Capture Test ===");
            Serial.println("ESP32-S3 with INMP441 Microphone");
        }

        // 初始化 I2S 音頻捕獲
        if (audio_init())
        {
            Serial.println("INMP441 initialized successfully!");
            Serial.println("Connect INMP441 as follows:");
            Serial.println("VCC -> 3.3V, GND -> GND");
            Serial.println("SD -> GPIO2, WS -> GPIO42, SCK -> GPIO41");
            Serial.println("L/R -> GND (Left channel)");

            if (voice_ai_mode)
            {
                Serial.println("\n🤖 Voice AI Model Ready!");
                Serial.println("Listening for: Speech, Music, Background, Silence");
            }
        }
        else
        {
            Serial.println("Failed to initialize INMP441!");
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

        // 計算基本音頻統計信息
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

            if (voice_ai_mode)
            {
                // ========= 語音AI模型推理 =========

                VoiceModelResult ai_result = voice_model.inference(features);

                // 每5幀顯示一次AI推理結果
                if (frame_count % 5 == 0)
                {
                    Serial.printf("%s %s (%.1f%%) | Energy:%.3f, Spectral:%.3f, Temporal:%.3f\n",
                                  get_voice_emoji(ai_result.classification),
                                  voice_output_to_string(ai_result.classification),
                                  ai_result.confidence * 100.0f,
                                  ai_result.energy_score,
                                  ai_result.spectral_score,
                                  ai_result.temporal_score);
                }

                // 每50幀顯示詳細的AI統計
                if (frame_count % 50 == 0)
                {
                    voice_model.print_model_stats();
                }
            }
            else
            {
                // ========= 原始音頻特徵顯示 =========

                // 顯示音頻特徵（每10幀顯示一次）
                if (frame_count % 10 == 0)
                {
                    Serial.println("\n📊 === AUDIO FEATURES ===");
                    Serial.printf("Frame #%d: RMS=%.4f, ZCR=%.4f, SC=%.4f\n",
                                  frame_count, features.rms_energy,
                                  features.zero_crossing_rate, features.spectral_centroid);

                    if (features.is_voice_detected)
                    {
                        Serial.println("🗣️  VOICE DETECTED!");
                    }
                    else
                    {
                        Serial.println("🔇 Background/Noise");
                    }
                    Serial.println("========================\n");
                }
            }
        }

        // 即時聲音檢測 (保持原有的實時監控)
        if (avg_amplitude > 15)
        {
            Serial.printf("🎤 LIVE: Amp=%d, Peak=%d ", avg_amplitude, peak_amplitude);

            // 繪製簡單的音量條
            int volume_bars = (avg_amplitude * 20) / 100;
            Serial.print("[");
            for (int i = 0; i < 20; i++)
            {
                if (i < volume_bars)
                    Serial.print("█");
                else
                    Serial.print("·");
            }
            Serial.println("]");
        }

        // 每隔50次採樣顯示基本統計
        if (sample_count % 50 == 0)
        {
            unsigned long now = millis();
            float samples_per_second = (50.0 * 512.0) / ((now - last_display) / 1000.0);
            last_display = now;

            Serial.printf("Sample #%d: %d samples, %.1f Hz sampling rate\n",
                          sample_count, samples_read, samples_per_second);
            Serial.printf("  Range: [%d, %d], Avg: %d, Peak: %d, Max seen: %d\n",
                          min_val, max_val, avg_amplitude, peak_amplitude, max_amplitude_seen);
            Serial.printf("  Frames processed: %d\n", frame_count);

            // 改進的聲音活動檢測
            if (avg_amplitude > 50)
            {
                Serial.println("  🔊 ACTIVE SOUND!");
            }
            else if (avg_amplitude > 20)
            {
                Serial.println("  🔉 Some activity");
            }
            else if (avg_amplitude > 10)
            {
                Serial.println("  🔈 Very quiet activity");
            }
            else
            {
                Serial.println("  🔇 Silent");
            }
            Serial.println("----------------------------------------");
        }
    }
    else
    {
        Serial.println("⚠️  No audio data received!");
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
