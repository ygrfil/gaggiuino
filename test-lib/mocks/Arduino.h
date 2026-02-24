#ifndef TEST_MOCK_ARDUINO_H
#define TEST_MOCK_ARDUINO_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <chrono>
#include <thread>

typedef uint8_t byte;
typedef bool boolean;
typedef uint16_t word;

using String = std::string;

#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif
#ifndef INPUT
#define INPUT 0x0
#endif
#ifndef OUTPUT
#define OUTPUT 0x1
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x2
#endif
#ifndef OUTPUT_OPEN_DRAIN
#define OUTPUT_OPEN_DRAIN 0x5
#endif

#ifndef FALLING
#define FALLING 0x2
#endif
#ifndef RISING
#define RISING 0x3
#endif
#ifndef CHANGE
#define CHANGE 0x4
#endif

#ifndef DEC
#define DEC 10
#endif
#ifndef HEX
#define HEX 16
#endif
#ifndef OCT
#define OCT 8
#endif
#ifndef BIN
#define BIN 2
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif

#ifndef F
#define F(x) x
#endif

class Print {
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) { return 1u; }
  virtual size_t write(const uint8_t *, size_t size) { return size; }
  virtual size_t print(const char *) { return 0u; }
  virtual size_t print(const String &) { return 0u; }
  virtual size_t print(char) { return 0u; }
  virtual size_t print(int, int = DEC) { return 0u; }
  virtual size_t print(unsigned int, int = DEC) { return 0u; }
  virtual size_t print(long, int = DEC) { return 0u; }
  virtual size_t print(unsigned long, int = DEC) { return 0u; }
  virtual size_t print(double, int = 2) { return 0u; }
  virtual size_t println(void) { return 0u; }
  virtual size_t println(const char *) { return 0u; }
  virtual size_t println(const String &) { return 0u; }

  template <typename T>
  size_t print(const T &) { return 0u; }

  template <typename T>
  size_t println(const T &value) {
    (void)value;
    return 0u;
  }
};

class Stream : public Print {
public:
  virtual int available(void) { return 0; }
  virtual int read(void) { return -1; }
  virtual int peek(void) { return -1; }
  virtual void flush(void) {}
};

class HardwareSerial : public Stream {
public:
  void begin(unsigned long) {}
};

extern HardwareSerial Serial;
extern HardwareSerial Serial1;
extern HardwareSerial Serial2;

inline unsigned long millis(void) {
  static auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

inline unsigned long micros(void) {
  static auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return (unsigned long)std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
}

inline void delay(unsigned long ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void delayMicroseconds(unsigned int us) {
  std::this_thread::sleep_for(std::chrono::microseconds(us));
}

inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return LOW; }
inline int analogRead(uint8_t) { return 0; }
inline void analogWrite(uint8_t, int) {}

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  if (in_max == in_min) return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

template <typename T>
inline T constrain(T x, T a, T b) {
  return x < a ? a : (x > b ? b : x);
}

inline long random(long max) {
  return max > 0 ? (long)(rand() % max) : 0;
}

inline long random(long min, long max) {
  if (max <= min) return min;
  return min + random(max - min);
}

inline void randomSeed(unsigned long seed) {
  srand((unsigned int)seed);
}

inline void yield(void) {}

#endif
