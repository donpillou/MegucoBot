
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>

#include "DataProtocol.h"
#include "BotProtocol.h"

class Bot
{
public:
  enum Regressions
  {
    regression1m,
    regression3m,
    regression5m,
    regression10m,
    regression15m,
    regression20m,
    regression30m,
    regression1h,
    regression2h,
    regression4h,
    regression6h,
    regression12h,
    regression24h,
    numOfRegressions,
  };

  enum BellRegressions
  {
    bellRegression1m,
    bellRegression3m,
    bellRegression5m,
    bellRegression10m,
    bellRegression15m,
    bellRegression20m,
    bellRegression30m,
    bellRegression1h,
    bellRegression2h,
    numOfBellRegressions,
  };

  struct Values
  {
    struct RegressionLine
    {
      double price; // a
      double incline; // b
      double average;
    };
    RegressionLine regressions[(int)numOfRegressions];
    RegressionLine bellRegressions[(int)numOfBellRegressions];
  };

  class Broker
  {
  public:
    virtual ~Broker() {}
    virtual bool_t buy(double price, double amount, timestamp_t timeout) = 0;
    virtual bool_t sell(double price, double amount, timestamp_t timeout) = 0;
    virtual double getBalanceBase() const = 0;
    virtual double getBalanceComm() const = 0;
    virtual double getFee() const = 0;
    virtual size_t getOpenBuyOrderCount() const = 0;
    virtual size_t getOpenSellOrderCount() const = 0;
    virtual timestamp_t getTimeSinceLastBuy() const = 0;
    virtual timestamp_t getTimeSinceLastSell() const = 0;

    // todo: remove transaction stuff
    virtual void_t getTransactions(List<BotProtocol::Transaction>& transactions) const = 0;
    virtual void_t getBuyTransactions(List<BotProtocol::Transaction>& transactions) const = 0;
    virtual void_t getSellTransactions(List<BotProtocol::Transaction>& transactions) const = 0;
    virtual void_t removeTransaction(uint32_t id) = 0;
    virtual void_t updateTransaction(const BotProtocol::Transaction& transaction) = 0;

    virtual void_t getItems(List<BotProtocol::SessionItem>& items) const = 0;
    virtual void_t getBuyItems(List<BotProtocol::SessionItem>& items) const = 0;
    virtual void_t getSellItems(List<BotProtocol::SessionItem>& items) const = 0;
    virtual const BotProtocol::SessionItem* getItem(uint32_t id) const = 0;
    virtual bool_t createItem(BotProtocol::SessionItem& item) = 0;
    virtual void_t removeItem(uint32_t id) = 0;
    virtual void_t updateItem(const BotProtocol::SessionItem& item) = 0;

    virtual void_t warning(const String& message) = 0;
  };

  class Session
  {
  public:
    virtual ~Session() {};
    virtual void_t setParameters(double* parameters) = 0;
    virtual void_t handle(const DataProtocol::Trade& trade, const Values& values) = 0;
    virtual void_t handleBuy(const BotProtocol::Transaction& transaction) = 0;
    virtual void_t handleSell(const BotProtocol::Transaction& transaction) = 0;
  };
  
  virtual ~Bot() {}
  virtual Session* createSession(Broker& broker) = 0;
  virtual unsigned int getParameterCount() const = 0;
};
