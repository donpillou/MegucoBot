
#include <nstd/Time.h>
#include <nstd/Math.h>
#include <nstd/Map.h>

#include "LiveBroker.h"
#include "BotConnection.h"

LiveBroker::LiveBroker(BotConnection& botConnection, const String& currencyBase, const String& currencyComm, const BotProtocol::Balance& balance, const List<BotProtocol::Transaction>& transactions, const List<BotProtocol::SessionItem>& items, const List<BotProtocol::Order>& orders, const List<BotProtocol::SessionProperty>& properties) :
  botConnection(botConnection), currencyBase(currencyBase), currencyComm(currencyComm),
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
  for(List<BotProtocol::SessionProperty>::Iterator i = properties.begin(), end = properties.end(); i != end; ++i)
  {
    BotProtocol::SessionProperty& property = *i;
    this->properties.append(BotProtocol::getString(property.name), property);
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
      transaction.total = order.total;
      botConnection.createSessionTransaction(transaction);
      transactions.append(transaction.entityId, transaction);

      if(order.type == BotProtocol::Order::buy)
        lastBuyTime = time;
      else
        lastSellTime = time;

      botConnection.removeSessionOrder(order.entityId);

      BotProtocol::Marker marker;
      marker.entityType = BotProtocol::sessionMarker;
      marker.date = time;
      if(order.type == BotProtocol::Order::buy)
      {
        marker.type = BotProtocol::Marker::buy;
        botSession.handleBuy(order.entityId, transaction);
      }
      else
      {
        marker.type = BotProtocol::Marker::sell;
        botSession.handleSell(order.entityId, transaction);
      }
      botConnection.createSessionMarker(marker);
      openOrders.remove(i);
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
        botConnection.removeSessionOrder(order.entityId);

        if(order.type == BotProtocol::Order::buy)
          botSession.handleBuyTimeout(order.entityId);
        else
          botSession.handleSellTimeout(order.entityId);

        openOrders.remove(i);
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

bool_t LiveBroker::buy(double price, double amount, double total, timestamp_t timeout, uint32_t* id, double* orderedAmount)
{
  BotProtocol::Order order;
  order.entityType = BotProtocol::marketOrder;
  order.type = BotProtocol::Order::buy;
  order.price = price;
  order.amount = amount;
  order.total = total;
  if(!botConnection.createMarketOrder(order))
  {
    error = botConnection.getErrorString();
    return false;
  }
  lastOrderRefreshTime = Time::time();
  order.timeout = time + timeout;
  if(id)
    *id = order.entityId;
  if(orderedAmount)
    *orderedAmount = order.amount;

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
  return true;
}

bool_t LiveBroker::sell(double price, double amount, double total, timestamp_t timeout, uint32_t* id, double* orderedAmount)
{
  BotProtocol::Order order;
  order.entityType = BotProtocol::marketOrder;
  order.type = BotProtocol::Order::sell;
  order.price = price;
  order.amount = amount;
  order.total = total;
  if(!botConnection.createMarketOrder(order))
  {
    error = botConnection.getErrorString();
    return false;
  }
  lastOrderRefreshTime = Time::time();
  order.timeout = time + timeout;
  if(id)
    *id = order.entityId;
  if(orderedAmount)
    *orderedAmount = order.amount;

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

const BotProtocol::SessionItem* LiveBroker::getItem(uint32_t id) const
{
  HashMap<uint32_t, BotProtocol::SessionItem>::Iterator it = items.find(id);
  if(it == items.end())
    return 0;
  return &*it;
}

bool_t LiveBroker::createItem(BotProtocol::SessionItem& item)
{
  if(!botConnection.createSessionItem(item))
  {
    error = botConnection.getErrorString();
    return false;
  }
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

const BotProtocol::SessionProperty* LiveBroker::getProperty(uint32_t id) const
{
  for(HashMap<String, BotProtocol::SessionProperty>::Iterator i = properties.begin(), end = properties.end(); i != end; ++i)
    if(i->entityId == id)
      return &*i;
  return 0;
}

void_t LiveBroker::updateProperty(const BotProtocol::SessionProperty& property)
{
  for(HashMap<String, BotProtocol::SessionProperty>::Iterator i = properties.begin(), end = properties.end(); i != end; ++i)
    if(i->entityId == property.entityId)
      *i = property;
}

double LiveBroker::getProperty(const String& name, double defaultValue) const
{
  HashMap<String, BotProtocol::SessionProperty>::Iterator it = properties.find(name);
  if(it == properties.end())
    return defaultValue;
  BotProtocol::SessionProperty& property = *it;
  return BotProtocol::getString(property.value).toDouble();
}

String LiveBroker::getProperty(const String& name, const String& defaultValue) const
{
  HashMap<String, BotProtocol::SessionProperty>::Iterator it = properties.find(name);
  if(it == properties.end())
    return defaultValue;
  BotProtocol::SessionProperty& property = *it;
  return BotProtocol::getString(property.value);
}

void LiveBroker::registerProperty(const String& name, double value, uint32_t flags, const String& unit)
{
  HashMap<String, BotProtocol::SessionProperty>::Iterator it = properties.find(name);
  if(it == properties.end())
    setProperty(name, value, flags, unit);
  else
  {
    it->flags = flags;
    BotProtocol::setString(it->unit, unit);
  }
}

void LiveBroker::registerProperty(const String& name, const String& value, uint32_t flags, const String& unit)
{
  HashMap<String, BotProtocol::SessionProperty>::Iterator it = properties.find(name);
  if(it == properties.end())
    setProperty(name, value, flags, unit);
  else
  {
    it->flags = flags;
    BotProtocol::setString(it->unit, unit);
  }
}

void LiveBroker::setProperty(const String& name, double value, uint32_t flags, const String& unit)
{
  BotProtocol::SessionProperty property;
  property.entityType = BotProtocol::sessionProperty;
  property.type = BotProtocol::SessionProperty::number;
  property.flags = flags;
  BotProtocol::setString(property.name, name);
  BotProtocol::setString(property.value, String::fromDouble(value));
  BotProtocol::setString(property.unit, unit);

  HashMap<String, BotProtocol::SessionProperty>::Iterator it = properties.find(name);
  if(it == properties.end())
  {
    botConnection.createSessionProperty(property);
    properties.append(name, property);
  }
  else
  {
    property.entityId = it->entityId;
    botConnection.updateSessionProperty(property);
    *it = property;
  }
}

void LiveBroker::setProperty(const String& name, const String& value, uint32_t flags, const String& unit)
{
  BotProtocol::SessionProperty property;
  property.entityType = BotProtocol::sessionProperty;
  property.type = BotProtocol::SessionProperty::string;
  property.flags = flags;
  BotProtocol::setString(property.name, name);
  BotProtocol::setString(property.value, value);
  BotProtocol::setString(property.unit, unit);

  HashMap<String, BotProtocol::SessionProperty>::Iterator it = properties.find(name);
  if(it == properties.end())
  {
    botConnection.createSessionProperty(property);
    properties.append(name, property);
  }
  else
  {
    property.entityId = it->entityId;
    botConnection.updateSessionProperty(property);
    *it = property;
  }
}

void LiveBroker::removeProperty(const String& name)
{
  HashMap<String, BotProtocol::SessionProperty>::Iterator it = properties.find(name);
  if(it == properties.end())
    return;
  botConnection.removeSessionProperty(it->entityId);
  properties.remove(it);
}

void_t LiveBroker::warning(const String& message)
{
  botConnection.addLogMessage(Time::time(), message);
}
