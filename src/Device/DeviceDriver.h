#ifndef DeviceDriver_h
#define DeviceDriver_h

/**
 * @defgroup DDImplementation Device Driver Implementations
 * @defgroup LuniFrame Luni Architecture Framework
 *
 */

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "ByteOrder.h"
#include "SemVer.h"
#include "Boards.h"
#include "LogicalUnitInfo.h"
#include "DeviceError.h"
#include "DeviceDefine.h"

class ClientReporter;
class DeviceTable;


/**
 * @ingroup LuniFrame, DDImplementation
 * This DeviceDriver class is the base class for all the device drivers.  It
 * implements common functions and provides a common API for all the drivers.
 */
 class DeviceDriver {

    friend class DeviceTable;       // in order to set deviceNumber

public:

    DeviceDriver(const char *nameRoot, const int count);

    virtual int open(int opts, int flags, const char *name) = 0;
    virtual int read(int handle, int flags, int reg, int count, byte *buf) = 0;
    virtual int write(int handle, int flags, int reg, int count, byte *buf) = 0;
    virtual int close(int handle, int flags) = 0;

    virtual int processTimerEvent(int lun, int timerSelector, ClientReporter *r);
    virtual void reset();

    virtual int checkForTimerEvents(ClientReporter *r) final;

protected:
    DECLARE_VERSION

    int deviceNumber;        // the major handle value, ie index in the DeviceTable
    char *unitNamePrefix;

    int logicalUnitCount;
    LogicalUnitInfo **logicalUnits;

    int buildVersionResponse(int count, byte *buf);
    int buildPrefixResponse(int count, byte *buf);
    int milliRateRun(int action, int handle, int flags, int reg, int count, byte *buf = 0);
    int milliRateStop(int action, int handle, int flags, int reg, int count, byte *buf = 0);
    int microRateRun(int action, int handle, int flags, int reg, int count, byte *buf = 0);
    int microRateStop(int action, int handle, int flags, int reg, int count, byte *buf = 0);

private:

    unsigned long calculateElapsedTime(LogicalUnitInfo *lui, int timerIndex);
    int timerRateRun(int timerSelector,int action, int handle, int flags, int reg, int count, byte *buf);
    int timerRateStop(int timerSelector, int action, int handle, int flags, int reg, int count, byte *buf);

};

#endif
