#include "KeyScannerFlasher.h"

// Errors:
//  0 : Success
//  1 : Data too long
//  2 : NACK on transmit of address
//  3 : NACK on transmit of data
//  4 : Other error
uint8_t KeyScannerFlasher::sendCommand(uint8_t address, uint8_t command, bool stopBit) {
  watchdog_update();
  WIRE_.beginTransmission(address);
  WIRE_.write(command);
  return WIRE_.endTransmission(stopBit);
}

uint8_t KeyScannerFlasher::sendMessage(uint8_t address, uint8_t *message, uint32_t lenMessage, bool stopBit) {
  watchdog_update();
  WIRE_.beginTransmission(address);
  WIRE_.write(message, lenMessage);
  return WIRE_.endTransmission(stopBit);
}

uint8_t KeyScannerFlasher::readData(uint8_t address, uint8_t *message, uint32_t lenMessage, bool stopBit) {
  watchdog_update();
  WIRE_.requestFrom(address, lenMessage, false);
  return WIRE_.readBytes(message, lenMessage);
}

uint32_t KeyScannerFlasher::align(uint32_t val, uint32_t to) {
  uint32_t r = val % to;
  return r ? val + (to - r) : val;
}


void KeyScannerFlasher::setSide(Side side) {
  side_   = side;
  address = side ? left_boot_address_ : right_boot_address_;
}

bool KeyScannerFlasher::getInfoFlasherKS(InfoAction &info_action) {
  if (sendCommand(address, Action::INFO)) return false;
  readData(address, (uint8_t *)&info_action, sizeof(info_action));
  return true;
}

bool KeyScannerFlasher::sendEraseAction(EraseAction erase_action) {
  if (sendCommand(address, Action::ERASE)) return false;
  if (sendMessage(address, (uint8_t *)&erase_action, sizeof(erase_action))) return false;
  uint8_t done = false;
  readData(address, (uint8_t *)&done, sizeof(done));
  return done;
}

uint32_t KeyScannerFlasher::sendWriteAction(WriteAction write_action, uint8_t *data) {
  if (sendCommand(address, Action::WRITE)) {
    return 0;
  }
  if (sendMessage(address, (uint8_t *)&write_action, sizeof(write_action))) {
    return 0;
  }

  if (sendMessage(address, data, write_action.size)) {
    return 0;
  }

  uint32_t crc32 = 0;
  readData(address, (uint8_t *)&crc32, sizeof(crc32));
  return crc32;
}

bool KeyScannerFlasher::sendReadAction(ReadAction read_action) {
  if (sendCommand(address, Action::READ)) return false;
  if (sendMessage(address, (uint8_t *)&read_action, sizeof(read_action))) return false;
  return true;
}

uint16_t KeyScannerFlasher::readData(uint8_t *data, size_t size) {
  return readData(address, data, size);
}
bool KeyScannerFlasher::sendJump(uint32_t address_to_jump) {
  if (sendCommand(address, Action::JUMP_ADDRESS)) return false;
  if (sendMessage(address, (uint8_t *)&address_to_jump, sizeof(address_to_jump))) return false;
  return true;
}

bool KeyScannerFlasher::sendValidateProgram() {
  if (sendCommand(address, Action::VALIDATE_PROGRAM)) return false;
  uint8_t done = false;
  readData(address, (uint8_t *)&done, sizeof(done));
  return done;
}

bool KeyScannerFlasher::sendBegin() {
  WIRE_.setTimeout(50);
  bool b = !sendCommand(address, Action::BEGIN);
  WIRE_.setTimeout(300);
  return b;
}

bool KeyScannerFlasher::sendFinish() {
  if (sendCommand(address, Action::FINISH)) return false;
  return true;
}
void KeyScannerFlasher::setLeftBootAddress(uint8_t left_boot_address) {
  left_boot_address_ = left_boot_address;
}
void KeyScannerFlasher::setRightBootAddress(uint8_t right_boot_address) {
  right_boot_address_ = right_boot_address;
}