
#pragma once

#include "Bot.h"

class Broker : public Bot::Broker
{
public:
  //virtual void_t addTransaction(const meguco_user_market_transaction_entity& transaction) = 0;
  //virtual void_t addOrder(const meguco_user_market_order_entity& order) = 0;
  //virtual void_t addAsset(const meguco_user_session_asset_entity& asset) = 0;
  //virtual void_t addProperty(const meguco_user_session_property_entity& property, const String& name, const String& value) = 0;

  virtual const String& getLastError() const = 0;
  virtual void_t handleTrade(Bot::Session& session, const meguco_trade_entity& trade, bool_t replayed) = 0;
};
