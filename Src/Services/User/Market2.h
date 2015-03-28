
#pragma once

#include "Tools/ZlimdbProtocol.h"

class ProcessManager;

class Market2
{
public:
  Market2(const meguco_user_market_entity& marketEntity, const String& executable) : marketEntity(marketEntity), executable(executable), processId(0) {}
  ~Market2();

private:
  meguco_user_market_entity marketEntity;
  String executable;
  uint32_t processId;
};
