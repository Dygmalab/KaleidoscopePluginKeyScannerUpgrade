/* -*- mode: c++ -*-
 * kaleidoscope::plugin::LEDCapsLockLight -- Highlight CapsLock when active
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

#include "Upgrade.h"
#include "CRC.h"
#include "kaleidoscope/plugin/FocusSerial.h"

namespace kaleidoscope {
namespace plugin {

EventHandlerResult Upgrade::onFocusEvent(const char *command) {
  if (::Focus.handleHelp(command,
                         PSTR(
                           "upgrade.start\n"
                           "upgrade.neuron\n"
                           "upgrade.end\n"
                           "upgrade.keyscanner.beginRight\n"  //Choose the side Right
                           "upgrade.keyscanner.beginLeft\n"   //Choose the side Left
                           "upgrade.keyscanner.getInfo\n"     //Version, and CRC, and is connected and start address, program is OK
                           "upgrade.keyscanner.sendWrite\n"   //Write //{Address size DATA crc} Check if we are going to support --? true false
                           "upgrade.keyscanner.validate\n"    //Check validity
                           "upgrade.keyscanner.finish\n"      //Finish bootloader
                           "upgrade.keyscanner.sendStart")))  //Start main application and check validy //true false

    return EventHandlerResult::OK;
  //TODO set numbers ot PSTR

  if (strncmp_P(command, PSTR("upgrade."), 8) != 0)
    return EventHandlerResult::OK;


  if (strcmp_P(command + 8, PSTR("start")) == 0) {
    serial_pre_activation = true;
    key_scanner_flasher_.setSide(KeyScannerFlasher::RIGHT);
    bool right_side = key_scanner_flasher_.sendBegin();
    if (right_side) {
      Kaleidoscope.hid().keyboard().pressKey(Key_Esc);
      activated = true;
      flashing  = true;
    }
    return EventHandlerResult::EVENT_CONSUMED;
  }

  if (strcmp_P(command + 8, PSTR("neuron")) == 0) {
    if (!flashing) return EventHandlerResult::ERROR;
    Runtime.rebootBootloader();
  }

  if (strcmp_P(command + 8, PSTR("isReady")) == 0) {
    ::Focus.send(flashing);
  }

  if (strcmp_P(command + 8, PSTR("end")) == 0) {
    serial_pre_activation = false;
    activated             = false;
    flashing              = false;
    pressed_time          = 0;
  }

  if (strncmp_P(command + 8, PSTR("keyscanner."), 11) != 0)
    return EventHandlerResult::OK;

  if (strcmp_P(command + 8 + 11, PSTR("beginRight")) == 0) {
    if (!flashing) return EventHandlerResult::ERROR;
    key_scanner_flasher_.setSide(KeyScannerFlasher::RIGHT);
    resetSides();
    bool right_side = key_scanner_flasher_.sendBegin();
    if (right_side) {
      Focus.send(true);
      return EventHandlerResult::EVENT_CONSUMED;
    }

    Focus.send(false);
    return EventHandlerResult::ERROR;
  }

  if (strcmp_P(command + 8 + 11, PSTR("beginLeft")) == 0) {
    if (!flashing) return EventHandlerResult::ERROR;
    key_scanner_flasher_.setSide(KeyScannerFlasher::LEFT);
    resetSides();
    bool right_side = key_scanner_flasher_.sendBegin();
    if (right_side) {
      Focus.send(true);
      return EventHandlerResult::EVENT_CONSUMED;
    }

    Focus.send(false);
    return EventHandlerResult::ERROR;
  }

  if (strcmp_P(command + 8 + 11, PSTR("getInfo")) == 0) {
    if (!flashing) return EventHandlerResult::ERROR;
    InfoAction info{};
    if (!key_scanner_flasher_.getInfoFlasherKS(info)) {
      Focus.send(false);
      return EventHandlerResult::ERROR;
    }
    key_scanner_flasher_.setSideInfo(info);

    Focus.send(info.hardwareVersion);
    Focus.send(info.flashStart);
    Focus.send(info.programVersion);
    Focus.send(info.bootloaderVersion);
    Focus.send(true);
  }

  if (strcmp_P(command + 8 + 11, PSTR("sendRead")) == 0) {
    if (!flashing) return EventHandlerResult::ERROR;
    Runtime.device().side.prepareForFlash();
    InfoAction info{};
    if (!key_scanner_flasher_.getInfoFlasherKS(info)) {
      Focus.send(false);
      return EventHandlerResult::ERROR;
    }
    key_scanner_flasher_.setSideInfo(info);

    ReadAction read{info.validationSpaceStart, sizeof(Seal)};
    Seal seal{};
    key_scanner_flasher_.sendReadAction(read);
    if (key_scanner_flasher_.readData((uint8_t *)&seal, sizeof(Seal)) != sizeof(Seal)) {
      Focus.send(false);
      return EventHandlerResult::ERROR;
    }
    Focus.send(info.hardwareVersion);
    Focus.send(info.flashStart);
    Focus.send(seal.programVersion);
    Focus.send(seal.programCrc);
    Focus.send(true);
  }

  if (strcmp_P(command + 8 + 11, PSTR("sendWrite")) == 0) {
    if (!flashing) return EventHandlerResult::ERROR;
    struct {
      WriteAction write_action;
      uint8_t data[256];
      uint32_t crc32Transmission;
    } packet;
    watchdog_update();
    Serial.readBytes((uint8_t *)&packet, sizeof(packet));
    watchdog_update();
    auto info_action = key_scanner_flasher_.getInfoAction();

    uint32_t crc32InMemory = crc32(packet.data, packet.write_action.size);
    if (packet.crc32Transmission != crc32InMemory) {
      ::Focus.send(false);
      return EventHandlerResult::ERROR;
    }

    if (packet.write_action.addr % info_action.eraseAlignment == 0) {
      EraseAction erase_action{packet.write_action.addr, info_action.eraseAlignment};
      if (!key_scanner_flasher_.sendEraseAction(erase_action)) {
        ::Focus.send(false);
        return EventHandlerResult::ERROR;
      }
    }
    uint32_t crcKeyScannerCalculation = key_scanner_flasher_.sendWriteAction(packet.write_action, packet.data);
    if (crcKeyScannerCalculation != packet.crc32Transmission) {
      ::Focus.send(false);
      return EventHandlerResult::ERROR;
    }
    ::Focus.send(true);
  }

  if (strcmp_P(command + 8 + 11, PSTR("validate")) == 0) {
    if (!key_scanner_flasher_.sendValidateProgram()) {
      Focus.send(false);
      return EventHandlerResult::ERROR;
    }
    Focus.send(true);
  }

  if (strcmp_P(command + 8 + 11, PSTR("finish")) == 0) {
    if (!key_scanner_flasher_.sendFinish()) {
      Focus.send(false);
      return EventHandlerResult::ERROR;
    }
    Focus.send(true);
  }

  if (strcmp_P(command + 8 + 11, PSTR("sendStart")) == 0) {
    auto info_action = key_scanner_flasher_.getInfoAction();
    if (!key_scanner_flasher_.sendValidateProgram()) {
      Focus.send(false);
      return EventHandlerResult::ERROR;
    }
    if (!key_scanner_flasher_.sendJump(info_action.programSpaceStart)) {
      Focus.send(false);
      return EventHandlerResult::ERROR;
    }
    Focus.send(true);
  }

  return EventHandlerResult::EVENT_CONSUMED;
}
void Upgrade::resetSides() const {
  Runtime.device().side.prepareForFlash();
  Runtime.device().side.reset();
}

EventHandlerResult Upgrade::onSetup() {
  key_scanner_flasher_.setLeftBootAddress(Runtime.device().side.left_boot_address);
  key_scanner_flasher_.setRightBootAddress(Runtime.device().side.right_boot_address);
  return EventHandlerResult::OK;
}
EventHandlerResult Upgrade::onKeyswitchEvent(Key &mapped_Key, KeyAddr key_addr, uint8_t key_state) {
  if (!serial_pre_activation)
    return EventHandlerResult::OK;

  if (!key_addr.isValid() || (key_state & INJECTED) != 0) {
    return EventHandlerResult::OK;
  }

  if (activated && key_addr.col() == 0 && key_addr.row() == 0 && keyToggledOff(key_state)) {
    activated    = false;
    pressed_time = 0;
    return EventHandlerResult::EVENT_CONSUMED;
  }

  if (key_addr.col() == 0 && key_addr.row() == 0 && keyToggledOn(key_state)) {
    activated    = true;
    pressed_time = Runtime.millisAtCycleStart();
    return EventHandlerResult::EVENT_CONSUMED;
  }

  return EventHandlerResult::OK;
}
EventHandlerResult Upgrade::beforeReportingState() {
  if (flashing) return EventHandlerResult::OK;
  if (!serial_pre_activation)
    return EventHandlerResult::OK;
  if (!activated)
    return EventHandlerResult::OK;
  if (Runtime.hasTimeExpired(pressed_time, press_time)) {
    flashing = true;
    return EventHandlerResult::OK;
  }
  return EventHandlerResult::OK;
}
}  // namespace plugin
}  // namespace kaleidoscope

kaleidoscope::plugin::Upgrade Upgrade;
