#include "DDSignal.h"

//---------------------------------------------------------------------------

extern DeviceTable *gDeviceTable;

/**
 * This DDSignal device driver provides basic analog and digital I/O functions.
 *
 * Values are collected on a channel by channel basis.  A single logical unit
 * addresses a channel group, one or more channels pointing in the same
 * direction (INPUT or OUTPUT).
 */
DDSignal::DDSignal(const char *dName, int lunCount) :
  DeviceDriver(dName, lunCount) {
  DEFINE_VERSION(0, 10, 0)
}

//---------------------------------------------------------------------------

int DDSignal::open(int opts, int flags, const char *name) {
  int lun;
  int status = DeviceDriver::open(opts, flags, name);
  if (status < 0) {
    return status;
  }

  lun = getUnitNumberFromHandle(status);
  LUSignal *currentUnit = new LUSignal();
  if (currentUnit == 0) {
    return ENOMEM;
  }

  currentUnit->channelCount = 0;

  logicalUnits[lun] = currentUnit;
  return lun;
}

//---------------------------------------------------------------------------

int DDSignal::read(int handle, int flags, int reg, int count, byte *buf) {
  int bufIndex;

  if (count < 0) return EINVAL;

  // First, handle registers that can be processed by the DeviceDriver base
  // class without knowing very much about our particular device type.

  int status = DeviceDriver::read(handle, flags, reg, count, buf);
  if (status != ENOTSUP) {
    return status;
  }

  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LUSignal *currentUnit = static_cast<LUSignal *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;

  // Enable continuous read, if requested

  if (flags == (int)DeviceConstants::DAF::MILLI_RUN) {
    DeviceDriver::milliRateRun((int)DeviceConstants::DAC::READ, handle, flags, reg, count,buf);
  } else if (flags == (int)DeviceConstants::DAF::MILLI_STOP) {
    DeviceDriver::milliRateStop((int)DeviceConstants::DAC::READ, handle, flags, reg, count,buf);
  }

  //  Second, handle registers that can only be processed if an open has been
  //  performed and there is an LUSignal object associated with the lun.

  switch (reg) {

  case (int)(REG::CHANNEL_VALUES):
    if (currentUnit->channelCount == 0) return ENODATA;
    if (currentUnit->direction != INPUT) {
      return ENOTSUP;
    }
    if (count < 1+(2*currentUnit->channelCount)) return EMSGSIZE;

    bufIndex = 0;
    fromHostTo8LE(currentUnit->channelCount,buf + bufIndex++);
    for (int c=0; c < currentUnit->channelCount; c++) {
      if (currentUnit->channel[c].op == OP_DIGITAL) {
        if (!IS_PIN_DIGITAL(currentUnit->channel[c].digitalPin)) return EBADFD;
        fromHostTo16LE(digitalRead(PIN_TO_DIGITAL(currentUnit->channel[c].digitalPin)),buf + bufIndex);
      } else {
        if (!IS_PIN_ANALOG(currentUnit->channel[c].digitalPin)) return EBADFD;
        fromHostTo16LE(analogRead(PIN_TO_ANALOG(currentUnit->channel[c].digitalPin)),buf + bufIndex);
      }
      bufIndex += 2;
    }
    return bufIndex;
  }
  return ENOTSUP;
}

//---------------------------------------------------------------------------

int DDSignal::write(int handle, int flags, int reg, int count, byte *buf) {
  int numChannels;
  int bufIndex;
  bool isNewLock;
  int c;

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
  LUSignal *currentUnit = static_cast<LUSignal *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;

  // Enable continuous write, if requested

  if (flags == (int)DeviceConstants::DAF::MILLI_RUN) {
    DeviceDriver::milliRateRun((int)DeviceConstants::DAC::WRITE, handle, flags, reg, count,buf);
  } else if (flags == (int)DeviceConstants::DAF::MILLI_STOP) {
    DeviceDriver::milliRateStop((int)DeviceConstants::DAC::WRITE, handle, flags, reg, count,buf);
  }

  switch (reg) {

  case (int)(DeviceConstants::CDR::Configure):
    if (count < 5) return EMSGSIZE;
    bufIndex = 0;
    currentUnit->direction = from8LEToHost(buf+bufIndex++);
    currentUnit->channelCount = 0;
    numChannels = from8LEToHost(buf+bufIndex++);
    if (numChannels > MAX_CHANNELS_PER_UNIT) {
      return EINVAL;
    }
    for (c=0; c < numChannels; c++) {
      currentUnit->channel[c].op = from8LEToHost(buf+bufIndex++);
      currentUnit->channel[c].config = from8LEToHost(buf+bufIndex++);
      currentUnit->channel[c].digitalPin = from8LEToHost(buf+bufIndex++);
      isNewLock = currentUnit->lockPin(currentUnit->channel[c].digitalPin);
      if (!isNewLock) return EBUSY;
    }

    if (currentUnit->direction == INPUT) {
      for (c=0; c < numChannels; c++) {
        switch (currentUnit->channel[c].op) {
          case OP_DIGITAL:
            if (!IS_PIN_DIGITAL(currentUnit->channel[c].digitalPin)) return EINVAL;
            pinMode(currentUnit->channel[c].digitalPin,currentUnit->channel[c].config);
            break;
          case OP_ANALOG:
            if (!IS_PIN_ANALOG(currentUnit->channel[c].digitalPin)) return EINVAL;
            pinMode(currentUnit->channel[c].digitalPin,currentUnit->channel[c].config);
            break;
          default:
            return EBADR;
        }
      }
    } else if (currentUnit->direction == OUTPUT) {
      for (c=0; c < numChannels; c++) {
        switch (currentUnit->channel[c].op) {
          case OP_DIGITAL:
            if (!IS_PIN_DIGITAL(currentUnit->channel[c].digitalPin)) return EINVAL;
            pinMode(currentUnit->channel[c].digitalPin,OUTPUT);
            break;
          case OP_ANALOG:
            if (!IS_PIN_PWM(currentUnit->channel[c].digitalPin)) return EINVAL;
            pinMode(currentUnit->channel[c].digitalPin,OUTPUT);
            break;
          }
          default:
            return EBADR;
        }
      } else {
        return EINVAL;  // direction has to be INPUT or OUTPUT
      }
      currentUnit->channelCount = numChannels;
      return bufIndex;

    case (int)(REG::CHANNEL_VALUES):
      bufIndex = 0;
      if (currentUnit->channelCount == 0) return ENODATA;

      if (currentUnit->direction != OUTPUT) {
        return ENOTSUP;
      }

      if (count < 2*currentUnit->channelCount) return EMSGSIZE;
      numChannels = from8LEToHost(buf + bufIndex++);
      if (numChannels != currentUnit->channelCount) return EMSGSIZE;

      for (c=0; c < currentUnit->channelCount; c++) {
        if (currentUnit->channel[c].op = OP_DIGITAL) {
          digitalWrite(PIN_TO_DIGITAL(currentUnit->channel[c].digitalPin),from16LEToHost(buf + bufIndex));
        } else {
          analogWrite(PIN_TO_PWM(currentUnit->channel[c].digitalPin),from16LEToHost(buf + bufIndex));
        }
        bufIndex += 2;
      }
      return bufIndex;
  }
  return ENOTSUP;
}

//---------------------------------------------------------------------------

int DDSignal::close(int handle, int flags) {
  int lun = getUnitNumberFromHandle(handle);
  if (lun < 0 || lun >= logicalUnitCount) return EINVAL;
  LUSignal *currentUnit = static_cast<LUSignal *>(logicalUnits[lun]);
  if (currentUnit == 0) return ENOTCONN;

  if (currentUnit->channelCount > 0) {
    for (int c=0; c < currentUnit->channelCount; c++) {
      currentUnit->unlockPin(currentUnit->channel[c].digitalPin);
    }
  }
  return DeviceDriver::close(handle, flags);
}

//---------------------------------------------------------------------------

// If a continuous read has been requested, process it.

int DDSignal::processTimerEvent(int lun, int timerSelector, ClientReporter *report) {
  int status;

  LUSignal *currentUnit = static_cast<LUSignal *>(logicalUnits[getUnitNumberFromHandle(lun)]);
  if (currentUnit == 0) return ENOTCONN;

  int h = currentUnit->eventAction[1].handle;
  int f = currentUnit->eventAction[1].flags;
  int r = currentUnit->eventAction[1].reg;
  int c = min(currentUnit->eventAction[1].count,DEVICE_RESPONSE_BUFFER_SIZE);

  // Is it time to do another read?

  if ((timerSelector == 1) && (currentUnit->eventAction[1].enabled)) {
    if ((currentUnit->eventAction[1].action && 0xF) == (int)(DeviceConstants::DAC::READ))  {
        status = gDeviceTable->read(h, f, r, c, currentUnit->eventAction[1].responseBuffer);
        report->reportRead(status, h, f, r, c,  currentUnit->eventAction[1].responseBuffer);
        return status;
    }
  }
  return ESUCCESS;
}
