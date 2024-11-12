#pragma once
#include "stdint.h"
#include "stdio.h"

struct sample_info_t {
  const unsigned char *data;
  unsigned int len;
  const char *label;
};

struct samples_table_t {
  const sample_info_t *samples;
  unsigned int len;
  float volume;
};

struct Sample_t {
  const void *data;
  size_t bytes;
  float volume;
};

struct wav_header_t {
  char chunkID[4]; // "RIFF" = 0x46464952
  uint32_t chunkSize;
  char format[4];      // "WAVE" = 0x45564157
  char subchunk1ID[4]; // "fmt " = 0x20746D66
  uint32_t subchunk1Size;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char subchunk2ID[4]; // "data" = 0x61746164
  uint32_t subchunk2Size;
};
