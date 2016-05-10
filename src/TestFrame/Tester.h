#ifndef test_assertions_h
#define test_assertions_h

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "Logger.h"

class Tester {
public:

  Tester();
  ~Tester();

  void countDown(int seconds);

  void beforeGroup(const char *groupName);
  void beforeTest(const char *testName);
  void afterTest();
  void afterGroup();

  void assertTrue(const char *msg, bool condition);
  void assertFalse(const char *msg, bool condition);

  template <typename T0, typename T1>
  void assertEquals(const char *msg, T0 expected, T1 actual);

  int getTestFailureCount();
  int getGroupFailureCount();

private:

  Logger *logger;

  const char *theTestName;
  const char *theGroupName;

  int testFailureCount;
  int groupFailureCount;
};

#include "TesterTemplates.h"

#endif