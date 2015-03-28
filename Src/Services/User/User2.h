
#pragma once

#include <nstd/List.h>

#include "Tools/ZlimdbProtocol.h"

class Market2;

class User2
{
public:
  User2() {}
  ~User2();

  Market2* createMarket(const meguco_user_market_entity& marketEntity, const String& executable);

private:
  List<Market2*> markets;
};
