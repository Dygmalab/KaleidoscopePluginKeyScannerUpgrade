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

#ifndef API_BOOTLOADER_KEYSCANNER_H_
#define API_BOOTLOADER_KEYSCANNER_H_

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


#endif  //API_BOOTLOADER_KEYSCANNER_H_
