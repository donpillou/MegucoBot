
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>

class Bot
{
public:
  enum class Regressions
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

  enum class BellRegressions
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
    RegressionLine regressions[(int)Regressions::numOfRegressions];
    RegressionLine bellRegressions[(int)BellRegressions::numOfBellRegressions];
  };

  class Broker
  {
  public:
    class Transaction
    {
    public:
      enum class Type
      {
        buy,
        sell,
      };

      uint32_t id;
      timestamp_t date;
      double price;
      double amount;
      double fee;
      Type type;
    };

    virtual ~Broker() {}
    virtual bool_t buy(double price, double amount, timestamp_t timeout) = 0;
    virtual bool_t sell(double price, double amount, timestamp_t timeout) = 0;
    virtual double getBalanceBase() const = 0;
    virtual double getBalanceComm() const = 0;
    virtual double getFee() const = 0;
    virtual uint_t getOpenBuyOrderCount() const = 0;
    virtual uint_t getOpenSellOrderCount() const = 0;
    virtual timestamp_t getTimeSinceLastBuy() const = 0;
    virtual timestamp_t getTimeSinceLastSell() const = 0;

    virtual void_t getTransactions(List<Transaction>& transactions) const = 0;
    virtual void_t getBuyTransactions(List<Transaction>& transactions) const = 0;
    virtual void_t getSellTransactions(List<Transaction>& transactions) const = 0;
    virtual void_t removeTransaction(uint32_t id) = 0;
    virtual void_t updateTransaction(uint32_t id, const Transaction& transaction) = 0;

    virtual void_t warning(const String& message) = 0;
  };

  //class Session
  //{
  //public:
  //  virtual ~Session() {};
  //  virtual void setParameters(double* parameters) = 0;
  //  virtual void handle(const DataProtocol::Trade& trade, const Values& values) = 0;
  //  virtual void handleBuy(const Broker::Transaction& transaction) = 0;
  //  virtual void handleSell(const Broker::Transaction& transaction) = 0;
  //};
  //
  //virtual ~Bot() {}
  //virtual Session* createSession(Broker& broker) = 0;
  //virtual unsigned int getParameterCount() const = 0;
};
