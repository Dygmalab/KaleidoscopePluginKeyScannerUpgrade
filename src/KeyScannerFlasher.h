/* -*- mode: c++ -*-
 * kaleidoscope::plugin::KeyScannerFlasher
 * Copyright (C) 2020  Dygma Lab S.L.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEYSCANNERFLASHER_H_
#define KEYSCANNERFLASHER_H_

#include "stdio.h"
#include "ApiBootloaderKeyscanner.h"
#include "Arduino.h"
#include "Wire.h"

#define WIRE_ Wire1


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
