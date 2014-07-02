
#pragma once

#include "Bot.h"
#include "DataProtocol.h"

class Broker : public Bot::Broker
{
public: 
  virtual const String& getLastError() const = 0;
  virtual void_t handleTrade(Bot::Session& session, const DataProtocol::Trade& trade) = 0;
};
