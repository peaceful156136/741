/********************************************
 * 合并后的 main.c
 * 包含 DS18B20 全部操作函数、定时器、按键、数码管显示
 * 晶振：12MHz
 ********************************************/

#include <reg51.h>
#include <intrins.h>

// ========== 类型定义 ==========
#ifndef uchar
#define uchar unsigned char
#endif
#ifndef uint
#define uint unsigned int
#endif

// ========== DS18B20 IO 定义 ==========
sbit DSPORT = P3^2;

// ========== 按键定义 ==========
sbit k1 = P1^0;   // 档位1
sbit k2 = P1^1;   // 档位2
sbit k3 = P1^2;   // 档位3
sbit k4 = P1^3;   // 档位4
sbit k5 = P1^4;   // 风速低
sbit k6 = P1^5;   // 风速中
sbit k7 = P1^6;   // 风速高
sbit k8 = P1^7;   // 定时加
sbit k9 = P3^7;   // 摇摆开关
sbit k10 = P3^6;  // 启动
sbit k11 = P3^5;  // 停止
sbit k12 = P3^4;  // 自动模式

// ========== 输出定义 ==========
sbit out1 = P3^0;  // 加热
sbit out2 = P3^1;  // 摇摆
sbit beep = P3^3;  // 蜂鸣器

// ========== 数码管位选 ==========
sbit smg1 = P2^0;
sbit smg2 = P2^1;
sbit smg3 = P2^2;
sbit smg4 = P2^3;
sbit smg5 = P2^4;
sbit smg6 = P2^5;
sbit smg7 = P2^6;
sbit smg8 = P2^7;

// ========== 段码表 ==========
uchar code smgduan0[10] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f}; // 不带点
uchar code smgduan1[10] = {0xbf,0x86,0xdb,0xcf,0xe6,0xed,0xfd,0x87,0xff,0xef}; // 带点

// ========== 全局变量 ==========
uchar mode = 0;
uchar auto_mode = 0;
uchar pwm = 40;
uchar time = 0;
uint time1 = 0, time2 = 0;
uchar yao_time = 1;
uint sec = 0;
uchar miao = 0;
uchar start = 0;
uchar yao = 0;
uchar ding = 0;
uchar dang = 1;
uchar feng = 1;
int wendu = 0;
uchar zan = 0;
uchar alarm_type = 0;   // 0-正常, 1-高温, 2-低温
uchar alarm_counter = 0;

// ========== 基础延时 ==========
void delay_ms(uint ms) {
    uint i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 123; j++);
}

// DS18B20 专用 10us 延时 (12MHz)
void delay_10us(uchar t) {
    while(t--) {
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
    }
}

// ========== DS18B20 操作函数 ==========
uchar Ds18b20Init() {
    uchar retry = 0;
    DSPORT = 0;
    delay_10us(50);
    DSPORT = 1;
    delay_10us(6);
    while (DSPORT && retry < 200) {
        retry++;
        delay_10us(1);
    }
    if (retry >= 200) return 0;
    retry = 0;
    while (!DSPORT && retry < 240) {
        retry++;
        delay_10us(1);
    }
    return 1;
}

void Ds18b20WriteByte(uchar dat) {
    uint i;
    uchar j;
    bit testb;
    for (j = 1; j <= 8; j++) {
        testb = dat & 0x01;
        dat >>= 1;
        if (testb) {
            DSPORT = 0;
            i++; i++;
            DSPORT = 1;
            delay_10us(6);
        } else {
            DSPORT = 0;
            delay_10us(6);
            DSPORT = 1;
            i++; i++;
        }
    }
}

uchar Ds18b20ReadByte() {
    uchar i, j, dat = 0;
    for (j = 1; j <= 8; j++) {
        dat >>= 1;
        DSPORT = 0;
        i++; i++;
        DSPORT = 1;
        delay_10us(1);
        if (DSPORT) dat |= 0x80;
        delay_10us(5);
    }
    return dat;
}

void Ds18b20ChangTemp() {
    Ds18b20Init();
    delay_10us(1);
    Ds18b20WriteByte(0xCC);
    Ds18b20WriteByte(0x44);
}

void Ds18b20ReadTempCom() {
    Ds18b20Init();
    delay_10us(1);
    Ds18b20WriteByte(0xCC);
    Ds18b20WriteByte(0xBE);
}

int Ds18b20ReadTemp() {
    uchar tmh, tml;
    int temp = 0;
    uchar flag = 0;
    Ds18b20ChangTemp();
    Ds18b20ReadTempCom();
    tml = Ds18b20ReadByte();
    tmh = Ds18b20ReadByte();
    if (tmh > 0x7F) {
        flag = 1;
        tmh = ~tmh;
        tml = ~tml;
        if (tml + 1 > 0xFF) tmh++;
        tml++;
    }
    temp = tmh;
    temp <<= 8;
    temp |= tml;
    temp = temp / 16;
    if (flag) temp = -temp;
    return temp;
}

// ========== 主函数 ==========
void main() {
    out2 = 0;          // 初始关闭摇摆（高电平有效，所以给0）
    TMOD |= 0x01;
    TH0 = 0xFC;
    TL0 = 0x18;
    ET0 = 1;
    EA = 1;
    TR0 = 1;

    while (1) {
        // ====== 数码管动态扫描 ======
        // 第5位：温度符号
        if (alarm_type == 0) {
            if (wendu >= 0) P0 = smgduan0[0];  // 显示0
            else P0 = 0x40;                    // 显示负号
        } else if (alarm_type == 1) {
            P0 = 0x76;  // 'H'
        } else {
            P0 = 0x38;  // 'L'
        }
        smg5 = 0; delay_ms(1); smg5 = 1;

        // 第6位：温度十位
        P0 = smgduan0[((wendu < 0) ? (-wendu) : wendu) / 10];
        smg6 = 0; delay_ms(1); smg6 = 1;

        // 第7位：温度个位
        P0 = smgduan0[((wendu < 0) ? (-wendu) : wendu) % 10];
        smg7 = 0; delay_ms(1); smg7 = 1;

        // 第8位：档位（无小数点）
        P0 = smgduan0[dang];
        smg8 = 0; delay_ms(1); smg8 = 1;

        // 第1位：档位（带小数点）
        P0 = smgduan1[dang];
        smg1 = 0; delay_ms(1); smg1 = 1;

        // 第2位：风速（带小数点）
        P0 = smgduan1[feng];
        smg2 = 0; delay_ms(1); smg2 = 1;

        // 第3位：定时十位
        P0 = smgduan0[ding / 10];
        smg3 = 0; delay_ms(1); smg3 = 1;

        // 第4位：定时个位
        P0 = smgduan0[ding % 10];
        smg4 = 0; delay_ms(1); smg4 = 1;

        // ====== 按键检测 ======
        if (!k1) { delay_ms(20); if (!k1) { dang = 1; auto_mode = 0; while (!k1); } }
        if (!k2) { delay_ms(20); if (!k2) { dang = 2; auto_mode = 0; while (!k2); } }
        if (!k3) { delay_ms(20); if (!k3) { dang = 3; auto_mode = 0; while (!k3); } }
        if (!k4) { delay_ms(20); if (!k4) { dang = 4; auto_mode = 0; while (!k4); } }
        if (!k5) { delay_ms(20); if (!k5) { feng = 1; auto_mode = 0; while (!k5); } }
        if (!k6) { delay_ms(20); if (!k6) { feng = 2; auto_mode = 0; while (!k6); } }
        if (!k7) { delay_ms(20); if (!k7) { feng = 3; auto_mode = 0; while (!k7); } }
        if (!k8) { delay_ms(20); if (!k8) { if (ding < 90) ding += 10; if (ding > 99) ding = 99; while (!k8); } }
        if (!k9) { delay_ms(20); if (!k9) { yao = !yao; while (!k9); } }
        if (!k10) { delay_ms(20); if (!k10) { start = 1; while (!k10); } }
        if (!k11) { delay_ms(20); if (!k11) { start = 0; yao = 0; ding = 0; auto_mode = 0; while (!k11); } }
        if (!k12) { delay_ms(20); if (!k12) { auto_mode = !auto_mode; while (!k12); } }
    }
}

// ========== 定时器0中断（1ms） ==========
void Timer0() interrupt 1 {
    TH0 = 0xFC;
    TL0 = 0x18;

    if (zan == 0) {   // 正常模式（非报警）
        if (start) {
            // PWM 加热控制
            if (time < 99) time++;
            else time = 0;
            if (time < pwm) out1 = 0;
            else out1 = 1;

            // 摇摆控制
            if (time2 < 20) time2++;
            else time2 = 0;
            if (time2 < yao_time) out2 = 1;
            else out2 = 0;

            // 摇摆周期切换（2秒翻转）
            if (yao) {
                if (time1 < 2000) time1++;
                else {
                    time1 = 0;
                    yao_time = (yao_time == 1) ? 2 : 1;
                }
            }
        } else {
            yao = 0;
            out1 = 1;   // 关闭加热
        }
    }

    // 每 0.5 秒执行一次任务
    if (sec < 500) sec++;
    else {
        sec = 0;

        // 根据风速计算 PWM 占空比
        switch (feng) {
            case 1: pwm = dang * 20 + (~pwm) % 20; break;
            case 2: pwm = dang * 20; break;
            case 3: pwm = dang * 5; break;
        }
        if (pwm > 99) pwm = 99;

        // 读取温度
        wendu = Ds18b20ReadTemp();

        // 温度报警判断
        if (wendu > 70) {
            alarm_type = 1;   // 高温
            zan = 1;
        } else if (wendu < 0) {
            alarm_type = 2;   // 低温
            zan = 1;
        } else {
            alarm_type = 0;   // 正常
            beep = 1;         // 关闭蜂鸣器
            zan = 0;
        }

        // 定时倒计时（每0.5秒减0.5分钟？实际每1秒减1分钟？原代码逻辑：miao计数到2为1秒，然后ding减1，即每1秒减1分钟）
        if (ding) {
            if (miao < 2) miao++;
            else {
                miao = 0;
                if (ding > 1) ding--;
                else {
                    start = 0;
                    ding = 0;
                    yao = 0;
                }
            }
        }

        // 自动模式：根据温度自动调整档位
        if (auto_mode) {
            if (wendu < 20) dang = 1;
            else if (wendu < 30) dang = 2;
            else if (wendu < 40) dang = 3;
            else dang = 4;
        }
    }

    // 报警蜂鸣器闪烁（中断中频率）
    if (alarm_type != 0) {
        alarm_counter++;
        if (alarm_type == 1) {   // 高温：0.25s 周期
            if (alarm_counter >= 50) {
                beep = !beep;
                alarm_counter = 0;
            }
        } else if (alarm_type == 2) { // 低温：0.5s 周期
            if (alarm_counter >= 100) {
                beep = !beep;
                alarm_counter = 0;
            }
        }
    }
}