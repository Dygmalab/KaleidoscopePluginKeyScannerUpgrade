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
#pragma once

#include "KeyScannerFlasher.h"

#include "kbd_if.h"

#define UPG_BUFFER_SIZE     4096

class Upgrade {
 public:
  result_t init();
  void run();

 private:
  kbdif_t * p_kbdif = NULL;
  result_t kbdif_initialize(void);
  kbdapi_event_result_t kbdif_key_event_process( kbdapi_key_t * p_key );
  kbdapi_event_result_t kbdif_command_event_process( const char * p_command );

 private:
  KeyScannerFlasher key_scanner_flasher_{};
  struct {
    bool connected=false;
    bool validProgram=false;
  } right, left;
  bool activated = false;
  bool flashing  = false;
  uint16_t press_time{1};
  uint16_t pressed_time{0};
  bool serial_pre_activation = false;
  void resetSides() const;
  bool escApprove() const;
  bool serialDataRead( uint8_t * p_data, uint32_t data_len, uint32_t timeout_ms );

 private:
  uint8_t buffer_data[UPG_BUFFER_SIZE];
  uint16_t buffer_pos = 0;
  uint16_t buffer_tx_size_max = 256;
  uint32_t buffer_flash_addr;

  void buffer_tx_size_max_set( uint16_t tx_size_max );
  uint16_t buffer_loadsize_get( void );
  uint16_t buffer_freesize_get( void );
  bool buffer_data_add( uint32_t ks_flash_addr, uint8_t * p_data, uint16_t data_len );
  void buffer_clear( void );

  bool buffer_send_write_action( void );

 private:
  static const kbdif_handlers_t kbdif_handlers;

  static kbdapi_event_result_t kbdif_key_event_cb( void * p_instance, kbdapi_key_t * p_key );
  static kbdapi_event_result_t kbdif_command_event_cb( void * p_instance, const char * p_command );
};

extern class Upgrade Upgrade;
#endif
