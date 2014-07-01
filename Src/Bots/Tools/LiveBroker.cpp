
#include <nstd/Time.h>
#include <nstd/Math.h>
#include <nstd/Map.h>

#include "LiveBroker.h"
#include "BotConnection.h"

LiveBroker::LiveBroker(BotConnection& botConnection,  const BotProtocol::Balance& balance, const List<BotProtocol::Transaction>& transactions, const List<BotProtocol::SessionItem>& items, const List<BotProtocol::Order>& orders) :
  botConnection(botConnection), balance(balance),
  time(0), lastBuyTime(0), lastSellTime(0), lastOrderRefreshTime(0)
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

void_t LiveBroker::handleTrade(Bot::Session& botSession, const DataProtocol::Trade& trade)
{
  if(trade.flags & DataProtocol::replayedFlag)
  {
    timestamp_t tradeAge = Time::time() - trade.time;
    if(tradeAge <= 0LL)
      tradeAge = 1LL;
    tradeHandler.add(trade, tradeAge);
    return;
  }

  tradeHandler.add(trade, 0LL);
  time = trade.time;

  for(List<BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const BotProtocol::Order& order = *i;
    if (Math::abs(order.price - trade.price) <= 0.01)
    {
      refreshOrders(botSession);
      lastOrderRefreshTime = Time::time();
      goto doneRefreshing;
    }
  }
  if(!openOrders.isEmpty())
  {
    timestamp_t now = Time::time();
    if(now - lastOrderRefreshTime >= 120 * 1000)
    {
      lastOrderRefreshTime = now;
      refreshOrders(botSession);
    }
  }
doneRefreshing:

  cancelTimedOutOrders(botSession);

  botSession.handle(trade, tradeHandler.values);
}

void_t LiveBroker::refreshOrders(Bot::Session& botSession)
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
}

void_t LiveBroker::cancelTimedOutOrders(Bot::Session& botSession)
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
        refreshOrders(botSession);
        return;
      }
    }
  }
}

bool_t LiveBroker::buy(double price, double amount, timestamp_t timeout)
{
  BotProtocol::Order order;
  order.entityType = BotProtocol::marketOrder;
  order.type = BotProtocol::Order::buy;
  order.price = price;
  order.amount = amount;
  if(!botConnection.createMarketOrder(order))
    return false;
  lastOrderRefreshTime = Time::time();
  order.timeout = time + timeout;
  double charge = order.price * order.amount + order.fee;

#ifdef BOT_TESTBOT
  List<BotProtocol::Order> marketOrders;
  botConnection.getMarketOrders(marketOrders);
  for(List<BotProtocol::Order>::Iterator i = marketOrders.begin(), end = marketOrders.end(); i != end; ++i)
  {
    if(i->entityId == order.entityId)
    {
      ASSERT(i->type == BotProtocol::Order::buy);
      ASSERT(i->amount == order.amount);
      ASSERT(i->price == order.price);
      goto testok;
    }
  }
  ASSERT(false);
testok:
#endif

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
  BotProtocol::Order order;
  order.entityType = BotProtocol::marketOrder;
  order.type = BotProtocol::Order::sell;
  order.price = price;
  order.amount = amount;
  if(!botConnection.createMarketOrder(order))
    return false;
  lastOrderRefreshTime = Time::time();
  order.timeout = time + timeout;

#ifdef BOT_TESTBOT
  List<BotProtocol::Order> marketOrders;
  botConnection.getMarketOrders(marketOrders);
  for(List<BotProtocol::Order>::Iterator i = marketOrders.begin(), end = marketOrders.end(); i != end; ++i)
  {
    if(i->entityId == order.entityId)
    {
      ASSERT(i->type == BotProtocol::Order::sell);
      ASSERT(i->amount == order.amount);
      ASSERT(i->price == order.price);
      goto testok;
    }
  }
  ASSERT(false);
testok:
#endif

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

void_t LiveBroker::getItems(List<BotProtocol::SessionItem>& items) const
{
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = this->items.begin(), end = this->items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    items.append(item);
  }
}

void_t LiveBroker::getBuyItems(List<BotProtocol::SessionItem>& items) const
{
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = this->items.begin(), end = this->items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.currentType == BotProtocol::SessionItem::buy)
      items.append(item);
  }
}

void_t LiveBroker::getSellItems(List<BotProtocol::SessionItem>& items) const
{
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = this->items.begin(), end = this->items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.currentType == BotProtocol::SessionItem::sell)
      items.append(item);
  }
}

bool_t LiveBroker::createItem(BotProtocol::SessionItem& item)
{
  if(!botConnection.createSessionItem(item))
    return false;
  items.append(item.entityId, item);
  return true;
}

void_t LiveBroker::removeItem(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::SessionItem>::Iterator it = items.find(id);
  if(it == items.end())
    return;
  const BotProtocol::SessionItem& item = *it;
  botConnection.removeSessionItem(item.entityId);
  items.remove(it);
}

void_t LiveBroker::updateItem(const BotProtocol::SessionItem& item)
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

void_t LiveBroker::warning(const String& message)
{
  botConnection.addLogMessage(message);
}
