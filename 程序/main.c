#include <reg51.h>
#include "lcd.h"
#include "temp.h"
#include <intrins.h>

typedef unsigned int u16;	
typedef unsigned char u8;

sbit moto = P1^0;
sbit KEY_PAUSE  = P3^0;
sbit KEY_RESUME = P3^1;
sbit KEY_CLEAR  = P3^2;

sbit beep = P1^5;	   // 蜂鸣器控制引脚

bit stopwatch_running = 1;    
volatile unsigned long stopwatch_count = 0;   
volatile unsigned char lcd_update_flag = 0; 

bit beep_enable = 0;  // 新增：蜂鸣器是否开启标志
bit beep_state = 0;   // 新增：当前蜂鸣器输出电平状态（用于周期闪烁）

uchar CNCHAR[6] = "TEMP";
unsigned long last_stopwatch_value = 0;


// Timer0 中断服务函数
void Timer0_ISR(void) interrupt 1 {
    static unsigned char ten_times = 0;
    static unsigned int beep_counter = 0;  // 新增：用于控制蜂鸣器周期

    TH0 = 0xDC;
    TL0 = 0x00;

    if (stopwatch_running) stopwatch_count++;

    if (++ten_times >= 10) {
        ten_times = 0;
        lcd_update_flag = 1;
    }

    // 蜂鸣器处理：大约每10ms进入一次此中断
    if (beep_enable) {
        if (++beep_counter >= 5) { // 每约50ms翻转一次蜂鸣器电平
            beep_counter = 0;
            beep_state = !beep_state;
            beep = beep_state;
        }
    } else {
        beep = 1;  // 关闭蜂鸣器
        beep_counter = 0;
        beep_state = 0;
    }
}

void delay_ms(u16 ms) {
    u16 i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 120; j++);
}

void delay(u16 i) {
    while(i--);	
}

void UsartConfiguration() {
    SCON=0x50;
    TMOD|=0x20;
    PCON=0x80;
    TH1=0xF3;
    TL1=0xF3;
    TR1=1;
}

void Timer0_Init() {
    TMOD &= 0xF0;
    TMOD |= 0x01;          
    TH0 = 0xDC;            
    TL0 = 0x00;            
    ET0 = 1;
    EA = 1;
    TR0 = 1;
}

//void Timer0_ISR(void) interrupt 1 {
//    static unsigned char ten_times = 0;
//    TH0 = 0xDC;
//    TL0 = 0x00;

//    if (stopwatch_running) stopwatch_count++;

//    if (++ten_times >= 10) {
//        ten_times = 0;
//        lcd_update_flag = 1;
//    }
//}

void LcdDisplayTemperature(int temp) {
    unsigned char i;
    unsigned char datas[] = {0, 0, 0, 0, 0};
    float tp;
    int int_temp;

    LcdWriteCom(0x80);
    LcdWriteData(temp < 0 ? '-' : '+');
    temp = temp < 0 ? ~temp + 1 : temp;

    tp = temp * 0.0625;
    int_temp = (int)(tp * 100 + 0.5);

    datas[0] = int_temp / 10000;
    datas[1] = int_temp % 10000 / 1000;
    datas[2] = int_temp % 1000 / 100;
    datas[3] = int_temp % 100 / 10;
    datas[4] = int_temp % 10;

    LcdWriteCom(0x82);
    for(i = 0; i < 3; i++) LcdWriteData('0' + datas[i]);
    LcdWriteData('.');
    LcdWriteData('0' + datas[3]);
    LcdWriteData('0' + datas[4]);

    for(i = 0; i < 6; i++) LcdWriteData(CNCHAR[i]);
}

void DisplayStopwatch() {
    unsigned long total_centisec = stopwatch_count;
    unsigned int total_seconds = total_centisec / 100;
    unsigned char minutes = total_seconds / 60;
    unsigned char seconds = total_seconds % 60;
    unsigned char centisec = total_centisec % 100;

    LcdWriteCom(0xC0);
    LcdWriteData("ST: ");
    LcdWriteData('0' + minutes / 10);
    LcdWriteData('0' + minutes % 10);
    LcdWriteData(':');
    LcdWriteData('0' + seconds / 10);
    LcdWriteData('0' + seconds % 10);
    LcdWriteData('.');
    LcdWriteData('0' + centisec / 10);
    LcdWriteData('0' + centisec % 10);
}

void KeyScan() {
    if (!KEY_PAUSE) {
        delay_ms(5);
        if (!KEY_PAUSE) {
            stopwatch_running = 0;
            while (!KEY_PAUSE);
        }
    }
    if (!KEY_RESUME) {
        delay_ms(5);
        if (!KEY_RESUME) {
            stopwatch_running = 1;
            while (!KEY_RESUME);
        }
    }
    if (!KEY_CLEAR) {
        delay_ms(5);
        if (!KEY_CLEAR) {
            stopwatch_count = 0;
            stopwatch_running = 0;
            DisplayStopwatch();
            while (!KEY_CLEAR);
        }
    }
}
void main() {
    UsartConfiguration();
    Timer0_Init();
    LcdInit();
    beep = 1;  // 初始化蜂鸣器关闭

    LcdWriteCom(0x88);
    LcdWriteData('C');

    while(1) {
        int current_temp;
        delay(10);
        KeyScan();

        current_temp = Ds18b20ReadTemp();  // 读取温度值
        LcdDisplayTemperature(current_temp);

        // 新逻辑：设置蜂鸣器使能标志
        if (current_temp > 480) {  // 30°C对应480（0.0625分辨率）
            beep_enable = 1;
        } else {
            beep_enable = 0;
        }

        if (lcd_update_flag) {
            lcd_update_flag = 0;
            if (stopwatch_count != last_stopwatch_value) {
                last_stopwatch_value = stopwatch_count;
                DisplayStopwatch();
            }
        }
    }
}
