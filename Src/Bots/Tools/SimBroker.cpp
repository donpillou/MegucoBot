
#include <nstd/Debug.h>
#include <nstd/Time.h>
#include <nstd/Math.h>

#include "SimBroker.h"
#include "BotConnection.h"

SimBroker::SimBroker(BotConnection& botConnection,  const BotProtocol::Balance& balance, const List<BotProtocol::Transaction>& transactions, const List<BotProtocol::SessionItem>& items, const List<BotProtocol::Order>& orders) :
  botConnection(botConnection), balance(balance), 
  time(0), lastBuyTime(0), lastSellTime(0), startTime(0)
{
  for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    this->transactions.append(transaction.entityId, transaction);
  }
  for(List<BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    this->items.append(item.entityId, item);
  }
  for(List<BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
  {
    const BotProtocol::Order& order = *i;
    this->openOrders.append(order);
  }
}

void_t SimBroker::handleTrade(Bot::Session& botSession, const DataProtocol::Trade& trade)
{
  static const timestamp_t warmupTime = 60 * 60 * 1000LL; // wait for 60 minutes of trade data to be evaluated

  if(startTime == 0)
    startTime = trade.time;
  if(trade.time - startTime <= warmupTime)
  {
    tradeHandler.add(trade, startTime + warmupTime - trade.time);
    return; 
  }

  if(trade.flags & DataProtocol::syncFlag)
    warning("sync");

  tradeHandler.add(trade, 0LL);

  time = trade.time;

  for(List<BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const BotProtocol::Order& order = *i;
    if(time >= order.timeout)
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
      if(order.type == BotProtocol::Order::buy)
      {
        marker.type = BotProtocol::Marker::buy;
        botSession.handleBuy(transaction);
      }
      else
      {
        marker.type = BotProtocol::Marker::sell;
        botSession.handleSell(transaction);
      }
      botConnection.createSessionMarker(marker);
    }
  }

  botSession.handle(trade, tradeHandler.values);
}

bool_t SimBroker::buy(double price, double amount, timestamp_t timeout)
{
  double fee = Math::ceil(amount * price * balance.fee * 100.) / 100.;
  // todo: fee = Math::ceil(amount * price * (1. + balance.fee) * 100.) / 100. - amount * price; ???
  double charge = amount * price + fee;
  if(charge > balance.availableUsd)
  {
    error = "Insufficient balance.";
    return false;
  }

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
  balance.availableUsd -= charge;
  balance.reservedUsd += charge;
  botConnection.updateSessionBalance(balance);
  return true;
}

bool_t SimBroker::sell(double price, double amount, timestamp_t timeout)
{
  if(amount > balance.availableBtc)
  {
    error = "Insufficient balance.";
    return false;
  }

  BotProtocol::Order order;
  order.entityType = BotProtocol::sessionOrder;
  order.type = BotProtocol::Order::sell;
  order.date = time;
  order.amount = amount;
  order.price = price;
  order.fee = Math::ceil(amount * price * balance.fee * 100.) / 100.;
  // todo: think about fee computation
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
  balance.availableBtc -= amount;
  balance.reservedBtc += amount;
  botConnection.updateSessionBalance(balance);
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

void_t SimBroker::getItems(List<BotProtocol::SessionItem>& items) const
{
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = this->items.begin(), end = this->items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    items.append(item);
  }
}

void_t SimBroker::getBuyItems(List<BotProtocol::SessionItem>& items) const
{
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = this->items.begin(), end = this->items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.state == BotProtocol::SessionItem::waitBuy)
      items.append(item);
  }
}

void_t SimBroker::getSellItems(List<BotProtocol::SessionItem>& items) const
{
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = this->items.begin(), end = this->items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.state == BotProtocol::SessionItem::waitSell)
      items.append(item);
  }
}

bool_t SimBroker::createItem(BotProtocol::SessionItem& item)
{
  if(!botConnection.createSessionItem(item))
  {
    error = botConnection.getErrorString();
    return false;
  }
  items.append(item.entityId, item);
  return true;
}

void_t SimBroker::removeItem(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::SessionItem>::Iterator it = items.find(id);
  if(it == items.end())
    return;
  const BotProtocol::SessionItem& item = *it;
  botConnection.removeSessionItem(item.entityId);
  items.remove(it);
}

void_t SimBroker::updateItem(const BotProtocol::SessionItem& item)
{
  HashMap<uint32_t, BotProtocol::SessionItem>::Iterator it = items.find(item.entityId);
  if(it == items.end())
    return;
  BotProtocol::SessionItem& destItem = *it;
  destItem = item;
  destItem.entityType = BotProtocol::sessionItem;
  destItem.entityId = item.entityId;
  botConnection.updateSessionItem(destItem);
}

void_t SimBroker::warning(const String& message)
{
  botConnection.addLogMessage(message);
}
