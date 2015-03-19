
#pragma once

#include <nstd/List.h>

#include "Tools/ZlimdbProtocol.h"

class Market2;
class ProcessManager;

class User2
{
public:
  User2() {}
  ~User2();

  Market2* createMarket(ProcessManager& processManager, const meguco_user_market_entity& marketEntity, const String& executable);

private:
  List<Market2*> markets;
};
