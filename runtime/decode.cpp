#include <cstdint>
#include <cstring>

extern "C" char *decode(char *str) {

  static char buffer[1024];

  std::strcpy(buffer, str);

  uint8_t Key[] = {0x12, 0x34, 0x56};

  int i = 0;

  while (buffer[i] != '\0') {

    buffer[i] = static_cast<char>(static_cast<uint8_t>(buffer[i]) ^ Key[i % 3]);

    i++;
  }

  return buffer;
}