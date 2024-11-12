#ifndef _MIC_PROC_H_
#define _MIC_PROC_H_

#include "stddef.h"
#include "stdint.h"
#include "string.h"

template <typename T> struct dc_blocker {
private:
  T xM_ = 0;
  T yM_ = 0;
  T y_ = 0;

public:
  T proc_val(T val) {
    y_ = val - xM_ + 0.995 * yM_;
    xM_ = val;
    yM_ = y_;
    return y_;
  }
};

template <typename T> T compute_max_abs(T *data, size_t len) {
  T max_abs = 0;
  for (size_t j = 0; j < len; j++) {
    const T abs_val = abs(data[j]);
    if (abs_val > max_abs) {
      max_abs = abs_val;
    }
  }
  return max_abs;
}

#endif // _MIC_PROC_H_
