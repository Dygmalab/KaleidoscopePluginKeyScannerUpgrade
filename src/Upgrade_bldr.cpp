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

#ifndef TEST_JIG
#include "Upgrade_bldr.h"
#ifdef ARDUINO_ARCH_RP2040
#include "CRC_wrapper.h"
#elif defined(NRF52_ARCH)
#include "CRC_wrapper.h"
#include "nrf_gpio.h"
#include "common.h"
#endif

#include "kaleidoscope/plugin/FocusSerial.h"

#include "Communications.h"
#include "Watchdog_timer.h"

#define ESC_APPROVE_TIMEOUT_MS              1000
#define SERIAL_FW_PACKET_WAIT_TIMEOUT_MS    5000
#define CONNECTION_TIMEOUT_MS               3000
extern Watchdog_timer watchdog_timer;

uint64_t conn_timeout_timer = 0;


namespace kaleidoscope
{
namespace plugin {

    bool Upgrade::setup_right_connection()
    {
        conn_timeout_timer = millis();
        while (!right.connected)
        {
            Runtime.device().side.prepareForFlash();

            if (!right.connected) {
                for (uint8_t i = 0; i < 4; i++) {
                    key_scanner_flasher_.setSide(KeyScannerFlasher::RIGHT);
                    right.connected = key_scanner_flasher_.sendBegin();
                }
            }

            if (!right.connected)
            {
                Runtime.device().side.reset_right_side();
            }

            if (millis() - conn_timeout_timer > CONNECTION_TIMEOUT_MS &&
                !right.connected)
            {
                return false;
            }
        }

        return true;

    }

    bool Upgrade::setup_left_connection()
    {
        conn_timeout_timer = millis();
        while (!left.connected)
        {
            Runtime.device().side.prepareForFlash();
            if(!left.connected)
            {
                for (uint8_t i = 0 ; i <4; i++)
                {
                    key_scanner_flasher_.setSide(KeyScannerFlasher::LEFT);
                    left.connected = key_scanner_flasher_.sendBegin();
                }
            }

            if (!left.connected)
            {
                Runtime.device().side.reset_left_side();
            }

            if (millis() - conn_timeout_timer > CONNECTION_TIMEOUT_MS && !left.connected)
            {
                return false;
            }

        }

        return true;
    }


    /*
     *Bazecor steps in order.
     * upgrade.start
     * upgrade.keyscanner.isConnected (0:Right / 1:Left)
     * upgrade.keyscanner.isBootloader (0:Right / 1:Left)
     * upgrade.keyscanner.begin (0:Right / 1:Left) // after this one, FW remembers the chosen side
     * upgrade.keyscanner.getInfo
     * upgrade.keyscanner.sendWrite
     * upgrade.keyscanner.validate
     * upgrade.keyscanner.finish
     * upgrade.neuron
     * upgrade.end
     */

EventHandlerResult Upgrade::onFocusEvent(const char *command)
{
  if (::Focus.handleHelp(command,
                         PSTR(
                           "upgrade.start\n"
                           "upgrade.neuron\n"
                           "upgrade.end\n"
                           "upgrade.keyscanner.isConnected\n"   //Check if is connected (zero left 1 right)
                           "upgrade.keyscanner.isBootloader\n"  //Check if in bootloader mode (zero left 1 right)
                           "upgrade.keyscanner.begin\n"         //Choose the side (zero left 1 right)
                           "upgrade.keyscanner.isReady\n"       //Returns if the upgrade can begin successfully
                           "upgrade.keyscanner.getInfo\n"       //Version, and CRC, and is connected and start address, program is OK
                           "upgrade.keyscanner.sendWrite\n"     //Write //{Address size DATA crc} Check if we are going to support --? true false
                           "upgrade.keyscanner.validate\n"      //Check validity
                           "upgrade.keyscanner.finish\n"        //Finish bootloader
                           "upgrade.keyscanner.sendStart")))    //Start the main application and check valid //true false

    return EventHandlerResult::OK;
  //TODO set numbers ot PSTR

  if (strncmp_P(command, PSTR("upgrade."), 8) != 0)
    return EventHandlerResult::OK;

  if (strcmp_P(command + 8, PSTR("start")) == 0) {
    InfoAction infoLeft{};
    serial_pre_activation = true;

    Runtime.hid().keyboard().releaseAllKeys();
    Runtime.hid().keyboard().sendReport();

    resetSides();

    flashing = false;

    right.connected = false;
    left.connected = false;

    left.validProgram = false;
    right.validProgram = false;

    setup_right_connection();

    setup_left_connection();

    if (right.connected)
    {
      key_scanner_flasher_.setSide(KeyScannerFlasher::RIGHT);
      right.validProgram = key_scanner_flasher_.sendValidateProgram();
    }

    if (left.connected)
    {
      key_scanner_flasher_.setSide(KeyScannerFlasher::LEFT);
      left.validProgram = key_scanner_flasher_.sendValidateProgram();

      key_scanner_flasher_.getInfoFlasherKS(infoLeft);

      //Check if the ESC key can be used. If not, left side program is assumed as invalid.
      left.validProgram = false;

    }

    //If the left keyboard is has not a valid program then we can continue
    if ( !left.validProgram )
    {
      flashing = true;
    }

    if(left.validProgram && infoLeft.programVersion == 0x00)
    {
      flashing = true;
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
/*    serial_pre_activation = false;
    activated             = false;
    flashing              = false;
    pressed_time          = 0;

    Communications.get_keyscanner_configuration(Devices::KEYSCANNER_DEFY_LEFT);
    Communications.get_keyscanner_configuration(Devices::KEYSCANNER_DEFY_RIGHT);*/
    resetSides();
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

  if (strcmp_P(command + 8 + 11, PSTR("begin")) == 0)
  {
    if (!flashing) return EventHandlerResult::ERROR;

    if (::Focus.isEOL()) return EventHandlerResult::EVENT_CONSUMED;

    uint8_t side;
    ::Focus.read(side);
    if (side != KeyScannerFlasher::Side::RIGHT && side != KeyScannerFlasher::Side::LEFT) {
      return EventHandlerResult::EVENT_CONSUMED;
    }

    key_scanner_flasher_.setSide((KeyScannerFlasher::Side)side);

    bool active_side = false;
    uint8_t i=0;

    if(side == KeyScannerFlasher::Side::RIGHT)
    {
        if (setup_right_connection() == false)
        {
            Focus.send(false);
            return EventHandlerResult::ERROR;
        }
        active_side = true;
    }

    if ( side == KeyScannerFlasher::Side::LEFT )
    {
        if (setup_left_connection() == false)
        {
            Focus.send(false);
            return EventHandlerResult::ERROR;
        }
        active_side = true;
    }

    if (active_side)
    {
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

    if( serialDataRead( (uint8_t *)&packet, sizeof( packet ), SERIAL_FW_PACKET_WAIT_TIMEOUT_MS ) == false )
    {
        ::Focus.send(false);
        return EventHandlerResult::ERROR;
    }

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
   //Runtime.device().side.reset_sides();
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

bool Upgrade::escApprove() const {

    uint32_t process_start_timestamp = millis();

    /* We are waiting for any valid packect from the Left side of the keyboard. If received, we can be pretty sure the ESC key will work */
    while( ( millis() - process_start_timestamp ) < ESC_APPROVE_TIMEOUT_MS )
    {
        Communications.run();
        watchdog_timer.reset();

        if( Communications.isWiredLeftAlive() == true )
        {
            return true;
        }
    }

    return false;
}

bool Upgrade::serialDataRead( uint8_t * p_data, uint32_t data_len, uint32_t timeout_ms )
{
    size_t read_len = 0;
    size_t bytes_cnt;

    uint32_t process_start_timestamp = millis();

    if( data_len == 0 )
    {
        /* Nothing to be received */
        return true;
    }

    while( ( millis() - process_start_timestamp ) < timeout_ms )
    {
        watchdog_update();

        /* Wait for available data in Serial */
        if( Serial.available() == 0 )
        {
            continue;
        }

        bytes_cnt = Serial.readBytes( p_data, data_len - read_len );

        p_data += bytes_cnt;
        read_len += bytes_cnt;

        /* Finish here if all data has been read */
        if( read_len == data_len )
        {
            return true;
        }
    }

    return false;
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

  if (key_addr.col() == 0 && key_addr.row() == 0 && keyToggledOn(key_state)) {
    activated    = true;
    pressed_time = Runtime.millisAtCycleStart();
    return EventHandlerResult::EVENT_CONSUMED;
  }

  return EventHandlerResult::OK;
}

EventHandlerResult Upgrade::beforeReportingState()
{
  if (flashing)
  {
      return EventHandlerResult::OK;
  }

  if (!serial_pre_activation)
  {
      return EventHandlerResult::OK;
  }

  if (!activated)
  {
      return EventHandlerResult::OK;
  }

  if (Runtime.hasTimeExpired(pressed_time, press_time))
  {
    flashing = true;
    activated = false;

    return EventHandlerResult::OK;
  }

  return EventHandlerResult::OK;
}

}  // namespace plugin
}  // namespace kaleidoscope


kaleidoscope::plugin::Upgrade Upgrade;


#endif
