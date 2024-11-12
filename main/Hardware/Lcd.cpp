#include "Lcd.hpp"
#include "I2C.hpp"
#include "utils.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>

#include "U8g2lib.h"
#include "esp_log.h"
#include "u8g2.h"

static constexpr char TAG[] = "Lcd";

#define STRING_LINE_COEF 1.2

extern "C" uint8_t u8x8_byte_hw_i2c_cb(U8X8_UNUSED u8x8_t *u8x8,
                                       U8X8_UNUSED uint8_t msg,
                                       U8X8_UNUSED uint8_t arg_int,
                                       U8X8_UNUSED void *arg_ptr) {
  static uint8_t s_buf[100];
  static unsigned s_cnt = 0;

  switch (msg) {
  case U8X8_MSG_BYTE_INIT:
    if (u8x8->bus_clock == 0)
      u8x8->bus_clock = u8x8->display_info->i2c_bus_clock_100kHz * 100000UL;
    break;

  case U8X8_MSG_BYTE_SEND: {
    unsigned rest =
      std::min((unsigned)arg_int, sizeof(s_buf) / sizeof(s_buf[0]) - s_cnt);
    uint8_t *ptr = (uint8_t *)arg_ptr;
    while (rest--)
      s_buf[s_cnt++] = *ptr++;
  } break;

  case U8X8_MSG_BYTE_START_TRANSFER:
    break;
  case U8X8_MSG_BYTE_END_TRANSFER: {
    bool ret = g_i2c.write(u8x8_GetI2CAddress(u8x8) >> 1, s_buf, s_cnt);
    (void)ret;
    s_cnt = 0;
  } break;
  default:
    return 0;
  }
  return 1;
}

extern "C" uint8_t u8x8_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg,
                                          uint8_t arg_int,
                                          U8X8_UNUSED void *arg_ptr) {
  return 1;
}

class U8G2_SSD1306_128X64_CUSTOM : public U8G2 {
public:
  U8G2_SSD1306_128X64_CUSTOM(const u8g2_cb_t *rotation,
                             uint8_t reset = U8X8_PIN_NONE,
                             uint8_t clock = U8X8_PIN_NONE,
                             uint8_t data = U8X8_PIN_NONE)
    : U8G2() {
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, rotation, u8x8_byte_hw_i2c_cb,
                                           u8x8_gpio_and_delay_cb);
    u8x8_SetPin_HW_I2C(getU8x8(), reset, clock, data);
  }
};

static U8G2_SSD1306_128X64_CUSTOM u8g2(U8G2_R0, U8X8_PIN_NONE);

Lcd::Lcd(Rotation rot) : IDisplay(), y_offset_(0) {
  if (!g_i2c.open()) {
    ESP_LOGE(TAG, "Failed to open I2C");
  }
  u8g2.begin();
  setRotation(rot);
  setFont(IDisplay::Font::COURB24);
}

void Lcd::setRotation(Rotation rot) const {
  switch (rot) {
  default:
  case Rotation::PORTRAIT:
    u8g2.setDisplayRotation(U8G2_R0);
    break;
  case Rotation::UPSIDE_DOWN:
    u8g2.setDisplayRotation(U8G2_R2);
    break;
  }
}

void Lcd::setFont(Font f) {
  switch (f) {
  default:
  case Font::COURR08:
    u8g2.setFont(u8g_font_courR08);
    break;
  case Font::COURR10:
    u8g2.setFont(u8g_font_courR10);
    break;
  case Font::COURB10:
    u8g2.setFont(u8g_font_courB10);
    break;
  case Font::COURB14:
    u8g2.setFont(u8g_font_courB14);
    break;
  case Font::COURB18:
    u8g2.setFont(u8g_font_courB18);
    break;
  case Font::COURB24:
    u8g2.setFont(u8g_font_courB24);
    break;
  case Font::F6X10TR:
    u8g2.setFont(u8g2_font_6x10_tr);
    break;
  case Font::F8X13B:
    u8g2.setFont(u8g_font_8x13B);
    break;
  case Font::F9X15:
    u8g2.setFont(u8g_font_9x15);
    break;
  case Font::F9X18:
    u8g2.setFont(u8g_font_9x18);
    break;
  case Font::CYR_6x12:
    u8g2.setFont(u8g2_font_6x12_t_cyrillic);
    break;
  case Font::CYR_10x20:
    u8g2.setFont(u8g2_font_10x20_t_cyrillic);
    break;
  }

  ESP_LOGD(TAG, "char_width=%d, char_height=%d", u8g2.getMaxCharWidth(),
           u8g2.getMaxCharHeight());
  y_offset_ = float(u8g2.getMaxCharHeight()) * STRING_LINE_COEF;
}

void Lcd::draw_string(unsigned x, unsigned y, const char *fmt, va_list argp) {
  const unsigned buf_size = 64;
  char buf[buf_size];
  vsnprintf(buf, buf_size, fmt, argp);

  u8g2.drawUTF8(x, y, buf);
}

void Lcd::print_string_ln(const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  draw_string(0, y_offset_, fmt, argp);
  va_end(argp);
  y_offset_ += float(u8g2.getMaxCharHeight()) * STRING_LINE_COEF;
}

void Lcd::print_string(unsigned x, unsigned y, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  draw_string(x, y, fmt, argp);
  va_end(argp);
}

void Lcd::draw(unsigned x, unsigned y, unsigned w, unsigned h,
               const uint8_t *bitmap) {
  u8g2.setBitmapMode(1);
  u8g2.setFontMode(1);
  u8g2.drawXBM(x, y, w, h, bitmap);
}

void Lcd::get_font_sz(unsigned &w, unsigned &h) {
  w = u8g2.getMaxCharWidth();
  h = u8g2.getMaxCharHeight();
}

void Lcd::send() {
  u8g2.sendBuffer();
  u8g2.clearBuffer();
  y_offset_ = float(u8g2.getMaxCharHeight()) * STRING_LINE_COEF;
}

void Lcd::clear() {
  u8g2.clearBuffer();
  y_offset_ = float(u8g2.getMaxCharHeight()) * STRING_LINE_COEF;
}
