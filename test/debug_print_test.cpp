/**
 * DebugPrint 模組測試範例
 * 展示如何使用通用 DebugPrint 模組
 */

#include <Arduino.h>
#include "debug_print.h"

// 創建一個簡單的測試類別
class TestModule
{
private:
    DebugPrint debug;
    int counter;
    
public:
    TestModule() : debug("TestModule", true), counter(0)  // debug 預設開啟用於演示
    {
        debug.print("建構函數被調用");
    }
    
    void test_basic_print()
    {
        debug.print("=== 測試基本輸出 ===");
        debug.print("這是一條簡單的 debug 訊息");
    }
    
    void test_formatted_print()
    {
        debug.print("=== 測試格式化輸出 ===");
        int value = 42;
        float pi = 3.14159;
        debug.printf("整數: %d, 浮點數: %.2f\n", value, pi);
        debug.printf("計數器值: %d\n", counter++);
    }
    
    void test_levels()
    {
        debug.print("=== 測試不同級別的輸出 ===");
        
        // 這些訊息即使 debug 關閉也會顯示
        debug.info("這是一條資訊訊息");
        debug.success("這是一條成功訊息");
        debug.warning("這是一條警告訊息");
        debug.error("這是一條錯誤訊息");
        debug.error_f("格式化錯誤: 代碼 %d\n", -1);
    }
    
    void test_debug_control()
    {
        debug.print("=== 測試 Debug 控制 ===");
        
        debug.printf("當前 debug 狀態: %s\n", 
                    debug.is_debug_enabled() ? "啟用" : "關閉");
        
        // 關閉 debug
        debug.set_debug(false);
        debug.print("這條訊息不會顯示（debug 已關閉）");
        
        // 但這些訊息仍然會顯示
        debug.info("即使 debug 關閉，info 仍然顯示");
        
        // 重新啟用 debug
        debug.set_debug(true);
        debug.print("Debug 已重新啟用");
    }
    
    void set_debug(bool enable) { debug.set_debug(enable); }
    DebugPrint& get_debug() { return debug; }
};

// 全域測試實例
TestModule test_module;

void setup()
{
    // 初始化 Serial
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n========================================");
    Serial.println("DebugPrint 模組測試程式");
    Serial.println("========================================\n");
    
    // 執行各種測試
    test_module.test_basic_print();
    delay(1000);
    
    test_module.test_formatted_print();
    delay(1000);
    
    test_module.test_levels();
    delay(1000);
    
    test_module.test_debug_control();
    
    Serial.println("\n========================================");
    Serial.println("所有測試完成！");
    Serial.println("========================================\n");
}

void loop()
{
    // 定期輸出測試訊息
    static unsigned long last_test = 0;
    unsigned long current_time = millis();
    
    if (current_time - last_test > 5000)
    {
        test_module.get_debug().printf("運行時間: %lu 秒\n", current_time / 1000);
        last_test = current_time;
    }
    
    delay(100);
}
