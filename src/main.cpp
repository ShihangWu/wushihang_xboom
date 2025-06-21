#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- 引脚与参数定义 ---
#ifndef __PORT_CONFIG__
#define __PORT_CONFIG__
// 功能引脚定义

#define SM1_PIN 5    // SM1 按键引脚
#define SM2_PIN 18    // SM2 按键引脚
#define SM3_PIN 19    // SM3 按键引脚
#define SM4_PIN 21    // SM4 按键引脚

#define WATERFALL_OUTPUT_PIN 4   // 流水灯输出引脚
#define BUZZER_PIN 2    // 蜂鸣器引脚
#define START_BUTTON_PIN SM1_PIN    // 开始按钮
#define STOP_BUTTON_PIN SM2_PIN    // 停止按钮
#define RESET_BUTTON_PIN SM3_PIN    // 复位按钮
#define WATERFALL_INPUT_PIN SM4_PIN   // 流水灯输入触发按钮

#define BOOM_TIME 10    // 倒计时秒数 (10秒)

// OLED 屏幕参数
#define SCREEN_WIDTH 128  // 修正拼写错误：SCREEN_MIDTH -> SCREEN_WIDTH
#define SCREEN_HEIGHT 64
#define OLED_RESET -1    // 不使用硬件复位
#define SCREEN_ADDRESS 0x3C    // 常用 I2C 地址: 0x3C
#define I2C_SDA_PIN 23
#define I2C_SCL_PIN 22

#endif

// 全局变量
Adafruit_SSD1306 OLED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

hw_timer_t *tim1 = NULL;    // 硬件定时器指针
volatile int timeCount = BOOM_TIME;  // 倒计时变量
int boomStatus = 0;    // X_Boom 状态（暂未使用）
int isStarted = 0;    // 倒计时是否启动标志
int isBeeping = 0;    // 蜂鸣器状态（暂未使用）
bool LED = LOW;    // 流水灯当前状态

// 函数声明
void OLED_Init();
void OLED_Display();
void X_BOOM_Server();
void pinInit();
void Timer_Init();
void tim1Interrupt();  // 修正函数名：timlInterrupt -> tim1Interrupt

// 初始化函数
void setup()
{
    Serial.begin(115200);
    pinInit();    // 初始化所有引脚
    Timer_Init();    // 初始化定时器
    OLED_Init();    // 初始化 OLED
}

void loop()
{
    OLED_Display();    // 每次循环更新 OLED 显示
    X_BOOM_Server(); // 按键检测与逻辑处理
}

// --- 引脚初始化 ---
void pinInit() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(START_BUTTON_PIN, INPUT_PULLUP);
    pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    pinMode(WATERFALL_INPUT_PIN, INPUT_PULLUP);
    pinMode(WATERFALL_OUTPUT_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, LOW); // 初始关闭蜂鸣器
}

// --- 定时器初始化 ---
void Timer_Init() {
    tim1 = timerBegin(0, 80, true); // 定时器0, 80分频 (1us/计数)
    timerAttachInterrupt(tim1, &tim1Interrupt, true); // 绑定中断服务函数
    timerAlarmWrite(tim1, 1000000, true); // 设置周期: 1秒 (1000000us)
    // 如果你想让它每 0.1 秒触发一次，则用 100000
}

// --- OLED 初始化 ---
void OLED_Init() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    OLED.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    OLED.clearDisplay();
    OLED.display();
}

// 定时器中断服务函数
void IRAM_ATTR tim1Interrupt() {  // 修正函数名：timlInterrupt -> tim1Interrupt
    timeCount--; // 每秒触发一次，倒计时减一
}

// --- OLED 显示刷新 ---
void OLED_Display() {
    OLED.clearDisplay();
    OLED.setTextSize(1);
    OLED.setTextColor(WHITE);
    OLED.setCursor(0, 0);

    OLED.print("X_Boom Status: ");
    OLED.println(boomStatus ? "ON" : "OFF");

    OLED.print("Countdown: ");
    float displayTime = (float)timeCount;
    if (displayTime < 0) displayTime = 0.0; // 避免显示负值
    OLED.println(displayTime, 1); // 保留一位小数

    OLED.print("LED Status: ");
    OLED.println(LED ? "OFF" : "ON");

    OLED.display(); // 推送显示
}

// --- 主要逻辑：按键控制、蜂鸣器和流水灯 ---
void X_BOOM_Server() {
    // 流水灯触发按钮（只有在未启动倒计时时才允许手动操作）
    if (!isStarted && digitalRead(WATERFALL_INPUT_PIN) == LOW) {
        LED = !LED;
        digitalWrite(WATERFALL_OUTPUT_PIN, LED);
        Serial.println(LED == LOW ? "流水灯开启（手动）" : "流水灯关闭（手动）");
        delay(500); // 简单防抖
    }


    // 启动倒计时，自动开启流水灯
    if (digitalRead(START_BUTTON_PIN) == LOW && !isStarted) {
        isStarted = 1;
        timerAlarmEnable(tim1); // 启动定时器
        LED = LOW;  // 自动开启流水灯（LED 亮）
        digitalWrite(WATERFALL_OUTPUT_PIN, LED);
        Serial.println("倒计时开始");
        Serial.println("流水灯开启（自动）");
        delay(500); // 防抖
    }
    
    // 暂停倒计时
    if (digitalRead(STOP_BUTTON_PIN) == LOW && isStarted) {
        timerAlarmDisable(tim1);      // 停止定时器中断
        isStarted = 0;                // 更新倒计时标志为未启动
        LED = HIGH;                   // 关闭流水灯（可选）
        digitalWrite(WATERFALL_OUTPUT_PIN, LED);
        Serial.println("倒计时停止");
        Serial.println("流水灯关闭（自动）");
        delay(500);                   // 简单防抖
    }

    
    // 重置倒计时
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
        timeCount = BOOM_TIME;        // 恢复倒计时时长
        isStarted = 0;                // 重置状态
        digitalWrite(BUZZER_PIN, LOW); // 关闭蜂鸣器
        timerAlarmDisable(tim1);      // 停止定时器
        boomStatus = 0;               // 清除爆炸状态标志
        LED = HIGH;                   // 关闭流水灯
        digitalWrite(WATERFALL_OUTPUT_PIN, LED);
        Serial.println("倒计时复位");
        Serial.println("流水灯关闭（自动）");
        delay(500);                   // 防抖
    }


    // 倒计时结束触发蜂鸣器（串口助手只打印一次），自动关闭流水灯
    if (timeCount < 0.1 && boomStatus == 0) {
        boomStatus = 1;                // 标记已响
        timerAlarmDisable(tim1);       // 停止定时器
        digitalWrite(BUZZER_PIN, HIGH); // 打开蜂鸣器
        Serial.println("蜂鸣器响");
        LED = HIGH;
        digitalWrite(WATERFALL_OUTPUT_PIN, LED);
        Serial.println("流水灯关闭（自动）");
    }
}