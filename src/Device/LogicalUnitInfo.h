#ifndef LogicalUnitInfo_h
#define LogicalUnitInfo_h

#include <arduino.h>
#include "DeviceDefine.h"

class LogicalUnitInfo {

  friend class DeviceDriver;
  friend class DDMeta;              // in order to have access to the intervalTimes

public:
  LogicalUnitInfo();
  virtual ~LogicalUnitInfo();

/**
 * This RateAction struct describes the device driver call to make when one of
 * the two timers expires for this logical unit.  Timer[0] counts in microseconds,
 * timer[1] counts in milliseconds.
 */
  struct RateAction {
    bool enabled;           /**< If true, then this RateAction has a valid definition and can be run */
    int action;             /**< The device action code of the driver call to be made: READ or WRITE */
    int handle;             /**< The handle to use in making the call. */
    int flags;              /**< The value of the flags argument for the call */
    int reg;                /**< The device register number for the call */
    int count;              /**< The number of bytes to be read or written */
    byte queryBuffer[DEVICE_QUERY_BUFFER_SIZE];  /**< The outgoing (query) buffer */
    byte responseBuffer[DEVICE_RESPONSE_BUFFER_SIZE];  /**< The incoming (response) buffer */
  };


  unsigned long intervalTime[2];    /**< desired length of time between calls to processTimerEvent() */
  unsigned long previousTime[2];    /**< the time the last interval expired */
  unsigned long currentTime[2];     /**< the current values from micros() and millis() */
  unsigned long deltaTime[2];       /**< amount of time since last interval expired */
  RateAction eventAction[2];        /**< action to take when timer expires */

protected:

    bool lockPin(int p);
    bool unlockPin(int p);

};

#endif
