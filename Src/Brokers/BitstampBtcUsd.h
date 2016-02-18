
#pragma once

#include <nstd/Variant.h>

#include "Tools/Broker.h"
#include "Tools/HttpRequest.h"

class BitstampBtcUsd : public Broker
{
public:
  BitstampBtcUsd(const String& clientId, const String& key, const String& secret);

private:
  String clientId;
  String key;
  String secret;

  meguco_user_broker_balance_entity balance;
  bool_t balanceLoaded;
  HashMap<uint64_t, meguco_user_broker_order_entity> orders;

  String error;

  HttpRequest httpRequest;

  int64_t lastRequestTime;
  uint64_t lastNonce;
  int64_t lastLiveTradeUpdateTime;

  //HashMap<uint32_t, String> entityIds;
  //HashMap<String, uint32_t> entityIdsById;
  //uint32_t nextEntityId;

private:
  bool_t request(const String& url, bool_t isPublic, const HashMap<String, Variant>& params, Variant& result);

  void_t avoidSpamming();

  double getOrderCharge(double amount, double price) const;
  double getMaxSellAmout() const;
  double getMaxBuyAmout(double price) const;

  bool_t loadBalanceAndFee();

private: // Market
  virtual const String& getLastError() const {return error;}
  virtual bool_t loadOrders(List<meguco_user_broker_order_entity>& orders);
  virtual bool_t loadBalance(meguco_user_broker_balance_entity& balance);
  virtual bool_t loadTransactions(List<meguco_user_broker_transaction_entity>& transactions);
  virtual bool_t createOrder(meguco_user_broker_order_type type, double price, double amount, double total, meguco_user_broker_order_entity& order);
  //virtual bool_t getOrder(uint64_t id, meguco_user_market_order_entity& order);
  virtual bool_t cancelOrder(uint64_t rawId);
};
