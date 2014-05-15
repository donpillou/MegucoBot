
#pragma once

#include <nstd/HashMap.h>

#include "Broker.h"

class BotConnection;

class SimBroker : public Broker
{
public:
  SimBroker(BotConnection& botConnection, double balanceBase, double balanceComm, double fee);

private:
  //class Order : public Bot::Market::Order
  //{
  //public:
  //  quint64 timeout;
  //  Order(const Market::Order& order, quint64 timeout) : Market::Order(order), timeout(timeout) {}
  //};

private:
  BotConnection& botConnection;
  //QList<Order> openOrders;
  double balanceBase;
  double balanceComm;
  double fee;
  timestamp_t time;
  timestamp_t lastBuyTime;
  timestamp_t lastSellTime;
  HashMap<uint32_t, Transaction> transactions;
  //quint64 nextOrderId;
  //quint64 nextTransactionId;

private: // Bot::Broker
  virtual bool_t buy(double price, double amount, timestamp_t timeout);
  virtual bool_t sell(double price, double amount, timestamp_t timeout);
  virtual double getBalanceBase() const {return balanceBase;}
  virtual double getBalanceComm() const {return balanceComm;}
  virtual double getFee() const {return fee;}
  virtual uint_t getOpenBuyOrderCount() const;
  virtual uint_t getOpenSellOrderCount() const;
  virtual timestamp_t getTimeSinceLastBuy() const{return time - lastBuyTime;}
  virtual timestamp_t getTimeSinceLastSell() const {return time - lastSellTime;}

  virtual void_t getTransactions(List<Transaction>& transactions) const;
  virtual void_t getBuyTransactions(List<Transaction>& transactions) const;
  virtual void_t getSellTransactions(List<Transaction>& transactions) const;
  virtual void_t removeTransaction(uint32_t id);
  virtual void_t updateTransaction(uint32_t id, const Transaction& transaction);

  virtual void_t warning(const String& message);

public: // Broker
  virtual void_t loadTransaction(const BotProtocol::Transaction& transaction);
  virtual void_t loadOrder(const BotProtocol::Order& order);
  virtual bool_t handleTrade(const DataProtocol::Trade& trade);
};

