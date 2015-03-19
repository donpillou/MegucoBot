
#pragma once

#include "Tools/ZlimdbProtocol.h"

class ProcessManager;

class Market2
{
public:
  Market2(ProcessManager& processManager, const meguco_user_market_entity& marketEntity, const String& executable) : processManager(processManager), marketEntity(marketEntity), executable(executable) {}
  ~Market2() {}

  bool_t startProcess();

private:
  ProcessManager& processManager;
  meguco_user_market_entity marketEntity;
  String executable;
};
