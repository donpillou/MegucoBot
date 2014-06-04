
#include <nstd/Debug.h>
#include <nstd/Time.h>
#include <nstd/Math.h>

#include "SimBroker.h"
#include "BotConnection.h"

SimBroker::SimBroker(BotConnection& botConnection, double balanceBase, double balanceComm, double fee) :
  botConnection(botConnection), balanceBase(balanceBase), balanceComm(balanceComm), fee(fee), 
  time(0), lastBuyTime(0), lastSellTime(0), botSession(0) {}

void_t SimBroker::loadTransaction(const BotProtocol::Transaction& transaction)
{
  transactions.append(transaction.entityId, transaction);
}

void_t SimBroker::loadOrder(const BotProtocol::Order& order)
{
  openOrders.append(order);
}

void_t SimBroker::handleTrade(const DataProtocol::Trade& trade)
{
  time = trade.time;

  for(List<BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const BotProtocol::Order& order = *i;
    if(time >= order.timeout)
    {
      if(order.type == BotProtocol::Order::buy)
        balanceBase += order.amount * order.price + order.fee;
      else
        balanceComm += order.amount;

      botConnection.removeSessionOrder(order.entityId);
      openOrders.remove(i);
      continue;
    }
    else if((order.type == BotProtocol::Order::buy && trade.price < order.price) ||
            ( order.type == BotProtocol::Order::sell && trade.price > order.price) )
    {
      BotProtocol::Transaction transaction;
      transaction.entityType = BotProtocol::sessionTransaction;
      transaction.type = order.type == BotProtocol::Order::buy ? BotProtocol::Transaction::buy : BotProtocol::Transaction::sell;
      transaction.date = Time::time();
      transaction.price = order.price;
      transaction.amount = order.amount;
      transaction.fee = order.fee;
      botConnection.createSessionTransaction(transaction);
      transactions.append(transaction.entityId, transaction);

      if(order.type == BotProtocol::Order::buy)
      {
        lastBuyTime = time;
        balanceComm += order.amount;
      }
      else
      {
        lastSellTime = time;
        balanceBase += order.amount * order.price - order.fee;
      }

      botConnection.removeSessionOrder(order.entityId);
      openOrders.remove(i);

      BotProtocol::Marker marker;
      marker.entityType = BotProtocol::sessionMarker;
      marker.date = time;
      if(order.type == BotProtocol::Order::buy)
      {
        marker.type = BotProtocol::Marker::buy;
        botSession->handleBuy(transaction);
      }
      else
      {
        marker.type = BotProtocol::Marker::sell;
        botSession->handleSell(transaction);
      }
      botConnection.createSessionMarker(marker);
    }
  }
}

void_t SimBroker::setBotSession(Bot::Session& session)
{
  botSession = &session;
}

bool_t SimBroker::buy(double price, double amount, timestamp_t timeout)
{
  double fee = Math::ceil(amount * price * this->fee * 100.) / 100.;
  double charge = amount * price + fee;
  if(charge > balanceBase)
    return false;

  BotProtocol::Order order;
  order.entityType = BotProtocol::sessionOrder;
  order.type = BotProtocol::Order::buy;
  order.date = time;
  order.amount = amount;
  order.price = price;
  order.fee = fee;
  timestamp_t orderTimeout = time + timeout;;
  order.timeout = orderTimeout;
  botConnection.createSessionOrder(order);
  ASSERT(order.timeout == orderTimeout);

  BotProtocol::Marker marker;
  marker.entityType = BotProtocol::sessionMarker;
  marker.date = time;
  marker.type = BotProtocol::Marker::buyAttempt;
  botConnection.createSessionMarker(marker);

  openOrders.append(order);
  balanceBase -= charge;
  return true;
}

bool_t SimBroker::sell(double price, double amount, timestamp_t timeout)
{
  if(amount > balanceComm)
    return false;

  BotProtocol::Order order;
  order.entityType = BotProtocol::sessionOrder;
  order.type = BotProtocol::Order::sell;
  order.date = time;
  order.amount = amount;
  order.price = price;
  order.fee = Math::ceil(amount * price * this->fee * 100.) / 100.;
  timestamp_t orderTimeout = time + timeout;
  order.timeout = orderTimeout;
  botConnection.createSessionOrder(order);
  ASSERT(order.timeout == orderTimeout);

  BotProtocol::Marker marker;
  marker.entityType = BotProtocol::sessionMarker;
  marker.date = time;
  marker.type = BotProtocol::Marker::sellAttempt;
  botConnection.createSessionMarker(marker);

  openOrders.append(order);
  balanceComm -= amount;
  return true;
}

uint_t SimBroker::getOpenBuyOrderCount() const
{
  size_t openBuyOrders = 0;
  for(List<BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const BotProtocol::Order& order = *i;
    if(order.type == BotProtocol::Order::buy)
      ++openBuyOrders;
  }
  return openBuyOrders;
}

uint_t SimBroker::getOpenSellOrderCount() const
{
  size_t openSellOrders = 0;
  for(List<BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const BotProtocol::Order& order = *i;
    if(order.type == BotProtocol::Order::sell)
      ++openSellOrders;
  }
  return openSellOrders;
}

void_t SimBroker::getTransactions(List<BotProtocol::Transaction>& transactions) const
{
  for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = this->transactions.begin(), end = this->transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    transactions.append(transaction);
  }
}

void_t SimBroker::getBuyTransactions(List<BotProtocol::Transaction>& transactions) const
{
  for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = this->transactions.begin(), end = this->transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    if(transaction.type == BotProtocol::Transaction::buy)
      transactions.append(transaction);
  }
}

void_t SimBroker::getSellTransactions(List<BotProtocol::Transaction>& transactions) const
{
  for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = this->transactions.begin(), end = this->transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    if(transaction.type == BotProtocol::Transaction::sell)
      transactions.append(transaction);
  }
}

void_t SimBroker::removeTransaction(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Transaction>::Iterator it = transactions.find(id);
  if(it == transactions.end())
    return;
  const BotProtocol::Transaction& transaction = *it;
  botConnection.removeSessionTransaction(transaction.entityId);
  transactions.remove(it);
}

void_t SimBroker::updateTransaction(const BotProtocol::Transaction& transaction)
{
  HashMap<uint32_t, BotProtocol::Transaction>::Iterator it = transactions.find(transaction.entityId);
  if(it == transactions.end())
    return;
  BotProtocol::Transaction& destTransaction = *it;
  destTransaction = transaction;
  destTransaction.entityType = BotProtocol::sessionTransaction;
  destTransaction.entityId = transaction.entityId;
  botConnection.updateSessionTransaction(destTransaction);
}

void_t SimBroker::warning(const String& message)
{
  botConnection.addLogMessage(message);
}
