
#pragma once

#include <nstd/Variant.h>

#include "Tools/Market.h"
#include "Tools/HttpRequest.h"

class BitstampMarket : public Market
{
public:
  BitstampMarket(const String& clientId, const String& key, const String& secret);

private:
  String clientId;
  String key;
  String secret;

  BotProtocol::MarketBalance balance;
  bool_t balanceLoaded;
  HashMap<uint32_t, BotProtocol::Order> orders;

  String error;

  HttpRequest httpRequest;

  timestamp_t lastRequestTime;
  uint64_t lastNonce;
  timestamp_t lastLiveTradeUpdateTime;

  HashMap<uint32_t, String> entityIds;
  HashMap<String, uint32_t> entityIdsById;
  uint32_t nextEntityId;

private:
  bool_t request(const String& url, bool_t isPublic, const HashMap<String, Variant>& params, Variant& result);

  void_t avoidSpamming();

  double getOrderCharge(double amount, double price) const;
  double getMaxSellAmout() const;
  double getMaxBuyAmout(double price) const;

  bool_t loadBalanceAndFee();

  uint32_t getEntityId(const String& id);
  void_t removeEntityId(uint32_t entityId);

private: // Market
  virtual const String& getLastError() const {return error;}
  virtual bool_t loadOrders(List<BotProtocol::Order>& orders);
  virtual bool_t loadBalance(BotProtocol::MarketBalance& balance);
  virtual bool_t loadTransactions(List<BotProtocol::Transaction>& transactions);
  virtual bool_t createOrder(BotProtocol::Order::Type type, double price, double amount, BotProtocol::Order& order);
  virtual bool_t cancelOrder(uint32_t id);
};
