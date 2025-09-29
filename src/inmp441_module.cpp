#include "inmp441_module.h"
#include "esp_log.h"
#include <Arduino.h>
#include <string.h>

static const char *TAG = "INMP441Module";

/**
 * 預設建構函數
 */
INMP441Module::INMP441Module()
    : raw_buffer(nullptr)
    , processed_buffer(nullptr)
    , current_state(INMP441_UNINITIALIZED)
    , i2s_installed(false)
    , total_samples_read(0)
    , last_read_time(0)
    , consecutive_errors(0)
{
    config = create_default_config();
    Serial.println("INMP441Module 建構函數 - 使用預設配置");
}

/**
 * 自定義配置建構函數
 */
INMP441Module::INMP441Module(const INMP441Config &custom_config)
    : raw_buffer(nullptr)
    , processed_buffer(nullptr)
    , current_state(INMP441_UNINITIALIZED)
    , i2s_installed(false)
    , total_samples_read(0)
    , last_read_time(0)
    , consecutive_errors(0)
    , config(custom_config)
{
    Serial.println("INMP441Module 建構函數 - 使用自定義配置");
}

/**
 * 解構函數
 */
INMP441Module::~INMP441Module()
{
    deinitialize();
    Serial.println("INMP441Module 解構函數完成");
}

/**
 * 初始化模組（使用當前配置）
 */
bool INMP441Module::initialize()
{
    Serial.println("正在初始化 INMP441 模組...");
    
    if (current_state != INMP441_UNINITIALIZED)
    {
        Serial.println("模組已經初始化");
        return true;
    }
    
    // 分配緩衝區
    raw_buffer = new int32_t[config.buffer_size];
    processed_buffer = new int16_t[config.buffer_size];
    
    if (!raw_buffer || !processed_buffer)
    {
        Serial.println("❌ 記憶體分配失敗");
        update_state(INMP441_ERROR, "記憶體分配失敗");
        deinitialize();
        return false;
    }
    
    // 清零緩衝區
    memset(raw_buffer, 0, config.buffer_size * sizeof(int32_t));
    memset(processed_buffer, 0, config.buffer_size * sizeof(int16_t));
    
    // 安裝 I2S 驅動
    if (!install_i2s_driver())
    {
        Serial.println("❌ I2S 驅動安裝失敗");
        deinitialize();
        return false;
    }
    
    // 配置 I2S 引腳
    if (!configure_i2s_pins())
    {
        Serial.println("❌ I2S 引腳配置失敗");
        deinitialize();
        return false;
    }
    
    // 清除 I2S 緩衝區
    esp_err_t ret = i2s_zero_dma_buffer(config.i2s_port);
    if (ret != ESP_OK)
    {
        Serial.printf("⚠️  清除 I2S 緩衝區失敗: %s\n", esp_err_to_name(ret));
    }
    
    // 重置統計信息
    reset_statistics();
    
    update_state(INMP441_INITIALIZED, "INMP441 初始化成功");
    Serial.println("✅ INMP441 模組初始化完成");
    return true;
}

/**
 * 初始化模組（使用自定義配置）
 */
bool INMP441Module::initialize(const INMP441Config &custom_config)
{
    config = custom_config;
    return initialize();
}

/**
 * 去初始化模組
 */
void INMP441Module::deinitialize()
{
    if (current_state == INMP441_RUNNING)
    {
        stop();
    }
    
    uninstall_i2s_driver();
    
    // 釋放緩衝區
    delete[] raw_buffer;
    delete[] processed_buffer;
    raw_buffer = nullptr;
    processed_buffer = nullptr;
    
    update_state(INMP441_UNINITIALIZED, "模組已去初始化");
    Serial.println("INMP441 模組去初始化完成");
}

/**
 * 開始音訊擷取
 */
bool INMP441Module::start()
{
    if (current_state != INMP441_INITIALIZED)
    {
        Serial.println("❌ 模組尚未初始化，無法開始");
        return false;
    }
    
    update_state(INMP441_RUNNING, "開始音訊擷取");
    last_read_time = millis();
    Serial.println("🎤 INMP441 開始擷取音訊");
    return true;
}

/**
 * 停止音訊擷取
 */
void INMP441Module::stop()
{
    if (current_state == INMP441_RUNNING)
    {
        update_state(INMP441_INITIALIZED, "停止音訊擷取");
        Serial.println("⏹️  INMP441 停止擷取音訊");
    }
}

/**
 * 讀取音訊數據（16-bit）
 */
size_t INMP441Module::read_audio_data(int16_t *output_buffer, size_t max_samples)
{
    if (current_state != INMP441_RUNNING || !output_buffer)
    {
        return 0;
    }
    
    size_t samples_to_read = min(max_samples, (size_t)config.buffer_size);
    size_t bytes_read = 0;
    
    // 從 I2S 讀取原始數據
    esp_err_t ret = i2s_read(config.i2s_port, raw_buffer, 
                            samples_to_read * sizeof(int32_t),
                            &bytes_read, 0);  // 非阻塞讀取
    
    if (ret != ESP_OK)
    {
        consecutive_errors++;
        if (consecutive_errors > 10)
        {
            update_state(INMP441_ERROR, "連續讀取錯誤");
        }
        return 0;
    }
    
    if (bytes_read == 0)
    {
        return 0; // 沒有數據可讀
    }
    
    size_t samples_read = bytes_read / sizeof(int32_t);
    
    // 轉換音訊數據
    convert_audio_data(raw_buffer, processed_buffer, samples_read);
    
    // 複製到輸出緩衝區
    memcpy(output_buffer, processed_buffer, samples_read * sizeof(int16_t));
    
    // 更新統計信息
    total_samples_read += samples_read;
    last_read_time = millis();
    consecutive_errors = 0;
    
    return samples_read;
}

/**
 * 讀取原始音訊數據（32-bit）
 */
size_t INMP441Module::read_raw_audio_data(int32_t *output_buffer, size_t max_samples)
{
    if (current_state != INMP441_RUNNING || !output_buffer)
    {
        return 0;
    }
    
    size_t samples_to_read = min(max_samples, (size_t)config.buffer_size);
    size_t bytes_read = 0;
    
    esp_err_t ret = i2s_read(config.i2s_port, output_buffer,
                            samples_to_read * sizeof(int32_t),
                            &bytes_read, 0);
    
    if (ret != ESP_OK || bytes_read == 0)
    {
        consecutive_errors++;
        return 0;
    }
    
    size_t samples_read = bytes_read / sizeof(int32_t);
    total_samples_read += samples_read;
    last_read_time = millis();
    consecutive_errors = 0;
    
    return samples_read;
}

/**
 * 讀取一幀音訊數據並調用回調
 */
bool INMP441Module::read_audio_frame()
{
    if (current_state != INMP441_RUNNING)
    {
        return false;
    }
    
    size_t samples_read = read_audio_data(processed_buffer, config.buffer_size);
    
    if (samples_read > 0 && audio_data_callback)
    {
        audio_data_callback(processed_buffer, samples_read);
        return true;
    }
    
    return false;
}

/**
 * 設置音訊數據回調
 */
void INMP441Module::set_audio_data_callback(AudioDataCallback callback)
{
    audio_data_callback = callback;
}

/**
 * 設置狀態變更回調
 */
void INMP441Module::set_state_change_callback(StateChangeCallback callback)
{
    state_change_callback = callback;
}

/**
 * 設置配置
 */
bool INMP441Module::set_config(const INMP441Config &new_config)
{
    if (current_state == INMP441_RUNNING)
    {
        Serial.println("❌ 無法在運行中更改配置");
        return false;
    }
    
    config = new_config;
    Serial.println("✅ 配置已更新");
    return true;
}

/**
 * 重置為預設配置
 */
void INMP441Module::reset_to_default_config()
{
    config = create_default_config();
}

/**
 * 獲取統計信息
 */
INMP441Module::INMP441Stats INMP441Module::get_statistics() const
{
    INMP441Stats stats;
    stats.total_samples = total_samples_read;
    stats.uptime_ms = (current_state == INMP441_RUNNING) ? (millis() - last_read_time) : 0;
    stats.error_count = consecutive_errors;
    stats.samples_per_second = (stats.uptime_ms > 0) ? 
        (float)total_samples_read / (stats.uptime_ms / 1000.0f) : 0.0f;
    stats.last_read_time = last_read_time;
    stats.buffer_size = config.buffer_size;
    
    return stats;
}

/**
 * 重置統計信息
 */
void INMP441Module::reset_statistics()
{
    total_samples_read = 0;
    consecutive_errors = 0;
    last_read_time = millis();
}

/**
 * 自我測試
 */
bool INMP441Module::self_test()
{
    Serial.println("🧪 開始 INMP441 自我測試...");
    
    if (!is_initialized())
    {
        if (!initialize())
        {
            Serial.println("❌ 自我測試失敗: 初始化錯誤");
            return false;
        }
    }
    
    if (!start())
    {
        Serial.println("❌ 自我測試失敗: 啟動錯誤");
        return false;
    }
    
    // 測試讀取數據
    int16_t test_buffer[64];
    size_t samples_read = read_audio_data(test_buffer, 64);
    
    if (samples_read == 0)
    {
        Serial.println("❌ 自我測試失敗: 無法讀取數據");
        return false;
    }
    
    Serial.printf("✅ 自我測試成功 - 讀取了 %zu 個樣本\n", samples_read);
    return true;
}

/**
 * 打印配置信息
 */
void INMP441Module::print_config() const
{
    Serial.println("📋 INMP441 配置信息:");
    Serial.printf("  WS 引腳: GPIO%d\n", config.ws_pin);
    Serial.printf("  SCK 引腳: GPIO%d\n", config.sck_pin);
    Serial.printf("  SD 引腳: GPIO%d\n", config.sd_pin);
    Serial.printf("  I2S 端口: %d\n", config.i2s_port);
    Serial.printf("  採樣率: %lu Hz\n", config.sample_rate);
    Serial.printf("  緩衝區大小: %d 樣本\n", config.buffer_size);
    Serial.printf("  DMA 緩衝區: %d x %d\n", config.dma_buf_count, config.dma_buf_len);
    Serial.printf("  增益係數: %d\n", config.gain_factor);
}

/**
 * 打印統計信息
 */
void INMP441Module::print_statistics() const
{
    INMP441Stats stats = get_statistics();
    
    Serial.println("📊 INMP441 統計信息:");
    Serial.printf("  狀態: %s\n", get_state_string());
    Serial.printf("  總樣本數: %lu\n", stats.total_samples);
    Serial.printf("  運行時間: %lu ms\n", stats.uptime_ms);
    Serial.printf("  錯誤計數: %zu\n", stats.error_count);
    Serial.printf("  採樣率: %.1f samples/sec\n", stats.samples_per_second);
    Serial.printf("  最後讀取: %lu ms ago\n", millis() - stats.last_read_time);
}

/**
 * 安裝 I2S 驅動
 */
bool INMP441Module::install_i2s_driver()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = config.sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = config.dma_buf_count,
        .dma_buf_len = config.dma_buf_len,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    esp_err_t ret = i2s_driver_install(config.i2s_port, &i2s_config, 0, NULL);
    if (ret != ESP_OK)
    {
        Serial.printf("❌ I2S 驅動安裝失敗: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    i2s_installed = true;
    return true;
}

/**
 * 卸載 I2S 驅動
 */
void INMP441Module::uninstall_i2s_driver()
{
    if (i2s_installed)
    {
        i2s_driver_uninstall(config.i2s_port);
        i2s_installed = false;
    }
}

/**
 * 配置 I2S 引腳
 */
bool INMP441Module::configure_i2s_pins()
{
    i2s_pin_config_t pin_config = {
        .bck_io_num = config.sck_pin,
        .ws_io_num = config.ws_pin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = config.sd_pin
    };
    
    esp_err_t ret = i2s_set_pin(config.i2s_port, &pin_config);
    if (ret != ESP_OK)
    {
        Serial.printf("❌ I2S 引腳配置失敗: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

/**
 * 轉換音訊數據從 32-bit 到 16-bit
 */
void INMP441Module::convert_audio_data(const int32_t *raw_data, int16_t *processed_data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        // INMP441 輸出 24-bit 數據，位於 32-bit 容器的高 24 位
        int32_t sample = raw_data[i];
        
        // 右移 8 位獲得 24-bit 值
        sample = sample >> 8;
        
        // 轉換為 16-bit 並應用增益
        processed_data[i] = (int16_t)((sample >> 4) * config.gain_factor);
    }
}

/**
 * 更新狀態
 */
void INMP441Module::update_state(INMP441State new_state, const char *message)
{
    if (current_state != new_state)
    {
        current_state = new_state;
        
        if (state_change_callback)
        {
            state_change_callback(new_state, message);
        }
        
        if (message)
        {
            Serial.printf("🔄 INMP441 狀態變更: %s - %s\n", get_state_string(), message);
        }
    }
}

/**
 * 清除錯誤
 */
void INMP441Module::clear_errors()
{
    consecutive_errors = 0;
    if (current_state == INMP441_ERROR)
    {
        update_state(INMP441_INITIALIZED, "錯誤已清除");
    }
}

/**
 * 獲取狀態字符串
 */
const char* INMP441Module::get_state_string() const
{
    return inmp441_state_to_string(current_state);
}

// ========== 靜態方法 ==========

/**
 * 創建預設配置
 */
INMP441Config INMP441Module::create_default_config()
{
    INMP441Config config;
    config.ws_pin = INMP441_WS_PIN;
    config.sck_pin = INMP441_SCK_PIN;
    config.sd_pin = INMP441_SD_PIN;
    config.i2s_port = INMP441_I2S_PORT;
    config.sample_rate = INMP441_SAMPLE_RATE;
    config.dma_buf_count = INMP441_DMA_BUF_COUNT;
    config.dma_buf_len = INMP441_DMA_BUF_LEN;
    config.buffer_size = INMP441_BUFFER_SIZE;
    config.gain_factor = INMP441_GAIN_FACTOR;
    
    return config;
}

/**
 * 創建自定義配置
 */
INMP441Config INMP441Module::create_custom_config(uint8_t ws_pin, uint8_t sck_pin, uint8_t sd_pin, uint32_t sample_rate)
{
    INMP441Config config = create_default_config();
    config.ws_pin = ws_pin;
    config.sck_pin = sck_pin;
    config.sd_pin = sd_pin;
    config.sample_rate = sample_rate;
    
    return config;
}

// ========== 便利函數 ==========

/**
 * 狀態轉字符串
 */
const char* inmp441_state_to_string(INMP441State state)
{
    switch (state)
    {
    case INMP441_UNINITIALIZED: return "未初始化";
    case INMP441_INITIALIZED: return "已初始化";
    case INMP441_RUNNING: return "運行中";
    case INMP441_ERROR: return "錯誤";
    default: return "未知狀態";
    }
}

/**
 * 創建基本配置
 */
INMP441Config inmp441_create_basic_config(uint8_t ws_pin, uint8_t sck_pin, uint8_t sd_pin)
{
    return INMP441Module::create_custom_config(ws_pin, sck_pin, sd_pin);
}