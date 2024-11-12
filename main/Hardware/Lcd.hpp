#pragma once

#include "IDisplay.hpp"

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64

class Lcd : public IDisplay {
public:
  Lcd(Rotation rot = Rotation::UPSIDE_DOWN);

  void setFont(Font f) override final;
  void setRotation(Rotation rot) const override final;

  void print_string_ln(const char *fmt, ...) override final;
  void print_string(unsigned x, unsigned y, const char *fmt,
                    ...) override final;
  void draw(unsigned x, unsigned y, unsigned w, unsigned h,
            const uint8_t *bitmap) override final;
  void get_font_sz(unsigned &w, unsigned &h) override final;

  void send() override final;
  void clear() override final;

private:
  void draw_string(unsigned x, unsigned y, const char *fmt, va_list argp);
  unsigned y_offset_;
};
