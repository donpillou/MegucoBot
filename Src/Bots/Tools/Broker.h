
#pragma once

#include "Bot.h"

class Broker : public Bot::Broker
{
public:
  virtual void_t registerTransaction2(const Bot::Transaction& transaction) = 0;
  virtual void_t registerOrder2(const Bot::Order& order) = 0;
  virtual void_t registerAsset2(const Bot::Asset& asset) = 0;
  virtual void_t unregisterAsset(uint64_t id) = 0;
  virtual void_t registerProperty2(const Bot::Property& property) = 0;

  virtual const Bot::Property* getProperty(uint64_t id) = 0;

  virtual const String& getLastError() const = 0;
  virtual void_t handleTrade2(Bot::Session& session, const Bot::Trade& trade, bool_t replayed) = 0;
};
