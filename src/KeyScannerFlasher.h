#ifndef KEYSCANNERFLASHER_H_
#define KEYSCANNERFLASHER_H_

#include "stdio.h"
#include "ApiBootloaderKeyscanner.h"
#include "Wire.h"

#define WIRE_ Wire1
#define watchdog_update()


class KeyScannerFlasher {
 public:
  enum Side : uint8_t {
    RIGHT,
    LEFT,
  };

  void setLeftBootAddress(uint8_t left_boot_address_);
  void setRightBootAddress(uint8_t right_boot_address);

  bool getInfoFlasherKS(InfoAction &info_action);
  uint32_t sendWriteAction(WriteAction write_action, uint8_t *data);
  bool sendEraseAction(EraseAction erase_action);
  bool sendReadAction(ReadAction read_action);
  bool sendValidateProgram();
  bool sendBegin();
  bool sendFinish();
  bool sendJump(uint32_t address_to_jump);
  uint16_t readData(uint8_t *data, size_t size);
  void setSide(Side side);

  void setSideInfo(InfoAction info_action) {
    InfoAction &info = side_ ? infoLeft : infoRight;
    info             = info_action;
  };

  InfoAction getInfoAction() {
    InfoAction &info = side_ ? infoLeft : infoRight;
    return info;
  }


 private:
  uint8_t address{};
  uint8_t left_boot_address_{};
  uint8_t right_boot_address_{};
  InfoAction infoLeft{};
  InfoAction infoRight{};
  Side side_{RIGHT};
  uint32_t align(uint32_t val, uint32_t to);
  uint8_t readData(uint8_t address, uint8_t *message, uint32_t lenMessage, bool stopBit = false);
  uint8_t sendMessage(uint8_t address, uint8_t *message, uint32_t lenMessage, bool stopBit = false);
  uint8_t sendCommand(uint8_t address, uint8_t command, bool stopBit = false);
};

#endif  //KEYSCANNERFLASHER_H_
