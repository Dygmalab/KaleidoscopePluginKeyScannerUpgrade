/* -*- mode: c++ -*-
 * Upgrade - Flashing Keyscanner
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
#include "Upgrade.h"
#ifdef ARDUINO_ARCH_RP2040
#include "CRC_wrapper.h"
#elif defined(NRF52_ARCH)
#include "CRC_wrapper.h"
#endif

#include "kaleidoscope/plugin/FocusSerial.h"

#include "Communications.h"
#include "Watchdog_timer.h"
#include "Time_counter.h"

#include "kbd_if_manager.h"

#define ESC_APPROVE_TIMEOUT_MS              1000
#define SERIAL_FW_PACKET_WAIT_TIMEOUT_MS    5000

extern Watchdog_timer watchdog_timer;

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

result_t Upgrade::init()
{
    result_t result = RESULT_ERR;

    result = kbdif_initialize();
    EXIT_IF_ERR( result, "kbdif_initialize failed" );

    key_scanner_flasher_.setLeftBootAddress(kaleidoscope::Runtime.device().side.left_boot_address);
    key_scanner_flasher_.setRightBootAddress(kaleidoscope::Runtime.device().side.right_boot_address);

_EXIT:
    return result;
}

void Upgrade::resetSides() const {
    kaleidoscope::Runtime.device().side.prepareForFlash();
    kaleidoscope::Runtime.device().side.reset_sides();
}

bool Upgrade::escApprove() const {
    dl_timer_t timer;

    /* Set the timer */
    timer_set_ms( &timer, ESC_APPROVE_TIMEOUT_MS );

    /* We are waiting for any valid packect from the Left side of the keyboard. If received, we can be pretty sure the ESC key will work */
    while( timer_check(&timer) == false )
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

    dl_timer_t timer;

    if( data_len == 0 )
    {
        /* Nothing to be received */
        return true;
    }

    /* Set the timer */
    timer_set_ms( &timer, timeout_ms );

    while( timer_check(&timer) == false )
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

void Upgrade::run()
{
    if (flashing)
    {
        return;
    }

    if (!serial_pre_activation)
    {
        return;
    }

    if (!activated)
    {
        return;
    }

    if (kaleidoscope::Runtime.hasTimeExpired(pressed_time, press_time))
    {
      flashing = true;
      activated = false;

      return;
    }

    return;
}

result_t Upgrade::kbdif_initialize()
{
    result_t result = RESULT_ERR;
    kbdif_conf_t config;

    /* Prepare the kbdif configuration */
    config.p_instance = this;
    config.handlers = &kbdif_handlers;

    /* Initialize the kbdif */
    result = kbdif_init( &p_kbdif, &config );
    EXIT_IF_ERR( result, "kbdif_init failed" );

    /* Add the kbdif into the kbdif manager */
    result = kbdifmgr_add( p_kbdif );
    EXIT_IF_ERR( result, "kbdifmgr_add failed" );

_EXIT:
    return result;
}

kbdapi_event_result_t Upgrade::kbdif_key_event_process( kbdapi_key_t * p_key )
{
    if (!serial_pre_activation)
      return KBDAPI_EVENT_RESULT_IGNORED;

    if (!p_key->coord.is_valid || p_key->injected) {
      return KBDAPI_EVENT_RESULT_IGNORED;
    }

    if (p_key->coord.col == 0 && p_key->coord.row == 0 && p_key->toggled_on) {
      activated    = true;
      pressed_time = kaleidoscope::Runtime.millisAtCycleStart();
      return KBDAPI_EVENT_RESULT_CONSUMED;
    }

    return KBDAPI_EVENT_RESULT_IGNORED;
}

kbdapi_event_result_t Upgrade::kbdif_command_event_process( const char * p_command )
{
    if (::Focus.handleHelp(p_command,
                           PSTR(
                             "upgrade.start\n"
                             "upgrade.neuron\n"
                             "upgrade.end\n"
                             "upgrade.keyscanner.isConnected\n"   //Check if is connected (0 left 1 right)
                             "upgrade.keyscanner.isBootloader\n"  //Check if in bootloader mode (0 left 1 right)
                             "upgrade.keyscanner.begin\n"         //Choose the side (0 left 1 right)
                             "upgrade.keyscanner.isReady\n"       //Returns if the upgrade can begin successfully
                             "upgrade.keyscanner.getInfo\n"       //Version, and CRC, and is connected and start address, program is OK
                             "upgrade.keyscanner.getWriteSize\n"  //Get the maximum write action data block size
                             "upgrade.keyscanner.sendWrite\n"     //Write //{Address size DATA crc} Check if we are going to support --? true false
                             "upgrade.keyscanner.validate\n"      //Check validity
                             "upgrade.keyscanner.finish\n"        //Finish bootloader
                             "upgrade.keyscanner.sendStart")))    //Start main application and check validity //true false

      return KBDAPI_EVENT_RESULT_IGNORED;
    //TODO set numbers ot PSTR

    if (strncmp_P(p_command, PSTR("upgrade."), 8) != 0)
      return KBDAPI_EVENT_RESULT_IGNORED;

    if (strcmp_P(p_command + 8, PSTR("start")) == 0) {
      InfoAction infoLeft{};
      serial_pre_activation = true;

      kaleidoscope::Runtime.hid().keyboard().releaseAllKeys();
      kaleidoscope::Runtime.hid().keyboard().sendReport();

      resetSides();

      uint8_t i=0;
      flashing = false;
      right.connected = false;
      left.connected = false;
      left.validProgram = false;
      right.validProgram = false;
      while (!(right.connected && left.connected) && i < 3) {
        key_scanner_flasher_.setSide(KeyScannerFlasher::RIGHT);
        right.connected = key_scanner_flasher_.sendBegin();
        key_scanner_flasher_.setSide(KeyScannerFlasher::LEFT);
        left.connected = key_scanner_flasher_.sendBegin();
        i++;
      }

      if (right.connected) {
        key_scanner_flasher_.setSide(KeyScannerFlasher::RIGHT);
        right.validProgram = key_scanner_flasher_.sendValidateProgram();
      }

      if (left.connected) {
        key_scanner_flasher_.setSide(KeyScannerFlasher::LEFT);
        left.validProgram = key_scanner_flasher_.sendValidateProgram();
        key_scanner_flasher_.getInfoFlasherKS(infoLeft);

        //Check if the ESC key can be used. If not, left side program is assumed as invalid.
        if( escApprove() == false )
        {
          left.validProgram = false;
        }
      }

      //If the left keyboard is has not a valid program then we can continue
      if ( !left.validProgram ) {
        flashing = true;
      }

      if(left.validProgram && infoLeft.programVersion == 0x00) {
        flashing = true;
      }

      resetSides();

      return KBDAPI_EVENT_RESULT_CONSUMED;
    }

    if (strcmp_P(p_command + 8, PSTR("neuron")) == 0) {
      if (!flashing) return KBDAPI_EVENT_RESULT_ERROR;
      kaleidoscope::Runtime.rebootBootloader();
    }

    if (strcmp_P(p_command + 8, PSTR("isReady")) == 0) {
      ::Focus.send(flashing);
    }

    if (strcmp_P(p_command + 8, PSTR("end")) == 0) {
  /*    serial_pre_activation = false;
      activated             = false;
      flashing              = false;
      pressed_time          = 0;

      Communications.get_keyscanner_configuration(Devices::KEYSCANNER_DEFY_LEFT);
      Communications.get_keyscanner_configuration(Devices::KEYSCANNER_DEFY_RIGHT);*/
      resetSides();
    }

    if (strncmp_P(p_command + 8, PSTR("keyscanner."), 11) != 0)
      return KBDAPI_EVENT_RESULT_IGNORED;

    if (strcmp_P(p_command + 8 + 11, PSTR("isConnected")) == 0) {
      if (::Focus.isEOL()) return KBDAPI_EVENT_RESULT_CONSUMED;

      uint8_t side;
      ::Focus.read(side);
      if (side != KeyScannerFlasher::Side::RIGHT && side != KeyScannerFlasher::Side::LEFT) {
        return KBDAPI_EVENT_RESULT_CONSUMED;
      }

      if (side == KeyScannerFlasher::Side::RIGHT) {
        Focus.send(right.connected);
      }

      if (side == KeyScannerFlasher::Side::LEFT) {
        Focus.send(left.connected);
      }

      return KBDAPI_EVENT_RESULT_CONSUMED;
    }

    if (strcmp_P(p_command + 8 + 11, PSTR("isBootloader")) == 0) {
      if (::Focus.isEOL()) return KBDAPI_EVENT_RESULT_CONSUMED;
      uint8_t side;
      ::Focus.read(side);

      if (side != KeyScannerFlasher::Side::RIGHT && side != KeyScannerFlasher::Side::LEFT) {
        return KBDAPI_EVENT_RESULT_CONSUMED;
      }

      if (side == KeyScannerFlasher::Side::RIGHT) {
        Focus.send(!right.validProgram);
      }

      if (side == KeyScannerFlasher::Side::LEFT) {
        Focus.send(!left.validProgram);
      }

      return KBDAPI_EVENT_RESULT_CONSUMED;
    }

    if (strcmp_P(p_command + 8 + 11, PSTR("begin")) == 0) {
      if (!flashing) return KBDAPI_EVENT_RESULT_ERROR;

      if (::Focus.isEOL()) return KBDAPI_EVENT_RESULT_CONSUMED;

      uint8_t side;
      ::Focus.read(side);
      if (side != KeyScannerFlasher::Side::RIGHT && side != KeyScannerFlasher::Side::LEFT) {
        return KBDAPI_EVENT_RESULT_CONSUMED;
      }

      key_scanner_flasher_.setSide((KeyScannerFlasher::Side)side);
      resetSides();
      bool active_side = false;
      uint8_t i=0;
      while (!active_side && i < 3) {
        active_side = key_scanner_flasher_.sendBegin();
        i++;
      }

      if (!active_side) {
        Focus.send(false);
        return KBDAPI_EVENT_RESULT_ERROR;
      }

      /* Get the transfer information of the active keyscanner  */
      InfoAction infoAction{};
      if( !key_scanner_flasher_.getInfoFlasherKS(infoAction) )
      {
          Focus.send(false);
          return KBDAPI_EVENT_RESULT_ERROR;
      }

      key_scanner_flasher_.setSideInfo(infoAction);

      /* Set the maximum transfer size */
      buffer_write_action_size_max_set( infoAction.maxTransmissionLength );

      Focus.send(true);
      return KBDAPI_EVENT_RESULT_CONSUMED;
    }

    if (strcmp_P(p_command + 8 + 11, PSTR("getInfo")) == 0) {
      if (!flashing) return KBDAPI_EVENT_RESULT_ERROR;
      auto info_action = key_scanner_flasher_.getInfoAction();

      ReadAction read{info_action.validationSpaceStart, sizeof(Seal)};
      Seal seal{};
      key_scanner_flasher_.sendReadAction(read);
      if (key_scanner_flasher_.readData((uint8_t *)&seal, sizeof(Seal)) != sizeof(Seal)) {
        Focus.send(false);
        return KBDAPI_EVENT_RESULT_ERROR;
      }
      Focus.send(info_action.hardwareVersion);
      Focus.send(info_action.flashStart);
      Focus.send(seal.programVersion);
      Focus.send(seal.programCrc);
      Focus.send(true);
    }

    if (strcmp_P(p_command + 8 + 11, PSTR("getWriteSize")) == 0) {
      if (!flashing) return KBDAPI_EVENT_RESULT_ERROR;
      Focus.send(UPG_WRITE_ACTION_DATA_SIZE_MAX);
      Focus.send(true);
    }

    if (strcmp_P(p_command + 8 + 11, PSTR("sendWrite")) == 0) {

      if (!flashing) return KBDAPI_EVENT_RESULT_ERROR;
      write_action_packet_t packet;
      watchdog_update();

      if( write_action_packet_read( &packet ) == false )
      {
          ::Focus.send(false);
          return KBDAPI_EVENT_RESULT_ERROR;
      }

      uint32_t crc32InMemory = crc32(packet.data, packet.write_action.size);
      uint32_t crc32Transmission = write_action_packet_crc_get( &packet );
      if (crc32Transmission != crc32InMemory) {
        ::Focus.send(false);
        return KBDAPI_EVENT_RESULT_ERROR;
      }

      if( !write_action_packet_process(&packet))
      {
          ::Focus.send(false);
          return KBDAPI_EVENT_RESULT_ERROR;
      }

      ::Focus.send(true);
    }

    if (strcmp_P(p_command + 8 + 11, PSTR("validate")) == 0) {
      if (!key_scanner_flasher_.sendValidateProgram()) {
        Focus.send(false);
        return KBDAPI_EVENT_RESULT_ERROR;
      }
      Focus.send(true);
    }

    if (strcmp_P(p_command + 8 + 11, PSTR("finish")) == 0) {
      /* Semd the remaining data in the buffer */
      if( !buffer_write_to_keyscanner() )
      {
        ::Focus.send(false);
        return KBDAPI_EVENT_RESULT_ERROR;
      }
      if (!key_scanner_flasher_.sendFinish()) {
        Focus.send(false);
        return KBDAPI_EVENT_RESULT_ERROR;
      }
      Focus.send(true);
      delay(100);       /* Let keyscanner finish its processes */
      kaleidoscope::Runtime.device().side.reset_sides();
    }

    if (strcmp_P(p_command + 8 + 11, PSTR("sendStart")) == 0) {
      auto info_action = key_scanner_flasher_.getInfoAction();
      if (!key_scanner_flasher_.sendValidateProgram()) {
        Focus.send(false);
        return KBDAPI_EVENT_RESULT_ERROR;
      }

      if (!key_scanner_flasher_.sendJump(info_action.programSpaceStart)) {
        Focus.send(false);
        return KBDAPI_EVENT_RESULT_ERROR;
      }

      Focus.send(true);
    }

    return KBDAPI_EVENT_RESULT_CONSUMED;
}

/**************************************************/
/*             Write Action processing            */
/**************************************************/

bool Upgrade::write_action_packet_read( write_action_packet_t * p_packet )
{
    /* Read the Write Action */
    if( Upgrade::serialDataRead( (uint8_t *)&p_packet->write_action, sizeof( p_packet->write_action ), SERIAL_FW_PACKET_WAIT_TIMEOUT_MS ) == false )
    {
        return false;
    }

    /* Check the incoming data size */
    if( p_packet->write_action.size > sizeof(p_packet->data) )
    {
        return false;
    }

    /* Read the rest of the packet */
    if( Upgrade::serialDataRead( p_packet->data,  p_packet->write_action.size + sizeof(p_packet->crc32_placeholder), SERIAL_FW_PACKET_WAIT_TIMEOUT_MS ) == false )
    {
        return false;
    }

    return true;
}

uint32_t Upgrade::write_action_packet_crc_get( write_action_packet_t * p_packet )
{
    return *(uint32_t *)&p_packet->data[ p_packet->write_action.size ];
}

bool Upgrade::write_action_packet_process( write_action_packet_t * p_packet )
{
    /* Check if we should free the previously buffered data first */
    if( p_packet->write_action.size > buffer_freesize_get() ||                      /* The incoming data size is bigger than the available space in the buffer */
        p_packet->write_action.addr != buffer_flash_addr + buffer_loadsize_get() )  /* The new write_action is not consistent with the previously buffered data */
    {
        /* Make space in the buffer by processing the previously buffered data into keyscanner and free it for
         * the new incoming data */
        if( !buffer_write_to_keyscanner() )
        {
            return false;
        }
    }

    /* Add data into the output buffer */
    if( !buffer_data_add( p_packet->write_action.addr, p_packet->data, p_packet->write_action.size) )
    {
        return false;
    }

    /* Check if the buffer has been filled */
    if( buffer_freesize_get() != 0 )
    {
        /* Wait for more data to come */
        return true;
    }

    if( !buffer_write_to_keyscanner() )
    {
        return false;
    }

    return true;
}

bool Upgrade::write_action_send( uint32_t flash_addr, uint8_t * p_data, uint32_t data_size )
{
    WriteAction write_action;
    uint32_t crcDataCalculation;
    uint32_t crcKeyScannerCalculation;

    if( data_size == 0 )
    {
        /* Nothing to be sent */
        return true;
    }

    /* Calculate the CRC of the data being sent */
    crcDataCalculation = crc32( p_data, data_size );

    /* Prepare the Write action and send */
    write_action.addr = flash_addr;
    write_action.size = data_size;

    crcKeyScannerCalculation = key_scanner_flasher_.sendWriteAction(write_action, p_data);

    /* Compare the CRC and return */
    return ( crcDataCalculation == crcKeyScannerCalculation ) ? true : false;
}

/**************************************************/
/*                Buffer processing               */
/**************************************************/

void Upgrade::buffer_write_action_size_max_set( uint16_t write_action_size_max )
{
    buffer_write_action_size_max = ( write_action_size_max <= sizeof(buffer_data) ) ? write_action_size_max : sizeof(buffer_data);

    buffer_clear();
}

uint16_t Upgrade::buffer_loadsize_get( void )
{
    return buffer_pos;
}

uint16_t Upgrade::buffer_freesize_get( void )
{
    //return buffer_tx_size_max - buffer_pos;
    return sizeof(buffer_data) - buffer_pos;
}

bool Upgrade::buffer_data_add( uint32_t flash_addr, uint8_t * p_data, uint16_t data_len )
{
    if( data_len > buffer_freesize_get() )
    {
        ASSERT_DYGMA( false, "Upgrade: buffer overflow" );
        return false;
    }

    /* Check if we are starting new block and, eventually, save the target flash address */
    if( buffer_loadsize_get() == 0 )
    {
        buffer_flash_addr = flash_addr;
    }

    /* Add data into the buffer */
    memcpy( &buffer_data[buffer_pos], p_data, data_len );

    /* Adjust the buffer position */
    buffer_pos += data_len;

    return true;
}

void Upgrade::buffer_data_consume( void )
{
    buffer_flash_addr += buffer_loadsize_get();
    buffer_pos = 0;
}

void Upgrade::buffer_clear( void )
{
    buffer_flash_addr = 0;
    buffer_pos = 0;
}

bool Upgrade::buffer_write_to_keyscanner( void )
{
    uint32_t flash_addr;
    uint32_t data_pos;
    uint32_t data_size_remaining;
    uint32_t data_size;
    InfoAction info_action;

    if( buffer_loadsize_get() == 0 )
    {
        /* Nothing to be sent */
        return true;
    }

    /* Initialize the process variables */
    flash_addr = buffer_flash_addr;
    data_pos = 0;
    data_size_remaining = buffer_loadsize_get();

    info_action = key_scanner_flasher_.getInfoAction();

    while( data_size_remaining != 0 )
    {
        /* Check if we are entering new erasable block */
        if ( flash_addr % info_action.eraseAlignment == 0 )
        {
            /* Erase the block before we start writing into it */
            EraseAction erase_action{ flash_addr, info_action.eraseAlignment };
            if (!key_scanner_flasher_.sendEraseAction(erase_action))
            {
                return false;
            }
        }

        /* Get the amount of data to be sent in this step */
        data_size = ( buffer_write_action_size_max < data_size_remaining ) ? buffer_write_action_size_max : data_size_remaining;

        /* Write data into the keyscanner */
        if (!write_action_send( flash_addr, &buffer_data[data_pos], data_size))
        {
            return false;
        }

        /* Adjust the process variables */
        flash_addr += data_size;
        data_pos += data_size;
        data_size_remaining -= data_size;
    }

    /* Clear the buffer */
    buffer_data_consume();

    return true;
}

/**************************************************/
/*                KBDIF processing                */
/**************************************************/

kbdapi_event_result_t Upgrade::kbdif_key_event_cb( void * p_instance, kbdapi_key_t * p_key )
{
    Upgrade * p_Upgrade = ( Upgrade *)p_instance;

    return p_Upgrade->kbdif_key_event_process( p_key );
}

kbdapi_event_result_t Upgrade::kbdif_command_event_cb( void * p_instance, const char * p_command )
{
    Upgrade * p_Upgrade = ( Upgrade *)p_instance;

    return p_Upgrade->kbdif_command_event_process( p_command );
}

const kbdif_handlers_t Upgrade::kbdif_handlers =
{
    .key_event_cb = kbdif_key_event_cb,
    .command_event_cb = kbdif_command_event_cb,
};

class Upgrade Upgrade;

#endif
