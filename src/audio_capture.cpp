#include "audio_capture.h"
#include "esp_log.h"

static const char *TAG = "AudioCapture";

// 全局變量
int32_t audio_buffer[BUFFER_SIZE];
int16_t processed_audio[BUFFER_SIZE];

/**
 * 初始化 I2S 介面用於 INMP441 麥克風
 */
bool audio_init()
{
    Serial.println("Initializing I2S for INMP441...");

    // I2S 配置結構
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // 主模式，接收
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,      // 32-bit 容器
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,       // 只使用左聲道
        .communication_format = I2S_COMM_FORMAT_STAND_I2S, // 標準 I2S 格式
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,          // 中斷優先級
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false, // 不使用 APLL
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    // I2S 引腳配置
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,         // 位元時鐘引腳
        .ws_io_num = I2S_WS_PIN,           // 字選引腳
        .data_out_num = I2S_PIN_NO_CHANGE, // 不使用輸出
        .data_in_num = I2S_SD_PIN          // 數據輸入引腳
    };

    // 安裝 I2S 驅動
    esp_err_t ret = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (ret != ESP_OK)
    {
        Serial.printf("Failed to install I2S driver: %s\n", esp_err_to_name(ret));
        return false;
    }

    // 設置 I2S 引腳
    ret = i2s_set_pin(I2S_PORT, &pin_config);
    if (ret != ESP_OK)
    {
        Serial.printf("Failed to set I2S pins: %s\n", esp_err_to_name(ret));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    // 清除 I2S 緩衝區
    ret = i2s_zero_dma_buffer(I2S_PORT);
    if (ret != ESP_OK)
    {
        Serial.printf("Failed to clear I2S buffer: %s\n", esp_err_to_name(ret));
    }

    Serial.println("I2S initialized successfully!");
    return true;
}

/**
 * 從 INMP441 讀取音頻數據
 */
size_t audio_read(int32_t *buffer, size_t buffer_size)
{
    size_t bytes_read = 0;

    esp_err_t ret = i2s_read(I2S_PORT, buffer, buffer_size * sizeof(int32_t),
                             &bytes_read, portMAX_DELAY);

    if (ret != ESP_OK)
    {
        Serial.printf("I2S read error: %s\n", esp_err_to_name(ret));
        return 0;
    }

    return bytes_read / sizeof(int32_t); // 返回樣本數量
}

/**
 * 處理原始音頻數據
 * 將 32-bit 數據轉換為 16-bit，並進行基本的信號處理
 */
void audio_process(int32_t *raw_data, int16_t *processed_data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        // INMP441 輸出 24-bit 數據，位於 32-bit 容器的高 24 位
        // 右移 8 位來獲得 24-bit 值，然後再右移 8 位轉為 16-bit
        int32_t sample = raw_data[i] >> 8; // 獲得 24-bit 有符號值

        // 轉換為 16-bit（右移 8 位）
        processed_data[i] = (int16_t)(sample >> 8);

        // 可選：簡單的增益調整（如果音量太小）
        // processed_data[i] = (int16_t)((sample >> 8) * 2); // 2倍增益
    }
}

/**
 * 去初始化 I2S
 */
void audio_deinit()
{
    i2s_driver_uninstall(I2S_PORT);
    Serial.println("I2S deinitialized");
}