#include "DDMeta.h"
#include <Device/DeviceTable.h>
#include <Device/ByteOrder.h>
#include <limits.h>

//---------------------------------------------------------------------------

extern DeviceTable *gDeviceTable;

/**
 * This DDMeta class is an administrative and development tool.  As a device
 * driver, it provides an API for:
 * <ul>
 * <li>access to the DeviceTable (how many devices, device driver versions, etc)</li>
 * <li>code analysis capabilities (memory usage, timing, etc)</li>
 * </ul>
 */
DDMeta::DDMeta(const char *dName, int count) :
  DeviceDriver(dName, count) {
  DEFINE_VERSION(0, 10, 0)
}

//---------------------------------------------------------------------------

int DDMeta::open(int opts, int flags, const char *name) {
  int lun;
  int status = DeviceDriver::open(opts, flags, name);
  if (status < 0) return status;

  lun = status;
  LUMeta *currentUnit = new LUMeta();
  if (currentUnit == 0) {
    return ENOMEM;
  }

  logicalUnits[lun] = currentUnit;
  return lun;
}

//---------------------------------------------------------------------------

int DDMeta::read(int handle, int flags, int reg, int count, byte *buf) {
  byte versionBuffer[DEVICE_RESPONSE_BUFFER_SIZE];

  // First, handle registers that can be processed by the DeviceDriver base
  // class without knowing very much about our particular device type.

  int status = DeviceDriver::read(handle, flags, reg, count, buf);
  if (status != ENOTSUP) {
    return status;
  }

  // Second, deal with "connection not required" requests.  (This DDMeta
  // driver is privileged and is the only driver with the ability to reach
  // into the global device table and formulate responses.)

  switch (reg) {

  case (int)(REG::DRIVER_COUNT):
    if (count < 2) return EMSGSIZE;
    fromHostTo16LE(gDeviceTable->deviceCount, buf);
    return 2;

  case (int)(REG::DRIVER_VERSION_LIST):
    for (int idx=0; idx<gDeviceTable->deviceCount; idx++) {
      status = gDeviceTable->read(makeDeviceHandle(idx,0), flags, (int)(DeviceConstants::CDR::DriverVersion), DEVICE_RESPONSE_BUFFER_SIZE, versionBuffer);
      gDeviceTable->cr->reportRead(status, handle, flags, (int)(DeviceConstants::CDR::DriverVersion), DEVICE_RESPONSE_BUFFER_SIZE, versionBuffer);
    }
    return ESUCCESS;

  case (int)(REG::UNIT_NAME_PREFIX_LIST):
    for (int idx=0; idx<gDeviceTable->deviceCount; idx++) {
      status = gDeviceTable->read(makeDeviceHandle(idx,0), flags, (int)(DeviceConstants::CDR::UnitNamePrefix), DEVICE_RESPONSE_BUFFER_SIZE, versionBuffer);
      gDeviceTable->cr->reportRead(status, handle, flags, (int)(DeviceConstants::CDR::UnitNamePrefix), DEVICE_RESPONSE_BUFFER_SIZE, versionBuffer);
    }
    return ESUCCESS;
}

  // Third, deal with connection-required requests

  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LUMeta *currentUnit = static_cast<LUMeta *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;

  // Take action regarding continuous read, if requested

  if (flags == (int)DeviceConstants::DAF::MILLI_RUN) {
    DeviceDriver::milliRateRun((int)DeviceConstants::DAC::READ, handle, flags, reg, count);
  } else if (flags == (int)DeviceConstants::DAF::MILLI_STOP) {
    DeviceDriver::milliRateStop((int)DeviceConstants::DAC::READ, handle, flags, reg, count);
  }

  switch (reg) {

  case (int)(REG::AVG_INTERVALS):
    return readATI(handle, flags, reg, count, buf);

  default:
    return ENOTSUP;
  }
}

int DDMeta::write(int handle, int flags, int reg, int count, byte *buf) {
  int unitNameLength;

  // First, handle registers that can be processed by the DeviceDriver base
  // class without knowing very much about our particular device type.

  int status = DeviceDriver::write(handle, flags, reg, count, buf);
  if (status != ENOTSUP) {
    return status;
  }

  //  Second, handle registers that can only be processed if an open has been
  //  performed and there is an LUMeta object associated with the lun.

  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LUMeta *currentUnit = static_cast<LUMeta *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;

  switch (reg) {

  default:
    return ENOTSUP;
  }
}

int DDMeta::close(int handle, int flags) {
  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LUMeta *currentUnit = static_cast<LUMeta *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;
  return DeviceDriver::close(handle, flags);
}

//---------------------------------------------------------------------------

// Collect duration samples.  The sample array is actually 0..SAMPLE_COUNT,
// and the useful samples are in 1..SAMPLE_COUNT.

int DDMeta::processTimerEvent(int lun, int timerSelector, ClientReporter *report) {

  LUMeta *cU = static_cast<LUMeta *>(logicalUnits[getUnitNumberFromHandle(lun)]);
  if (cU == 0) return ENOTCONN;

  switch (timerSelector) {

  case 0:       // microsecond timer
  case 1:       // millisecond timer

    cU->samples[timerSelector][cU->sampleIndex[timerSelector]] = cU->deltaTime[timerSelector];
    cU->isSampleBufferFull[timerSelector] |= (cU->sampleIndex[timerSelector] == SAMPLE_COUNT);
    cU->sampleIndex[timerSelector] = 1 + ((cU->sampleIndex[timerSelector]) % (int)(SAMPLE_COUNT));

    if (timerSelector == 1) {

      int h = cU->eventAction[1].handle;
      int f = cU->eventAction[1].flags;
      int r = cU->eventAction[1].reg;
      int c = min(cU->eventAction[1].count,DEVICE_RESPONSE_BUFFER_SIZE);

      if (cU->eventAction[1].enabled) {
        if ((cU->eventAction[1].action & 0xF) == (int)(DeviceConstants::DAC::READ))  {
          int status = gDeviceTable->read(h,f,r,c,cU->eventAction[1].responseBuffer);
          report->reportRead(status, h, f, r, c, (const byte *)(cU->eventAction[1].responseBuffer));
          return status;
        }
      }
    }
    return ESUCCESS;

  default:      // unrecognized timer index
    return ENOTSUP;

  }
}

//---------------------------------------------------------------------------

int DDMeta::readATI(int handle, int flags, int reg, int count, byte *buf) {
  unsigned long avg;
  LUMeta *cU = static_cast<LUMeta *>(logicalUnits[getUnitNumberFromHandle(handle)]);
  if (cU == 0) return ENOTCONN;

  if (count < 8) return EMSGSIZE;

  for (int timerIndex = 0; timerIndex < 2; timerIndex++) {
    unsigned long sum = 0;

    if ((cU->intervalTime[timerIndex] == 0) ||
      (!(cU->isSampleBufferFull[timerIndex]))) {
      return ENODATA;
    }
    for (int idx = 1; idx <= SAMPLE_COUNT; idx++) {
      sum += cU->samples[timerIndex][idx];
    }
    avg = sum / SAMPLE_COUNT;
    fromHostTo32LE(avg, buf + (4 * timerIndex));
  }
  return 8;
}
