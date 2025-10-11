#include <Arduino.h>
#include "TensorFlowLite_ESP32.h"

#include "hello_world_model_data.h"
#include "audio_module.h"
#include "voice_model.h"
#include "keyword_model.h"
#include "debug_print.h"

// 測試模式選擇
bool audio_test_mode = true; // 設為 true 來測試 INMP441 麥克風
bool voice_ai_mode = false;  // 關閉語音AI，只保留關鍵字檢測
bool keyword_mode = true;    // 設為 true 來啟用關鍵字檢測

// 全域 Debug 模組
DebugPrint debug_main("Main", true); // 主程式 debug，預設啟用

// 音訊擷取模組實例
AudioCaptureModule audio_module;

// 外部宣告全域變數（在各自的 .cpp 檔案中定義）
extern VoiceModel voice_model;
extern KeywordDetector keyword_detector;

// 函數宣告
void audio_loop();
void original_tensorflow_loop();

// 音訊模組回調函數
void on_audio_frame(const AudioFeatures &features);
void on_vad_event(const VADResult &result);
void on_speech_complete(const float *speech_data, size_t length, unsigned long duration_ms);

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
    debug_main.info("\n\n==================================");
    debug_main.info("ESP32-S3 BOOT SUCCESSFUL!");
    debug_main.info("Serial Communication Test");
    debug_main.info("==================================");

    if (audio_test_mode)
    {
        debug_main.info("=== 關鍵字檢測模式 ===");
        debug_main.info("ESP32-S3 + INMP441 + 關鍵字辨識 (模組化版本)");

        // 可選: 自定義 INMP441 配置
        // INMP441Config custom_config = INMP441Module::create_custom_config(42, 41, 2, 16000);
        
        // 初始化音訊模組
        if (audio_module.initialize())  // 或使用 audio_module.initialize(custom_config)
        {
            debug_main.success("音訊模組初始化成功!");

            // 顯示 INMP441 配置信息
            audio_module.get_inmp441_module().print_config();
            
            // 設置回調函數
            audio_module.set_audio_frame_callback(on_audio_frame);
            audio_module.set_vad_callback(on_vad_event);
            audio_module.set_speech_complete_callback(on_speech_complete);
            
            // 開始音訊擷取
            if (audio_module.start_capture())
            {
                debug_main.info("🎤 正在聆聽中... 請說出關鍵字:");
                debug_main.info("👋 \"你好\" | \"Hello\"");
                debug_main.info("✅ \"好的\" | \"Yes\"");
                debug_main.info("❌ \"不要\" | \"No\"");
                debug_main.info("🟢 \"開\" | \"On\"");
                debug_main.info("🔴 \"關\" | \"Off\"");
                debug_main.info("----------------------------------------");
            }
            else
            {
                debug_main.error("啟動音訊擷取失敗!");
                audio_test_mode = false;
            }
        }
        else
        {
            debug_main.error("初始化音訊模組失敗!");
            audio_test_mode = false;
        }
    }
    else
    {
        debug_main.info("=== Basic Serial Communication Test ===");
        debug_main.info("ESP32-S3 Serial Port Working!");
        debug_main.info("Testing basic output before audio features...");
    }

    debug_main.success("Serial communication established!");
    debug_main.info("Starting main loop in 2 seconds...");
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
    // 使用新的音訊模組進行處理
    audio_module.process_audio_loop();
    
    // 獲取音訊統計信息並偶爾顯示
    static unsigned long last_stats_display = 0;
    unsigned long current_time = millis();
    
    if (current_time - last_stats_display > 5000) // 每5秒顯示一次統計
    {
        AudioCaptureModule::AudioStats stats = audio_module.get_audio_stats();
        if (stats.avg_amplitude > 50)
        {
            debug_main.printf("📊 音訊統計 - 平均振幅: %d, 最大: %d, 最小: %d\n",
                              stats.avg_amplitude, stats.max_amplitude, stats.min_amplitude);
        }
        last_stats_display = current_time;
    }
}

void original_tensorflow_loop()
{
    static float x = 0.0f;
    static int counter = 0;

    counter++;

    // 計算預期的 sin(x) 值作為對比
    float expected = sin(x);

    debug_main.printf("[%d] Input x = %.3f, Expected sin(x) = %.6f\n", counter, x, expected);

    // 每隔 10 次輸出一個分隔線
    if (counter % 10 == 0)
    {
        debug_main.print("----------------------------------------");
    }

    // 更新輸入
    x += 0.1f;
    if (x > 2 * 3.14159f)
    {
        x = 0.0f;
        debug_main.info(">>> Cycle complete - Restarting from x=0 <<<");
        debug_main.print("");
    }

    delay(1000);
}

// ========== 音訊模組回調函數實現 ==========

/**
 * 音訊幀處理回調
 * 每當有新的音訊幀處理完成時調用
 */
void on_audio_frame(const AudioFeatures &features)
{
    // 可以在這裡添加即時特徵監控
    static int frame_count = 0;
    frame_count++;

    // 每100幀輸出一次特徵信息（避免過多輸出）
    if (frame_count % 100 == 0 && features.rms_energy > 0.01f)
    {
        debug_main.printf("🎵 幀特徵 - RMS:%.3f ZCR:%.3f SC:%.3f Voice:%s\n",
                          features.rms_energy,
                          features.zero_crossing_rate,
                          features.spectral_centroid,
                          features.is_voice_detected ? "是" : "否");
    }
}

/**
 * VAD 事件回調
 * 當語音活動檢測狀態改變時調用
 */
void on_vad_event(const VADResult &result)
{
    static VADState last_state = VAD_SILENCE;
    
    // 只在狀態改變時輸出
    if (result.state != last_state)
    {
        switch (result.state)
        {
        case VAD_SPEECH_START:
            debug_main.info("🎤 語音檢測開始...");
            break;
            
        case VAD_SPEECH_ACTIVE:
            debug_main.info("🗣️  正在收集語音數據...");
            break;
            
        case VAD_SPEECH_END:
            debug_main.printf("⏹️  語音檢測結束 - 持續時間: %lu ms\n", result.duration_ms);
            break;
            
        case VAD_SILENCE:
            if (last_state != VAD_SILENCE)
                debug_main.info("🔇 回到靜音狀態");
            break;
        }
        last_state = result.state;
    }
}

/**
 * 語音完成回調
 * 當完整語音段落收集完成時調用，進行關鍵字檢測
 */
void on_speech_complete(const float *speech_data, size_t length, unsigned long duration_ms)
{
    if (!keyword_mode || length == 0)
    {
        return;
    }

    debug_main.info("🎯 開始分析完整語音段落...");

    // 計算語音持續時間
    float duration_seconds = (float)length / AUDIO_SAMPLE_RATE;
    
    // 將整個語音段落分段處理以提取更好的特徵
    AudioFeatures overall_features = {0};
    int num_segments = (length / AUDIO_FRAME_SIZE) + 1;
    float total_rms = 0, total_zcr = 0, total_sc = 0;
    int valid_segments = 0;
    
    // 分段分析並累積特徵
    for (int seg = 0; seg < num_segments && seg * AUDIO_FRAME_SIZE < length; seg++)
    {
        int start_pos = seg * AUDIO_FRAME_SIZE;
        int end_pos = min(start_pos + AUDIO_FRAME_SIZE, (int)length);
        int segment_size = end_pos - start_pos;
        
        if (segment_size >= AUDIO_FRAME_SIZE / 4) // 只處理有足夠長度的段落
        {
            // 創建臨時 AudioCaptureModule 實例來提取特徵
            // 這裡直接計算特徵，因為我們已經有了語音數據
            float rms = 0.0f;
            for (int i = start_pos; i < end_pos; i++)
            {
                rms += speech_data[i] * speech_data[i];
            }
            rms = sqrt(rms / segment_size);
            
            // 計算零穿越率
            int zero_crossings = 0;
            for (int i = start_pos + 1; i < end_pos; i++)
            {
                if ((speech_data[i] >= 0) != (speech_data[i-1] >= 0))
                {
                    zero_crossings++;
                }
            }
            float zcr = (float)zero_crossings / (segment_size - 1);
            
            // 計算頻譜重心（簡化版）
            float high_freq_energy = 0.0f;
            float total_energy = 0.0f;
            for (int i = start_pos; i < end_pos; i++)
            {
                float energy = speech_data[i] * speech_data[i];
                total_energy += energy;
                if (i > start_pos + segment_size / 2)
                {
                    high_freq_energy += energy;
                }
            }
            float spectral_centroid = (total_energy > 0) ? (high_freq_energy / total_energy) : 0.0f;
            
            total_rms += rms;
            total_zcr += zcr;
            total_sc += spectral_centroid;
            valid_segments++;
        }
    }
    
    // 計算平均特徵
    if (valid_segments > 0)
    {
        overall_features.rms_energy = total_rms / valid_segments;
        overall_features.zero_crossing_rate = total_zcr / valid_segments;
        overall_features.spectral_centroid = total_sc / valid_segments;
        overall_features.is_voice_detected = true;
        
        // 進行關鍵字檢測
        KeywordResult keyword_result = keyword_detector.detect(overall_features);
        
        // 顯示完整的分析結果
        debug_main.printf("📏 語音段落 - 長度: %zu 樣本 (%.2f 秒)\n", length, duration_seconds);

        debug_main.printf("🔊 整體特徵 - RMS: %.3f, ZCR: %.3f, SC: %.3f\n",
                          overall_features.rms_energy,
                          overall_features.zero_crossing_rate,
                          overall_features.spectral_centroid);

        // 顯示關鍵字檢測結果
        if (keyword_result.detected_keyword != KEYWORD_SILENCE &&
            keyword_result.detected_keyword != KEYWORD_UNKNOWN)
        {
            debug_main.printf("🎯 關鍵字檢測: %s (信心度: %.1f%%)\n",
                              keyword_to_string(keyword_result.detected_keyword),
                              keyword_result.confidence * 100.0f);

            // 顯示所有類別的機率
            debug_main.printf("📊 機率分佈 - 靜音:%.1f%%, 未知:%.1f%%, 是:%.1f%%, 否:%.1f%%, 你好:%.1f%%, 開:%.1f%%, 關:%.1f%%\n",
                              keyword_result.probabilities[0] * 100.0f,
                              keyword_result.probabilities[1] * 100.0f,
                              keyword_result.probabilities[2] * 100.0f,
                              keyword_result.probabilities[3] * 100.0f,
                              keyword_result.probabilities[4] * 100.0f,
                              keyword_result.probabilities[5] * 100.0f,
                              keyword_result.probabilities[6] * 100.0f);

            // 顯示檢測到的特定關鍵字
            switch (keyword_result.detected_keyword)
            {
            case KEYWORD_YES:
                debug_main.success("✅ 檢測到: 是的/好的/Yes");
                break;
            case KEYWORD_NO:
                debug_main.info("❌ 檢測到: 不要/不是/No");
                break;
            case KEYWORD_HELLO:
                debug_main.info("👋 檢測到: 你好/Hello");
                break;
            case KEYWORD_ON:
                debug_main.success("🟢 檢測到: 開/On - 系統啟動");
                break;
            case KEYWORD_OFF:
                debug_main.warning("🔴 檢測到: 關/Off - 系統關閉");
                break;
            }
        }
        else
        {
            debug_main.info("❓ 未檢測到明確關鍵字");
        }

        debug_main.info("========================================");
    }
}
