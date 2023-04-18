#ifndef TEST_JIG
#include "Upgrade.h"
#ifdef ARDUINO_ARCH_RP2040
#include "CRC.h"
#elif defined(NRF52_ARCH)
#include "CRC_wrapper.h"
#endif
#include "kaleidoscope/plugin/FocusSerial.h"

namespace kaleidoscope {
namespace plugin {

EventHandlerResult Upgrade::onFocusEvent(const char *command) {
  if (::Focus.handleHelp(command,
                         PSTR(
                           "upgrade.start\n"
                           "upgrade.neuron\n"
                           "upgrade.end\n"
                           "upgrade.keyscanner.isConnected\n"   //Check if is connected (0 left 1 right)
                           "upgrade.keyscanner.isBootloader\n"  //Check if in bootloader mode (0 left 1 right)
                           "upgrade.keyscanner.begin\n"         //Choose the side (0 left 1 right)
                           "upgrade.keyscanner.getInfo\n"       //Version, and CRC, and is connected and start address, program is OK
                           "upgrade.keyscanner.sendWrite\n"     //Write //{Address size DATA crc} Check if we are going to support --? true false
                           "upgrade.keyscanner.validate\n"      //Check validity
                           "upgrade.keyscanner.finish\n"        //Finish bootloader
                           "upgrade.keyscanner.sendStart")))    //Start main application and check validy //true false

    return EventHandlerResult::OK;
  //TODO set numbers ot PSTR

  if (strncmp_P(command, PSTR("upgrade."), 8) != 0)
    return EventHandlerResult::OK;


  if (strcmp_P(command + 8, PSTR("start")) == 0) {
    serial_pre_activation = true;
    resetSides();
    key_scanner_flasher_.setSide(KeyScannerFlasher::RIGHT);
    right.connected = key_scanner_flasher_.sendBegin();
    key_scanner_flasher_.setSide(KeyScannerFlasher::LEFT);
    left.connected = key_scanner_flasher_.sendBegin();
    if (right.connected) {
      key_scanner_flasher_.setSide(KeyScannerFlasher::RIGHT);
      right.validProgram = key_scanner_flasher_.sendValidateProgram();
    }
    if (left.connected) {
      key_scanner_flasher_.setSide(KeyScannerFlasher::LEFT);
      left.validProgram = key_scanner_flasher_.sendValidateProgram();
    }
    //If the left keyboard is has not a valid program then we can continue
    if (!left.validProgram) {
      flashing = true;
    }
    resetSides();
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

  if (strcmp_P(command + 8 + 11, PSTR("isConnected")) == 0) {
    if (::Focus.isEOL()) return EventHandlerResult::EVENT_CONSUMED;
    uint8_t side;
    ::Focus.read(side);
    if (side != KeyScannerFlasher::Side::RIGHT && side != KeyScannerFlasher::Side::LEFT) {
      return EventHandlerResult::EVENT_CONSUMED;
    }
    if (side == KeyScannerFlasher::Side::RIGHT) {
      Focus.send(right.connected);
    }
    if (side == KeyScannerFlasher::Side::LEFT) {
      Focus.send(left.connected);
    }
    return EventHandlerResult::EVENT_CONSUMED;
  }

  if (strcmp_P(command + 8 + 11, PSTR("isBootloader")) == 0) {
    if (::Focus.isEOL()) return EventHandlerResult::EVENT_CONSUMED;
    uint8_t side;
    ::Focus.read(side);
    if (side != KeyScannerFlasher::Side::RIGHT && side != KeyScannerFlasher::Side::LEFT) {
      return EventHandlerResult::EVENT_CONSUMED;
    }
    if (side == KeyScannerFlasher::Side::RIGHT) {
      Focus.send(!right.validProgram);
    }
    if (side == KeyScannerFlasher::Side::LEFT) {
      Focus.send(!left.validProgram);
    }
    return EventHandlerResult::EVENT_CONSUMED;
  }

  if (strcmp_P(command + 8 + 11, PSTR("begin")) == 0) {
    if (!flashing) return EventHandlerResult::ERROR;
    if (::Focus.isEOL()) return EventHandlerResult::EVENT_CONSUMED;
    uint8_t side;
    ::Focus.read(side);
    if (side != KeyScannerFlasher::Side::RIGHT && side != KeyScannerFlasher::Side::LEFT) {
      return EventHandlerResult::EVENT_CONSUMED;
    }

    key_scanner_flasher_.setSide((KeyScannerFlasher::Side)side);
    resetSides();
    bool active_side = key_scanner_flasher_.sendBegin();
    if (active_side) {
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
    } packet{};
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
  Runtime.device().side.reset_sides();
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
#endif