#include <Device/DeviceDriver.h>

//---------------------------------------------------------------------------

extern DeviceTable *gDeviceTable;

//---------------------------------------------------------------------------

DeviceDriver::DeviceDriver(const char *pre, const int count) :
  unitNamePrefix((char *)pre),
  logicalUnitCount(count),
  logicalUnits(new LogicalUnitInfo * [count]) {

  if (logicalUnits == 0) {  // out of memory
    logicalUnitCount = 0;
  }

  for (int idx = 0; idx < logicalUnitCount; idx++) {
    logicalUnits[idx] = 0;
  }
};

//---------------------------------------------------------------------------
/**
 * This DeviceDriver base class implementation of open() performs the initial
 * tasks of opening a logical unit, namely finding out if the call is targeting
 * a device that this instance is responsible for, and if it is, then checking
 * the associated lun and making sure that it is free.  The result of the work
 * by this base class open() is then returned for further processing or error
 * reporting to the calling subclass open() method.
 *
 * @param  opts  [description]
 * @param  flags [description]
 * @param  name  [description]
 * @return       [description]
 */
int DeviceDriver::open(int opts, int flags, const char *name) {
  int lun;

  int prefixLength = strcspn(name, ":");
  if ((strlen(unitNamePrefix) != prefixLength) || (strncmp(unitNamePrefix, name, prefixLength) != 0)) {
    return ENODEV;
  }

  lun = atoi(&name[prefixLength + 1]);
  if (lun < 0 || lun >= logicalUnitCount) {
    return ENXIO;
  }

  if (logicalUnits[lun] == 0) {
    return lun;
  }

  if (flags == (int)(DeviceConstants::DAF::FORCE)) {
    delete logicalUnits[lun];
    return lun;
  } else {
    return EADDRINUSE;
  }

  return ENXIO;
}

//---------------------------------------------------------------------------
/**
 * This DeviceDriver base class implementation of read() is called by the
 * subclass read() methods to handle the generic responses that all device
 * drivers implement the same way.  If this method returns ENOTSUP, then the
 * subclass read() should attempt to process the call itself.  If this method
 * returns any other value, positive or negative, then the subclass read()
 * should return that value to its caller with no further processing.
 *
 * @param  handle [description]
 * @param  flags  [description]
 * @param  reg    [description]
 * @param  count  [description]
 * @param  buf    [description]
 * @return        [description]
 */
int DeviceDriver::read(int handle, int flags, int reg, int count, byte *buf) {

   // First, handle registers that can be read even before a connection
   // has been made. Note that currently the only source of handle-less
   // operations is the Meta device driver.  It has access to DeviceTable
   // private values and thus can construct handles itself without having
   // to actually perform an open().  In this way it can query device drivers
   // without interfering with the activities of the clients that are following
   // the rules about open() and close().

  switch (reg) {
  case (int)(DeviceConstants::CDR::DriverVersion):
    return buildVersionResponse(count, buf);
  case (int)(DeviceConstants::CDR::UnitNamePrefix):
      return buildPrefixResponse(count,buf);
  }

  //  Second, registers for which we must have a handle (ie, open() has
  //  been done) but we don't need to have any detailed info about the
  //  unit itself beyond that contained in the LogicalUnitInfo base class.

  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LogicalUnitInfo *currentUnit = static_cast<LogicalUnitInfo *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;

  switch (reg) {
  case (int)(DeviceConstants::CDR::Intervals):
    if (count < 8) return EMSGSIZE;
    fromHostTo32LE(currentUnit->intervalTime[0], &buf[0]);
    fromHostTo32LE(currentUnit->intervalTime[1], &buf[4]);
    return 8;
  }

  return ENOTSUP;
}

//---------------------------------------------------------------------------
/**
 * This DeviceDriver base class implementation of write() is called by the
 * subclass write() methods to handle the generic responses that all device
 * drivers implement the same way.  If this method returns ENOTSUP, then the
 * subclass write() should attempt to process the call itself.  If this method
 * returns any other value, positive or negative, then the subclass write()
 * should return that value to its caller with no further processing.
 *
 * @param  handle [description]
 * @param  flags  [description]
 * @param  reg    [description]
 * @param  count  [description]
 * @param  buf    [description]
 * @return        [description]
 */
int DeviceDriver::write(int handle, int flags, int reg, int count, byte *buf) {

   // First, handle registers that can be written even before a connection
   // has been made. Note that currently the only source of handle-less
   // operations is the Meta device driver.  It has access to DeviceTable
   // private values and thus can construct handles itself without having
   // to actually perform an open().  In this way it can query device drivers
   // without interfering with the activities of the clients that are following
   // the rules about open() and close().

  // switch (reg) {
  //   ... No applicable registers ...
  // }

  //  Second, registers for which we must have a handle (ie, open() has
  //  been done) but we don't need to have any detailed info about the
  //  unit itself beyond that contained in the LogicalUnitInfo base class.

  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LogicalUnitInfo *currentUnit = static_cast<LogicalUnitInfo *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;

  switch (reg) {
  case (int)(DeviceConstants::CDR::Intervals):
    if (count < 8) return EMSGSIZE;
    currentUnit->intervalTime[0] = from32LEToHost(&buf[0]);
    currentUnit->intervalTime[1] = from32LEToHost(&buf[4]);
    return 8;
  }
  return ENOTSUP;
}

//---------------------------------------------------------------------------
/**
 * This DeviceDriver base class implementation of close() performs the final
 * tasks of closing a logical unit, namely releasing the memory that was
 * allocated for the logical unit info object by the subclass and zeroing out
 * the pointer to it to indicate that the lun is now available again.
 *
 * Prior to calling this close() method, the device driver subclass should do
 * any cleanup needed for the specific tasks that were done during the
 * associated open.
 *
 * Note that this method is the one that releases the memory, even though the
 * subclass open() is where it was allocated.
 *
 * @param  handle [description]
 * @param  flags  [description]
 * @return        [description]
 */
int DeviceDriver::close(int handle, int flags) {

  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LogicalUnitInfo *currentUnit = static_cast<LogicalUnitInfo *>(logicalUnits[lun]);

  if (currentUnit != 0) {
    delete currentUnit;
    logicalUnits[lun] = 0;
  }
  return ESUCCESS;
}

//---------------------------------------------------------------------------

// Any subclass of DeviceDriver that requires additional functionality in its
// processTimerEvent() or reset() methods should override these implementations
// appropriately.

int DeviceDriver::processTimerEvent(int lun, int timerSelector, ClientReporter * r) {
  return ESUCCESS;
}

void DeviceDriver::reset() {}

//---------------------------------------------------------------------------

int DeviceDriver::checkForTimerEvents(ClientReporter *r) {
  int status;
  int result = ESUCCESS;
  for (int lun = 0; lun < logicalUnitCount; lun++) {
    LogicalUnitInfo *currentUnit = logicalUnits[lun];
    if (currentUnit != 0) {
      for (int timer = 0; timer < 2; timer++) {
        if (currentUnit->intervalTime[timer] > 0) {
          currentUnit->deltaTime[timer] = calculateElapsedTime(currentUnit, timer);
          if (currentUnit->deltaTime[timer] >= currentUnit->intervalTime[timer]) {
            status = this->processTimerEvent(lun, timer, r);
            currentUnit->previousTime[timer] = currentUnit->currentTime[timer];
            result = (status == ESUCCESS) ? result : status;
          }
        }
      }
    }
  }
  return result;
}

//---------------------------------------------------------------------------

/**
 * This method returns two null-terminated strings in the given buffer.
 * The first string is the unit name associated with this device driver when
 * the constructor was originally called during compile and link.  The second
 * string is the name of the device driver, the same name as in the version
 * response, but without any of the release information.  Note that these are
 * two distinct null-terminated strings.  The second string starts after the
 * null that terminates the first string.
 *
 * @param  count The number of bytes in the provided buffer
 * @param  buf   The buffer in which to return the two strings
 * @return       Two null-terminated strings: the unit name root and the name
 * of the device driver
 */
int DeviceDriver::buildPrefixResponse(int count, byte *buf) {
  int scopeNameLength = strlen(scopeName);
  int prefixLength = strlen(unitNamePrefix);
  if (count < prefixLength + scopeNameLength + 2) return EMSGSIZE;
  strlcpy((char *)buf, unitNamePrefix,count);
  strlcpy((char *)buf+prefixLength+1, scopeName, count);
  return prefixLength + scopeNameLength + 2;
}

//---------------------------------------------------------------------------

int DeviceDriver::buildVersionResponse(int count, byte *buf) {

  int packetSize = releaseVersion[0];
  int nameLength = strlen(scopeName);
  int prLength = strlen(preReleaseLabel);
  int bLength = strlen(buildLabel);

  if (count < (1 + packetSize + nameLength + 1 + prLength + 1 + bLength + 1)) {
    return EMSGSIZE;
  }

  int byteIndex = 0;

  // name (including terminating null)

  for (int idx = 0; idx <= nameLength; idx++) {
    buf[byteIndex++] = scopeName[idx];
  }

  // version

  buf[byteIndex++] = packetSize;
  for (int idx = 0; idx < packetSize; idx++) {
    buf[byteIndex++] = releaseVersion[idx + 1];
  }

  // preReleaseLabel (including terminating null)

  for (int idx = 0; idx <= prLength; idx++) {
    buf[byteIndex++] = preReleaseLabel[idx];
  }

  // buildLabel (including terminating null)

  for (int idx = 0; idx <= bLength; idx++) {
    buf[byteIndex++] = buildLabel[idx];
  }

  return byteIndex;
}

//---------------------------------------------------------------------------

unsigned long DeviceDriver::calculateElapsedTime(LogicalUnitInfo *lui, int timerIndex) {
  unsigned long elapsedTime;

  lui->currentTime[timerIndex] = (timerIndex == 0) ? micros() : millis();

  if (lui->currentTime[timerIndex] >= lui->previousTime[timerIndex]) {
    elapsedTime = lui->currentTime[timerIndex] - lui->previousTime[timerIndex];
  } else {
    elapsedTime = (ULONG_MAX - lui->previousTime[timerIndex]) + (lui->currentTime[timerIndex] + 1);
  }
  return elapsedTime;
}

//---------------------------------------------------------------------------

int DeviceDriver::microRateRun(int action, int handle, int flags, int reg, int count, byte *buf) {
  return timerRateRun(0, action, handle, flags, reg, count, buf);
}

int DeviceDriver::milliRateRun(int action, int handle, int flags, int reg, int count, byte *buf) {
  return timerRateRun(1, action, handle, flags, reg, count, buf);
}

int DeviceDriver::microRateStop(int action, int handle, int flags, int reg, int count, byte *buf) {
  return timerRateStop(0, action, handle, flags, reg, count, buf);
}

int DeviceDriver::milliRateStop(int action, int handle, int flags, int reg, int count, byte *buf) {
  return timerRateStop(1, action, handle, flags, reg, count, buf);
}

int DeviceDriver::timerRateRun(int timerSelector,int action, int handle, int flags, int reg, int count, byte *buf) {
  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LogicalUnitInfo *currentUnit = logicalUnits[lun];
  if (currentUnit == 0) return ENOTCONN;

  if (timerSelector != 0 && timerSelector != 1) return EINVAL;
  if (flags != (int)(DeviceConstants::DAF::MILLI_RUN) && flags != (int)(DeviceConstants::DAF::MICRO_RUN)) return EINVAL;

  currentUnit->eventAction[timerSelector].action = action;
  currentUnit->eventAction[timerSelector].handle = handle;
  currentUnit->eventAction[timerSelector].flags = flags;
  currentUnit->eventAction[timerSelector].reg = reg;
  currentUnit->eventAction[timerSelector].count = count;
  currentUnit->eventAction[timerSelector].enabled = true;
  return ESUCCESS;
}

int DeviceDriver::timerRateStop(int timerSelector, int action, int handle, int flags, int reg, int count, byte *buf) {
  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LogicalUnitInfo *currentUnit = logicalUnits[lun];
  if (currentUnit == 0) return ENOTCONN;

  if (timerSelector != 0 && timerSelector != 1) return EINVAL;
  if (flags != (int)(DeviceConstants::DAF::MILLI_STOP) && flags != (int)(DeviceConstants::DAF::MICRO_STOP)) return EINVAL;

  currentUnit->eventAction[timerSelector].enabled = false;
  return ESUCCESS;
}
