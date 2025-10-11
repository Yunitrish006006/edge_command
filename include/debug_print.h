#ifndef DEBUG_PRINT_H
#define DEBUG_PRINT_H

#include <Arduino.h>
#include <stdarg.h>

/**
 * 通用 Debug 輸出模組
 * 提供統一的 debug 輸出介面，支援全域和物件級別的 debug 控制
 */
class DebugPrint
{
private:
    bool debug_enabled;
    const char* module_name;

public:
    /**
     * 建構函數
     * @param name 模組名稱（用於識別輸出來源）
     * @param enabled 預設是否啟用 debug
     */
    DebugPrint(const char* name = nullptr, bool enabled = false)
        : debug_enabled(enabled), module_name(name) {}

    /**
     * 設置 debug 開關
     */
    void set_debug(bool enable) { debug_enabled = enable; }

    /**
     * 獲取 debug 狀態
     */
    bool is_debug_enabled() const { return debug_enabled; }

    /**
     * 設置模組名稱
     */
    void set_module_name(const char* name) { module_name = name; }

    /**
     * 獲取模組名稱
     */
    const char* get_module_name() const { return module_name; }

    /**
     * 輸出簡單字串訊息
     * @param message 要輸出的訊息
     */
    void print(const char* message) const
    {
        if (debug_enabled && message)
        {
            if (module_name)
            {
                Serial.printf("[%s] ", module_name);
            }
            Serial.println(message);
        }
    }

    /**
     * 輸出格式化訊息
     * @param format 格式化字串
     * @param ... 可變參數
     */
    void printf(const char* format, ...) const
    {
        if (debug_enabled && format)
        {
            char buffer[256];
            va_list args;
            va_start(args, format);
            
            // 如果有模組名稱，先輸出模組名稱
            if (module_name)
            {
                Serial.printf("[%s] ", module_name);
            }
            
            // 格式化輸出
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            Serial.print(buffer);
        }
    }

    /**
     * 輸出警告訊息（即使 debug 關閉也會顯示）
     * @param message 警告訊息
     */
    void warning(const char* message) const
    {
        if (message)
        {
            if (module_name)
            {
                Serial.printf("[%s] ⚠️  WARNING: ", module_name);
            }
            else
            {
                Serial.print("⚠️  WARNING: ");
            }
            Serial.println(message);
        }
    }

    /**
     * 輸出錯誤訊息（即使 debug 關閉也會顯示）
     * @param message 錯誤訊息
     */
    void error(const char* message) const
    {
        if (message)
        {
            if (module_name)
            {
                Serial.printf("[%s] ❌ ERROR: ", module_name);
            }
            else
            {
                Serial.print("❌ ERROR: ");
            }
            Serial.println(message);
        }
    }

    /**
     * 輸出錯誤訊息（格式化版本）
     * @param format 格式化字串
     * @param ... 可變參數
     */
    void error_f(const char* format, ...) const
    {
        if (format)
        {
            char buffer[256];
            va_list args;
            va_start(args, format);
            
            if (module_name)
            {
                Serial.printf("[%s] ❌ ERROR: ", module_name);
            }
            else
            {
                Serial.print("❌ ERROR: ");
            }
            
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            Serial.println(buffer);
        }
    }

    /**
     * 輸出資訊訊息（即使 debug 關閉也會顯示）
     * @param message 資訊訊息
     */
    void info(const char* message) const
    {
        if (message)
        {
            if (module_name)
            {
                Serial.printf("[%s] ℹ️  ", module_name);
            }
            else
            {
                Serial.print("ℹ️  ");
            }
            Serial.println(message);
        }
    }

    /**
     * 輸出成功訊息（即使 debug 關閉也會顯示）
     * @param message 成功訊息
     */
    void success(const char* message) const
    {
        if (message)
        {
            if (module_name)
            {
                Serial.printf("[%s] ✅ ", module_name);
            }
            else
            {
                Serial.print("✅ ");
            }
            Serial.println(message);
        }
    }
};

/**
 * 全域 Debug 控制器（可選）
 * 用於控制所有模組的 debug 開關
 */
class GlobalDebugController
{
private:
    static bool global_debug_enabled;

public:
    /**
     * 設置全域 debug 開關
     */
    static void set_global_debug(bool enable)
    {
        global_debug_enabled = enable;
    }

    /**
     * 獲取全域 debug 狀態
     */
    static bool is_global_debug_enabled()
    {
        return global_debug_enabled;
    }
};

#endif // DEBUG_PRINT_H
