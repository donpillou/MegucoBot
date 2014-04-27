
#pragma once

#include <nstd/Variant.h>

#include "Tools/Market.h"
#include "Tools/HttpRequest.h"

class BitstampMarket : public Market
{
public:
  BitstampMarket(const String& userName, const String& key, const String& secret);

private:
  String userName;
  String key;
  String secret;

  BotProtocol::MarketBalance balance;
  bool balanceLoaded;
  HashMap<String, BotProtocol::Order> orders;

  String error;

  HttpRequest httpRequest;

  timestamp_t lastRequestTime;
  uint64_t lastNonce;
  timestamp_t lastLiveTradeUpdateTime;

  bool request(const char_t* url, bool_t isPublic, const HashMap<String, Variant>& params, Variant& result);

  void avoidSpamming();

  double getOrderCharge(double amount, double price) const;
  double getMaxSellAmout() const;
  double getMaxBuyAmout(double price) const;

  bool loadBalanceAndFee();

private: // Market
  virtual const String& getLastError() const;
  virtual bool_t loadOrders(List<BotProtocol::Order>& orders);
  virtual bool_t loadBalance(BotProtocol::MarketBalance& balance);
  virtual bool_t loadTransactions(List<BotProtocol::Transaction>& transactions);
  virtual bool_t createOrder(double amount, double price, BotProtocol::Order& order);
  virtual bool_t cancelOrder(const String& id);
};
