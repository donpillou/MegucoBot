
#pragma once

#include <nstd/HashMap.h>

#include "Broker.h"

class BotConnection;

class SimBroker : public Broker
{
public:
  SimBroker(BotConnection& botConnection, const BotProtocol::Balance& balance);

private:
  BotConnection& botConnection;
  List<BotProtocol::Order> openOrders;
  BotProtocol::Balance balance;
  timestamp_t time;
  timestamp_t lastBuyTime;
  timestamp_t lastSellTime;
  HashMap<uint32_t, BotProtocol::Transaction> transactions;
  HashMap<uint32_t, BotProtocol::SessionItem> items;
  Bot::Session* botSession;

private: // Bot::Broker
  virtual bool_t buy(double price, double amount, timestamp_t timeout);
  virtual bool_t sell(double price, double amount, timestamp_t timeout);
  virtual double getBalanceBase() const {return balance.availableUsd;}
  virtual double getBalanceComm() const {return balance.availableBtc;}
  virtual double getFee() const {return balance.fee;}
  virtual size_t getOpenBuyOrderCount() const;
  virtual size_t getOpenSellOrderCount() const;
  virtual timestamp_t getTimeSinceLastBuy() const{return time - lastBuyTime;}
  virtual timestamp_t getTimeSinceLastSell() const {return time - lastSellTime;}

  virtual void_t getTransactions(List<BotProtocol::Transaction>& transactions) const;
  virtual void_t getBuyTransactions(List<BotProtocol::Transaction>& transactions) const;
  virtual void_t getSellTransactions(List<BotProtocol::Transaction>& transactions) const;
  virtual void_t removeTransaction(uint32_t id);
  virtual void_t updateTransaction(const BotProtocol::Transaction& transaction);

  virtual void_t getItems(List<BotProtocol::SessionItem>& items) const;
  virtual void_t getBuyItems(List<BotProtocol::SessionItem>& items) const;
  virtual void_t getSellItems(List<BotProtocol::SessionItem>& items) const;
  virtual bool_t createItem(BotProtocol::SessionItem& item);
  virtual void_t removeItem(uint32_t id);
  virtual void_t updateItem(const BotProtocol::SessionItem& item);

  virtual void_t warning(const String& message);

public: // Broker
  virtual void_t loadTransaction(const BotProtocol::Transaction& transaction);
  virtual void_t loadItem(const BotProtocol::SessionItem& item);
  virtual void_t loadOrder(const BotProtocol::Order& order);
  virtual void_t handleTrade(const DataProtocol::Trade& trade);
  virtual void_t setBotSession(Bot::Session& session);
};

