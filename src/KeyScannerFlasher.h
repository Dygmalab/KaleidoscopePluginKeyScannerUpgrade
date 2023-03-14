#ifndef KEYSCANNERFLASHER_H_
#define KEYSCANNERFLASHER_H_

#include "stdio.h"

#ifdef ARDUINO_ARCH_RP2040

#define WIRE_ Wire1

#endif


class KeyScannerFlasher {
 public:
  enum Action {
    BEGIN            = 'B',
    INFO             = 'I',
    SEAL             = 'S',
    WRITE            = 'W',
    READ             = 'R',
    ERASE            = 'E',
    VALIDATE_PROGRAM = 'V',
    JUMP_ADDRESS     = 'J',
    FINISH           = 'F',
  };

  struct WriteAction {
    uint32_t addr;
    uint32_t size;
  };
  struct ReadAction {
    uint32_t addr;
    uint32_t size;
  };
  struct EraseAction {
    uint32_t addr;
    uint32_t size;
  };

  struct SealHeader {
    uint32_t version; /* Version of the Seal */
    uint32_t size;    /* Size of the Seal */
    uint32_t crc;     /* CRC of the Seal */
  };

  struct Seal {
    /* The header of the Seal */
    SealHeader header;

    /* The Seal */
    uint32_t programStart;
    uint32_t programSize;
    uint32_t programCrc;
    uint32_t programVersion;
  };

  struct InfoAction {
    uint32_t deviceId;
    uint32_t bootloaderVersion;
    uint32_t hardwareVersion;
    uint32_t programVersion;
    uint32_t flashStart;
    uint32_t validationSpaceStart;
    uint32_t programSpaceStart;
    uint32_t flashAvailable;
    uint32_t eraseAlignment;
    uint32_t writeAlignment;
    uint32_t maxTransmissionLength;
  };

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
  Side side_{0};
  uint32_t align(uint32_t val, uint32_t to);
  uint8_t readData(uint8_t address, uint8_t *message, uint32_t lenMessage, bool stopBit = false);
  uint8_t sendMessage(uint8_t address, uint8_t *message, uint32_t lenMessage, bool stopBit = false);
  uint8_t sendCommand(uint8_t address, uint8_t command, bool stopBit = false);
};

#endif  //KEYSCANNERFLASHER_H_
