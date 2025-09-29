#include <Arduino.h>
#include "TensorFlowLite_ESP32.h"

#include "hello_world_model_data.h"
#include "audio_capture.h"

// 音頻測試變數
bool audio_test_mode = true; // 設為 true 來測試音頻捕獲

// 函數宣告
void audio_loop();
void original_tensorflow_loop();

void setup()
{
    // 初始化 USB CDC 串口
    Serial.begin(115200);

    // 等待串口連接 (USB CDC 需要等待)
    while (!Serial && millis() < 5000)
    {
        delay(10);
    }

    delay(1000);
    Serial.println("");

    if (audio_test_mode)
    {
        Serial.println("=== INMP441 Audio Capture Test ===");
        Serial.println("ESP32-S3 with INMP441 Microphone");

        // 初始化 I2S 音頻捕獲
        if (audio_init())
        {
            Serial.println("INMP441 initialized successfully!");
            Serial.println("Connect INMP441 as follows:");
            Serial.println("VCC -> 3.3V, GND -> GND");
            Serial.println("SD -> GPIO4, WS -> GPIO5, SCK -> GPIO6");
            Serial.println("L/R -> GND (Left channel)");
        }
        else
        {
            Serial.println("Failed to initialize INMP441!");
            audio_test_mode = false;
        }
    }
    else
    {
        Serial.println("=== Hello World TensorFlow Lite Example ===");
        Serial.println("ESP32-S3 Starting...");
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

    // 讀取音頻數據
    size_t samples_read = audio_read(audio_buffer, BUFFER_SIZE);

    if (samples_read > 0)
    {
        // 處理音頻數據
        audio_process(audio_buffer, processed_audio, samples_read);

        sample_count++;

        // 每隔 100 次採樣顯示一次統計信息
        if (sample_count % 100 == 0)
        {
            // 計算音頻統計信息
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

            Serial.printf("Sample #%d: Read %d samples\n", sample_count, samples_read);
            Serial.printf("  Range: [%d, %d], Avg Amplitude: %d\n", min_val, max_val, avg_amplitude);

            // 簡單的音量檢測
            if (avg_amplitude > 1000)
            {
                Serial.println("  🎤 SOUND DETECTED!");
            }
            else if (avg_amplitude > 100)
            {
                Serial.println("  🔉 Low volume detected");
            }
            else
            {
                Serial.println("  🔇 Mostly quiet");
            }
            Serial.println("----------------------------------------");
        }
    }
    else
    {
        Serial.println("No audio data received!");
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
