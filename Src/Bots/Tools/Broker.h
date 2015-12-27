
#pragma once

#include "Bot.h"

class Broker : public Bot::Broker
{
public:
  virtual void_t registerTransaction(const meguco_user_broker_transaction_entity& transaction) = 0;
  virtual void_t registerOrder(const meguco_user_broker_order_entity& order) = 0;
  virtual void_t registerAsset(const meguco_user_session_asset_entity& asset) = 0;
  virtual void_t unregisterAsset(uint64_t id) = 0;
  virtual void_t registerProperty(const meguco_user_session_property_entity& property, const String& name, const String& value, const String& unit) = 0;

  virtual const meguco_user_session_property_entity* getProperty(uint64_t id, String& name, String& value, String& unit) = 0;

  virtual const String& getLastError() const = 0;
  virtual void_t handleTrade(Bot::Session& session, const meguco_trade_entity& trade, bool_t replayed) = 0;
};
