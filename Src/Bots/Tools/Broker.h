
#pragma once

#include "Bot.h"
#include "DataProtocol.h"

class Broker : public Bot::Broker
{
public: 
  virtual void_t handleTrade(Bot::Session& session, const DataProtocol::Trade& trade) = 0;
};
