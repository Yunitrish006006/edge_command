#include <Arduino.h>
#include "TensorFlowLite_ESP32.h"

#include "hello_world_model_data.h"

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
    Serial.println("=== Hello World TensorFlow Lite Example ===");
    Serial.println("ESP32-S3 Starting...");
    Serial.println("Serial communication established!");
    Serial.println("Starting main loop in 2 seconds...");
    delay(2000);
}

void loop()
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
