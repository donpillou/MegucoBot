
#pragma once

#include "Bot.h"
#include "BotProtocol.h"
#include "DataProtocol.h"

class Broker : public Bot::Broker
{
public:
  virtual void_t loadTransaction(const BotProtocol::Transaction& transaction) = 0;
  virtual void_t loadItem(const BotProtocol::SessionItem& item) = 0;
  virtual void_t loadOrder(const BotProtocol::Order& order) = 0;
  virtual void_t handleTrade(const DataProtocol::Trade& trade) = 0;
  virtual void_t setBotSession(Bot::Session& session) = 0;
};

