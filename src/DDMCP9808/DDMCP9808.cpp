#include "DDMCP9808.h"
#include <Wire.h>

//---------------------------------------------------------------------------

extern DeviceTable *gDeviceTable;

/**
 * This device driver is for the Microchip Technology MCP9808 Digital
 * Temperature Sensor.
 *
 * The device is controlled via I2C.  It expects that 16-bit register values
 * will be written in big-endian order, that is, the most significant byte
 * first, the least significant byte last.  However, that detail is handled
 * here in the driver - interaction with the caller is all handled in Little
 * Endian order as documented in the specification for this device driver.
 */
DDMCP9808::DDMCP9808(const char *dName, int lunCount, int baseI2CAddress) :
  DeviceDriver(dName, lunCount),
  baseAddress(baseI2CAddress),
  i2c() {
  DEFINE_VERSION(0, 10, 0)
}

//---------------------------------------------------------------------------

int DDMCP9808::open(int opts, int flags, const char *name) {
  int lun;
  int status = DeviceDriver::open(opts, flags, name);
  if (status < 0) {
    return status;
  }

  lun = status;
  LUMCP9808 *currentUnit = new LUMCP9808(baseAddress + lun);
  if (currentUnit == 0) {
    return ENOMEM;
  }

  int address = currentUnit->i2cAddress;
  int theRegister = (int)(REG::MANUF_ID);
  if (i2c.read16BE(address, theRegister) != 0x0054) {
    return ECONNREFUSED;
  }

  theRegister = (int)(REG::DEVICE_ID);
  if (i2c.read16BE(address, theRegister) != 0x0400) {
    return ECONNREFUSED;
  }

  logicalUnits[lun] = currentUnit;
  return lun;
}

//---------------------------------------------------------------------------

/**
 * Read a register on the device.
 */
int DDMCP9808::read(int handle, int flags, int reg, int count, byte *buf) {
  uint8_t v8;
  uint16_t v16;
  int v;

  if (count < 0) return EINVAL;

   // First, handle registers that can be processed by the DeviceDriver base
   // class without knowing very much about our particular device type.

  int status = DeviceDriver::read(handle, flags, reg, count, buf);
  if (status != ENOTSUP) {
    return status;
  }

  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LUMCP9808 *currentUnit = static_cast<LUMCP9808 *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;

  // Enable continuous read, if requested

  if (flags == (int)DeviceConstants::DAF::MILLI_RUN) {
    DeviceDriver::milliRateRun((int)DeviceConstants::DAC::READ, handle, flags, reg, count,buf);
  } else if (flags == (int)DeviceConstants::DAF::MILLI_STOP) {
    DeviceDriver::milliRateStop((int)DeviceConstants::DAC::READ, handle, flags, reg, count,buf);
  }

  //  Second, handle registers that can only be processed if an open has been
  //  performed and there is an LUSignal object associated with the lun.

  int address = currentUnit->i2cAddress;

  switch (reg) {

  case (int)(DeviceConstants::CDR::Stream):
    if (count < 2) {
      return EINVAL;
    }

    reg = (int)(REG::AMBIENT_TEMP);
    v = i2c.read16BE(address, reg);
    fromHostTo16LE(v, buf);
    return 2;

  case (int)(REG::CONFIG):
  case (int)(REG::UPPER_TEMP):
  case (int)(REG::LOWER_TEMP):
  case (int)(REG::CRIT_TEMP):
  case (int)(REG::AMBIENT_TEMP):
  case (int)(REG::MANUF_ID):
  case (int)(REG::DEVICE_ID):
    if (count < 2) return EMSGSIZE;
    v16 = i2c.read16BE(address, reg);
    fromHostTo16LE(v16, buf);
    return 2;

  case (int)(REG::RESOLUTION):
    if (count < 1) return EMSGSIZE;
    v8 = i2c.read8(address, reg);
    buf[0] = v8;
    return 1;

  default:
    return ENOTSUP;
  }
}

//---------------------------------------------------------------------------

int DDMCP9808::write(int handle, int flags, int reg, int count, byte *buf) {

   // First, handle registers that can be processed by the DeviceDriver base
   // class without knowing very much about our particular device type.

  int status = DeviceDriver::write(handle, flags, reg, count, buf);
  if (status != ENOTSUP) {
    return status;
  }

  //  Second, handle registers that can only be processed if an open has been
  //  performed and there is an LUSignal object associated with the lun.

  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LUMCP9808 *currentUnit = static_cast<LUMCP9808 *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;

  int theI2CAddress = currentUnit->i2cAddress;
  if (theI2CAddress < I2C_MIN_7BIT_ADDRESS || theI2CAddress > I2C_MAX_7BIT_ADDRESS) {
    return EBADFD;
  }

  switch (reg) {

  case (int)(REG::CONFIG):
  case (int)(REG::UPPER_TEMP):
  case (int)(REG::LOWER_TEMP):
  case (int)(REG::CRIT_TEMP):
    if (count == 2) {
      i2c.write16BE(theI2CAddress, reg, from16LEToHost(buf));
      return count;
    } else {
      return EINVAL;
    }

  case (int)(REG::RESOLUTION):
    if (count == 1) {
      i2c.write8BE(theI2CAddress, reg, from8LEToHost(buf));
      return count;
    } else {
      return EINVAL;
    }

  default:
    return ENOTSUP;
  }
  return EPANIC;
}

//---------------------------------------------------------------------------

int DDMCP9808::close(int handle, int flags) {
  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LUMCP9808 *currentUnit = static_cast<LUMCP9808 *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;
  return DeviceDriver::close(handle, flags);
}

//---------------------------------------------------------------------------

// If a continuous read has been requested, process it.

int DDMCP9808::processTimerEvent(int lun, int timerSelector, ClientReporter *report) {

  LogicalUnitInfo *cU = logicalUnits[getUnitNumberFromHandle(lun)];
  if (cU == 0) return ENOTCONN;

  int h = cU->eventAction[1].handle;
  int f = cU->eventAction[1].flags;
  int r = cU->eventAction[1].reg;
  int c = min(cU->eventAction[1].count,DEVICE_RESPONSE_BUFFER_SIZE);

  if (timerSelector == 1) {
    if (cU->eventAction[1].enabled) {
      if ((cU->eventAction[1].action & 0xF) == (int)(DeviceConstants::DAC::READ))  {
        int status = gDeviceTable->read(h,f,r,c,cU->eventAction[1].responseBuffer);
        report->reportRead(status, h, f, r, c, (const byte *)(cU->eventAction[1].responseBuffer));
        return status;
      }
    }
  }
  return ESUCCESS;
}

