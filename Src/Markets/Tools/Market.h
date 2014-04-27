
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>

#include "BotProtocol.h"

class Market
{
public:
  virtual ~Market() {};

  virtual const String& getLastError() const = 0;

  virtual bool_t loadOrders(List<BotProtocol::Order>& orders) = 0;
  virtual bool_t loadBalance(BotProtocol::MarketBalance& balance) = 0;
  virtual bool_t loadTransactions(List<BotProtocol::Transaction>& transactions) = 0;
  virtual bool_t createOrder(double amount, double price, BotProtocol::Order& order) = 0;
  virtual bool_t cancelOrder(uint32_t id) = 0;
};
