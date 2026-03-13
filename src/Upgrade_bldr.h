/* -*- mode: c++ -*-
 * Upgrade_bldr - Flashing KS Bootloader
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
  bool setup_right_connection();
  bool setup_left_connection();
  void resetSides() const;
  bool escApprove() const;
  bool serialDataRead( uint8_t * p_data, uint32_t data_len, uint32_t timeout_ms );

 private:
  static const kbdif_handlers_t kbdif_handlers;

  static kbdapi_event_result_t kbdif_key_event_cb( void * p_instance, kbdapi_key_t * p_key );
  static kbdapi_event_result_t kbdif_command_event_cb( void * p_instance, const char * p_command );
};

extern class Upgrade Upgrade;
#endif
