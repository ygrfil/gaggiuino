#ifndef TEST_MOCK_WIRE_H
#define TEST_MOCK_WIRE_H

#include <Arduino.h>

class TwoWire : public Stream {
public:
  using OnReceiveCb = void (*)(int);

  void begin(void) {}
  void beginTransmission(uint8_t) {}
  size_t write(const uint8_t *, size_t size) override { return size; }
  size_t write(uint8_t) override { return 1u; }
  uint8_t endTransmission(void) { return 0u; }
  int available(void) override { return 0; }
  int read(void) override { return -1; }
  void onReceive(OnReceiveCb) {}
};

extern TwoWire Wire;

#endif
