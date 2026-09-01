// ─────────────────────────────────────────────────────────────
// FasterEdge 开源项目
// Github: https://github.com/FasterEdge
// Gitee:  https://gitee.com/FasterEdge
// ─────────────────────────────────────────────────────────────
// fe_port.c — FasterEdge MCU 平台移植层实现（STC8 (8051 增强 1T) 版，Keil C51 工具链）
// 目标芯片：STC8H8K64U（uvproj Device）。内置 EEPROM（IAP，寄存器 0xC2-0xC7）。
#include "fe_port.h"
#include <reg52.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// ============================================================
// 格式化输出
// ============================================================
// Keil C51 无标准 snprintf：委托 vsprintf 到临时缓冲后截断。
int fe_snprintf(char *buf, u16 size, const char *fmt, ...) {
    va_list ap;
    char tmp[128];
    size_t n;
    va_start(ap, fmt);
    vsprintf(tmp, fmt, ap);
    va_end(ap);
    n = strlen(tmp);
    if (n >= size) n = size - 1;
    memcpy(buf, tmp, n);
    buf[n] = 0;
    return (int)n;
}

// ============================================================
// 串口（UART0，定时器 1 作波特率）
// ============================================================
static fe_port_uart_rx_cb_t g_rx_cb = NULL;
static void *g_rx_user = NULL;

// 波特率重载值：TH1 = 256 - FOSC/(12*32*baud)
static u8 baud_reload(u32 baud) {
    u32 t = FOSC / 12UL / 32UL;
    u8 v = 0;
    if (baud > 0) {
        t /= baud;
        if (t < 256) v = (u8)(256 - t);
        else v = 1;
    }
    return v;
}

void fe_port_uart_init(u8 port, u32 baud, fe_port_uart_rx_cb_t rx_cb, void *user) {
    u8 reload;
    (void)port;
    g_rx_cb = rx_cb;
    g_rx_user = user;
    reload = baud_reload(baud);
    TMOD = (TMOD & 0x0F) | 0x20;   // 定时器1 模式2（8 位自动重载）
    TH1 = reload;
    TL1 = reload;
    SCON = 0x50;                   // 模式1，REN=1 允许接收
    TR1 = 1;                       // 启动波特率定时器
    if (rx_cb) { ES = 1; EA = 1; } // 需要回调时开串口中断
}

// 串口接收中断（中断号 4）
void fe_port_uart_isr(void) interrupt 4 using 1 {
    u8 b;
    if (!RI) return;
    RI = 0;
    b = SBUF;
    if (g_rx_cb) g_rx_cb(b, g_rx_user);
}

u16 fe_port_uart_write(u8 port, const u8 *data, u16 len) {
    u16 i;
    (void)port;
    for (i = 0; i < len; i++) {
        while (!TI);
        TI = 0;
        SBUF = data[i];
    }
    return len;
}

u8 fe_port_uart_available(u8 port) {
    (void)port;
    return RI ? TRUE : FALSE;
}

int fe_port_uart_read(u8 port) {
    (void)port;
    if (!RI) return -1;
    RI = 0;
    return SBUF;
}

void fe_port_uart_close(u8 port) {
    (void)port;
    ES = 0;
}

// ============================================================
// EEPROM（STC8 内置 IAP，寄存器 0xC2-0xC7；扇区 512B）
// ============================================================
sfr IAP_DATA  = 0xC2;
sfr IAP_ADDRH = 0xC3;
sfr IAP_ADDRL = 0xC4;
sfr IAP_CMD   = 0xC5;
sfr IAP_TRIG  = 0xC6;
sfr IAP_CONTR = 0xC7;

#define FE_IAP_SECTOR 512

static void iap_idle(void) {
    IAP_CONTR = 0; IAP_CMD = 0; IAP_TRIG = 0;
    IAP_ADDRH = 0; IAP_ADDRL = 0;
}

static u8 iap_read(u16 addr) {
    u8 d;
    IAP_CONTR = 0x80; IAP_CMD = 1;
    IAP_ADDRL = (u8)addr; IAP_ADDRH = (u8)(addr >> 8);
    IAP_TRIG = 0x5A; IAP_TRIG = 0xA5;
    d = IAP_DATA;
    iap_idle();
    return d;
}

static void iap_write(u16 addr, u8 d) {
    IAP_CONTR = 0x80; IAP_CMD = 2;
    IAP_ADDRL = (u8)addr; IAP_ADDRH = (u8)(addr >> 8);
    IAP_DATA = d;
    IAP_TRIG = 0x5A; IAP_TRIG = 0xA5;
    iap_idle();
}

static void iap_erase(u16 addr) {
    IAP_CONTR = 0x80; IAP_CMD = 3;
    IAP_ADDRL = (u8)addr; IAP_ADDRH = (u8)(addr >> 8);
    IAP_TRIG = 0x5A; IAP_TRIG = 0xA5;
    iap_idle();
}

// 写 N 字节到同一扇区：读回扇区、改字节、擦除、重写
static u8 iap_write_sector(u16 addr, const u8 *src, u16 n) {
    u16 base = (u16)(addr & (u16)~(FE_IAP_SECTOR - 1));
    u16 i, off = (u16)(addr - base);
    u8 xdata page[FE_IAP_SECTOR];
    for (i = 0; i < FE_IAP_SECTOR; i++)
        page[i] = iap_read((u16)(base + i));
    for (i = 0; i < n && off + i < FE_IAP_SECTOR; i++)
        page[off + i] = src[i];
    iap_erase(base);
    for (i = 0; i < FE_IAP_SECTOR; i++)
        iap_write((u16)(base + i), page[i]);
    return TRUE;
}

u8 fe_port_eeprom_get_str(u16 addr, char *out, u16 outlen) {
    u16 i;
    if (!out || outlen == 0) return FALSE;
    for (i = 0; i + 1 < outlen; i++) {
        u8 c = iap_read((u16)(addr + i));
        out[i] = (char)c;
        if (c == 0) return TRUE;
    }
    out[outlen - 1] = 0;
    return TRUE;
}

u8 fe_port_eeprom_set_str(u16 addr, const char *value) {
    return iap_write_sector(addr, (const u8 *)value, (u16)strlen(value) + 1);
}

u8 fe_port_eeprom_get_u32(u16 addr, u32 *out) {
    u8 i;
    u32 v = 0;
    if (!out) return FALSE;
    for (i = 0; i < 4; i++)
        v |= (u32)iap_read((u16)(addr + i)) << (8 * i);
    *out = v;
    return TRUE;
}

u8 fe_port_eeprom_set_u32(u16 addr, u32 value) {
    u8 b[4], i;
    for (i = 0; i < 4; i++) b[i] = (u8)(value >> (8 * i));
    return iap_write_sector(addr, b, 4);
}

// ============================================================
// 系统时间——定时器 0（模式 1）50ms 中断计数
// ============================================================
static volatile u32 s_epoch_base;
static volatile u8  s_second_count;
static volatile u8  s_timer0_ready;

#define TIMER0_RELOAD (65536UL - FOSC / 12UL / 20UL)   // 50ms @12T

void fe_port_timer0_isr(void) interrupt 1 using 1 {
    TH0 = (u8)(TIMER0_RELOAD >> 8);
    TL0 = (u8)TIMER0_RELOAD;
    if (++s_second_count >= 20) {   // 20 * 50ms = 1s
        s_second_count = 0;
        s_epoch_base++;
    }
}

static void timer0_start(void) {
    if (s_timer0_ready) return;
    s_timer0_ready = 1;
    s_second_count = 0;
    TMOD = (TMOD & 0xF0) | 0x01;   // 定时器0 模式1（16 位）
    TH0 = (u8)(TIMER0_RELOAD >> 8);
    TL0 = (u8)TIMER0_RELOAD;
    ET0 = 1;                       // 开定时器0 中断
    EA  = 1;                       // 开总中断
    TR0 = 1;                       // 启动
}

u32 fe_port_time_now(void) {
    timer0_start();
    return s_epoch_base;
}

void fe_port_time_set(u32 epoch) {
    timer0_start();
    s_epoch_base = epoch;
}

// ============================================================
// 随机数
// ============================================================
void fe_port_random_fill(u8 *buf, u16 len) {
    static u32 state = 0xFE51C51u;
    u16 i;
    timer0_start();
    state ^= (u32)TL0 << 8 | TH0;
    for (i = 0; i < len; i++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        buf[i] = (u8)(state >> 24);
    }
}

// ============================================================
// 延时
// ============================================================
void fe_port_delay_ms(u32 ms) {
    volatile u32 i;
    for (; ms > 0; ms--)
        for (i = 0; i < 1000; i++) ;
}

// ============================================================
// 寄存器 / 存储空间读写（跳转表）
// ============================================================
u8 fe_port_sfr_read(u8 addr) {
    switch (addr) {
        case 0x80: return P0;   case 0x90: return P1;
        case 0xA0: return P2;   case 0xB0: return P3;
        case 0x88: return TCON; case 0x98: return SCON;
        case 0x8A: return TL0;  case 0x8B: return TL1;
        case 0x8C: return TH0;  case 0x8D: return TH1;
        case 0xA8: return IE;   case 0xB8: return IP;
        case 0xD0: return PSW;  case 0xE0: return ACC;
        case 0xF0: return B;
        default: return 0;
    }
}

void fe_port_sfr_write(u8 addr, u8 val) {
    switch (addr) {
        case 0x80: P0 = val; break;   case 0x90: P1 = val; break;
        case 0xA0: P2 = val; break;   case 0xB0: P3 = val; break;
        case 0x8A: TL0 = val; break;  case 0x8B: TL1 = val; break;
        case 0x8C: TH0 = val; break;  case 0x8D: TH1 = val; break;
        default: break;
    }
}

u8 fe_port_xram_read(u16 addr) {
    return *(volatile u8 xdata *)addr;
}

void fe_port_xram_write(u16 addr, u8 val) {
    *(volatile u8 xdata *)addr = val;
}

// ============================================================
// 芯片信息
// ============================================================
void fe_port_chip_info(char *out, u16 outlen) {
    fe_snprintf(out, outlen,
        "{\"chip\":\"STC8H8K64U\",\"arch\":\"MCS-51 (1T)\","
        "\"ramBytes\":8192,\"flashBytes\":63488,\"eepromBytes\":5120,\"freqMHz\":%lu}",
        (unsigned long)(FOSC / 1000000UL));
}

/* 说明：STC8 内置 IAP 直接使用上方 sfr IAP_*（0xC2-0xC7）。
 * STC89/STC15 系列 IAP 寄存器地址为 0xE2-0xE7，按数据手册调整即可。
 * 8051 通用件（UART/时间/随机/SFR/XRAM）与 C51 版完全一致。
 */
