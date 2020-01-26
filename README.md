## LuniLib

### An Arduino library to enable the creation and use of device driver modules with local and remote access.

#### Summary

LuniLib implements a framework for device drivers for use on the Arduino.

The traditional role of device drivers as an abstraction layer is a very useful one and can be applied in the Arduino environment to bundle device access into a logical, standard, easy to access API.  Each device driver includes methods for open, read, write, and close of **L**ogical **Uni**it instances that represent target device instances. The target device can be physical, virtual, or a combination of the two.

This library can be used standalone (running on an Arduino with no required interface to another system) or it can be used as a support library with Firmata.  In the latter case, a copy of ConfigurableFirmata with Device Driver Extensions must also be loaded.  There should be a copy of this required version of ConfigurableFirmata available from the same place you got this Luni library.

