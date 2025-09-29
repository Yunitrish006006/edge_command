#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include "driver/i2s.h"
#include <Arduino.h>

// I2S 引腳定義
#define I2S_WS_PIN 5  // WS (Word Select) 信號
#define I2S_SCK_PIN 6 // SCK (Serial Clock) 時鐘信號
#define I2S_SD_PIN 4  // SD (Serial Data) 數據信號

// I2S 配置參數
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 16000  // 16kHz 採樣率，適合語音識別
#define BITS_PER_SAMPLE 32 // INMP441 輸出 24-bit，但使用 32-bit 容器
#define CHANNELS 1         // 單聲道
#define BUFFER_SIZE 512    // 每次讀取的樣本數
#define DMA_BUF_COUNT 8    // DMA 緩衝區數量
#define DMA_BUF_LEN 64     // 每個 DMA 緩衝區長度

// 音頻緩衝區
extern int32_t audio_buffer[BUFFER_SIZE];
extern int16_t processed_audio[BUFFER_SIZE];

// 函數聲明
bool audio_init();
size_t audio_read(int32_t *buffer, size_t buffer_size);
void audio_process(int32_t *raw_data, int16_t *processed_data, size_t length);
void audio_deinit();

#endif // AUDIO_CAPTURE_H