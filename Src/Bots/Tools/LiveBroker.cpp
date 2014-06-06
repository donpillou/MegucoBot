
#include <nstd/Time.h>
#include <nstd/Math.h>
#include <nstd/Map.h>

#include "LiveBroker.h"
#include "BotConnection.h"

LiveBroker::LiveBroker(BotConnection& botConnection, const BotProtocol::Balance& balance) :
  botConnection(botConnection), balance(balance),
  time(0), lastBuyTime(0), lastSellTime(0), botSession(0) {}

void_t LiveBroker::loadTransaction(const BotProtocol::Transaction& transaction)
{
  transactions.append(transaction.entityId, transaction);
}

void_t LiveBroker::loadOrder(const BotProtocol::Order& order)
{
  openOrders.append(order);
}

void_t LiveBroker::handleTrade(const DataProtocol::Trade& trade)
{
  time = trade.time;

  for(List<BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const BotProtocol::Order& order = *i;
    if (Math::abs(order.price - trade.price) <= 0.01)
    {
      refreshOrders();
      break;
    }
  }

  cancelTimedOutOrders();
}

void_t LiveBroker::refreshOrders()
{
  List<BotProtocol::Order> orders;
  if(!botConnection.getMarketOrders(orders))
    return;
  Map<uint32_t, const BotProtocol::Order*> openOrderIds;
  for(List<BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
  {
    const BotProtocol::Order& order = *i;
    openOrderIds.insert(order.entityId, &order);
  }
  for(List<BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const BotProtocol::Order& order = *i;
    if(!openOrderIds.contains(order.entityId))
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
        balance.reservedUsd -= order.amount * order.price + order.fee;
        balance.availableBtc += order.amount;
      }
      else
      {
        lastSellTime = time;
        balance.reservedBtc -= order.amount;
        balance.availableUsd += order.amount * order.price - order.fee;
      }
      botConnection.removeSessionOrder(order.entityId);
      openOrders.remove(i);
      botConnection.updateSessionBalance(balance);

      BotProtocol::Marker marker;
      marker.entityType = BotProtocol::sessionMarker;
      marker.date = time;
      if(order.amount >= 0.)
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

void_t LiveBroker::cancelTimedOutOrders()
{
  for(List<BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const BotProtocol::Order& order = *i;
    if(time >= order.timeout)
    {
      if(botConnection.removeMarketOrder(order.entityId))
      {
        if(order.type == BotProtocol::Order::buy)
        {
          double charge = order.amount * order.price + order.fee;
          balance.availableUsd += charge;
          balance.reservedUsd -= charge;
        }
        else
        {
          balance.availableBtc += order.amount;
          balance.reservedBtc -= order.amount;
        }

        botConnection.removeSessionOrder(order.entityId);
        openOrders.remove(i);
        botConnection.updateSessionBalance(balance);
        continue;
      }
      else
      {
        refreshOrders();
        return;
      }
    }
  }
}

void_t LiveBroker::setBotSession(Bot::Session& session)
{
  botSession = &session;
}

bool_t LiveBroker::buy(double price, double amount, timestamp_t timeout)
{
  BotProtocol::Order order;
  order.entityType = BotProtocol::marketOrder;
  order.type = BotProtocol::Order::buy;
  order.price = price;
  order.amount = amount;
  timestamp_t orderTimeout = time + timeout;
  order.timeout = orderTimeout;
  if(!botConnection.createMarketOrder(order))
    return false;
  ASSERT(order.timeout == orderTimeout);
  double charge = order.price * order.amount + order.fee; 

  order.entityType = BotProtocol::sessionOrder;
  botConnection.updateSessionOrder(order);

  BotProtocol::Marker marker;
  marker.entityType = BotProtocol::sessionMarker;
  marker.date = time;
  marker.type = BotProtocol::Marker::buyAttempt;
  botConnection.createSessionMarker(marker);

  openOrders.append(order);
  balance.availableUsd -= charge;
  balance.reservedUsd += charge;
  botConnection.updateSessionBalance(balance);
  return true;
}

bool_t LiveBroker::sell(double price, double amount, timestamp_t timeout)
{
  if(amount > balance.availableBtc)
    return false;

  BotProtocol::Order order;
  order.entityType = BotProtocol::marketOrder;
  order.type = BotProtocol::Order::sell;
  order.price = price;
  order.amount = amount;
  timestamp_t orderTimeout = time + timeout;
  order.timeout = orderTimeout;
  if(!botConnection.createMarketOrder(order))
    return false;
  ASSERT(order.timeout == orderTimeout);

  order.entityType = BotProtocol::sessionOrder;
  botConnection.updateSessionOrder(order);

  BotProtocol::Marker marker;
  marker.entityType = BotProtocol::sessionMarker;
  marker.date = time;
  marker.type = BotProtocol::Marker::sellAttempt;
  botConnection.createSessionMarker(marker);

  openOrders.append(order);
  balance.availableBtc -= amount;
  balance.reservedBtc += amount;
  botConnection.updateSessionBalance(balance);
  return true;
}

uint_t LiveBroker::getOpenBuyOrderCount() const
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

uint_t LiveBroker::getOpenSellOrderCount() const
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

void_t LiveBroker::getTransactions(List<BotProtocol::Transaction>& transactions) const
{
  for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = this->transactions.begin(), end = this->transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    transactions.append(transaction);
  }
}

void_t LiveBroker::getBuyTransactions(List<BotProtocol::Transaction>& transactions) const
{
  for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = this->transactions.begin(), end = this->transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    if(transaction.type == BotProtocol::Transaction::buy)
      transactions.append(transaction);
  }
}

void_t LiveBroker::getSellTransactions(List<BotProtocol::Transaction>& transactions) const
{
  for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = this->transactions.begin(), end = this->transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    if(transaction.type == BotProtocol::Transaction::sell)
      transactions.append(transaction);
  }
}

void_t LiveBroker::removeTransaction(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Transaction>::Iterator it = transactions.find(id);
  if(it == transactions.end())
    return;
  const BotProtocol::Transaction& transaction = *it;
  botConnection.removeSessionTransaction(transaction.entityId);
  transactions.remove(it);
}

void_t LiveBroker::updateTransaction(const BotProtocol::Transaction& transaction)
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

void_t LiveBroker::warning(const String& message)
{
  botConnection.addLogMessage(message);
}
