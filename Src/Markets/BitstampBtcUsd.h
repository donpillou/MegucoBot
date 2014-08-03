
#pragma once

#include <nstd/Variant.h>

#include "Tools/Market.h"
#include "Tools/HttpRequest.h"

class BitstampBtcUsd : public Market
{
public:
  BitstampBtcUsd(const String& clientId, const String& key, const String& secret);

private:
  String clientId;
  String key;
  String secret;

  BotProtocol::Balance balance;
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

  void_t setEntityId(const String& bitstampId, uint32_t entityId);
  uint32_t getNewEntityId(const String& bitstampId);
  void_t removeEntityId(uint32_t entityId);

private: // Market
  virtual const String& getLastError() const {return error;}
  virtual bool_t loadOrders(List<BotProtocol::Order>& orders);
  virtual bool_t loadBalance(BotProtocol::Balance& balance);
  virtual bool_t loadTransactions(List<BotProtocol::Transaction>& transactions);
  virtual bool_t createOrder(uint32_t id, BotProtocol::Order::Type type, double price, double amount, double total, BotProtocol::Order& order);
  virtual bool_t getOrder(uint32_t id, BotProtocol::Order& order);
  virtual bool_t cancelOrder(uint32_t id);
};
