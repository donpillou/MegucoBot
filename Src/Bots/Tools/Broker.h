
#pragma once

#include "Bot.h"

class Broker : public Bot::Broker
{
public:
  virtual void_t registerOrder(const Bot::Order& order) = 0;
  virtual void_t registerAsset(const Bot::Asset& asset) = 0;
  virtual void_t unregisterAsset(uint64_t id) = 0;
  virtual void_t registerProperty(const Bot::Property& property) = 0;

  virtual const Bot::Property* getProperty(uint64_t id) = 0;

  virtual const String& getLastError() const = 0;
  virtual void_t handleTrade(Bot::Session& session, const Bot::Trade& trade, bool_t replayed) = 0;
};
