

#include "Bot.h"

class SimBroker : public Bot::Broker
{
public:
  SimBroker(double balanceBase, double balanceComm, double fee) : balanceBase(balanceBase), balanceComm(balanceComm), fee(fee), 
    time(0), lastBuyTime(0), lastSellTime(0) {}

private:
  //class Order : public Bot::Market::Order
  //{
  //public:
  //  quint64 timeout;
  //  Order(const Market::Order& order, quint64 timeout) : Market::Order(order), timeout(timeout) {}
  //};

private:
  //QList<Order> openOrders;
  double balanceBase;
  double balanceComm;
  double fee;
  timestamp_t time;
  timestamp_t lastBuyTime;
  timestamp_t lastSellTime;
  //QHash<quint64, Transaction> transactions;
  //quint64 nextOrderId;
  //quint64 nextTransactionId;

private: // Bot::Broker
  virtual bool buy(double price, double amount, timestamp_t timeout);
  virtual bool sell(double price, double amount, timestamp_t timeout);
  virtual double getBalanceBase() const {return balanceBase;}
  virtual double getBalanceComm() const {return balanceComm;}
  virtual double getFee() const {return fee;}
  virtual unsigned int getOpenBuyOrderCount() const;
  virtual unsigned int getOpenSellOrderCount() const;
  virtual timestamp_t getTimeSinceLastBuy() const{return time - lastBuyTime;}
  virtual timestamp_t getTimeSinceLastSell() const {return time - lastSellTime;}

  virtual void getTransactions(List<Transaction>& transactions) const;
  virtual void getBuyTransactions(List<Transaction>& transactions) const;
  virtual void getSellTransactions(List<Transaction>& transactions) const;
  virtual void removeTransaction(uint64_t id);
  virtual void updateTransaction(uint64_t id, const Transaction& transaction);

  virtual void warning(const String& message);
};

