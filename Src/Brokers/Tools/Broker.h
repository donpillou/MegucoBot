
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>

#include <megucoprotocol.h>

class Broker
{
public:
  virtual ~Broker() {};

  virtual const String& getLastError() const = 0;

  virtual bool_t loadOrders(List<meguco_user_market_order_entity>& orders) = 0;
  virtual bool_t loadBalance(meguco_user_market_balance_entity& balance) = 0;
  virtual bool_t loadTransactions(List<meguco_user_market_transaction_entity>& transactions) = 0;
  virtual bool_t createOrder(uint64_t id, meguco_user_market_order_type type, double price, double amount, double total, meguco_user_market_order_entity& order) = 0;
  //virtual bool_t getOrder(uint64_t id, meguco_user_market_order_entity& order) = 0;
  virtual bool_t cancelOrder(uint64_t id) = 0;
};
