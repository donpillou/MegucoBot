
#include <nstd/Time.h>
#include <nstd/Math.h>
#include <nstd/Map.h>

#include "LiveBroker.h"
#include "BotConnection.h"

LiveBroker::LiveBroker(BotConnection& botConnection, const String& currencyBase, const String& currencyComm, const BotProtocol::Balance& balance, const List<BotProtocol::Transaction>& transactions, const List<BotProtocol::SessionAsset>& assets, const List<BotProtocol::Order>& orders, const List<BotProtocol::SessionProperty>& properties) :
  botConnection(botConnection), currencyBase(currencyBase), currencyComm(currencyComm),
  time(0), lastBuyTime(0), lastSellTime(0), lastOrderRefreshTime(0)
{
  for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    this->transactions.append(transaction.entityId, transaction);
  }
  for(List<BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const BotProtocol::SessionAsset& asset = *i;
    this->assets.append(asset.entityId, asset);
  }
  for(List<BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
  {
    const BotProtocol::Order& order = *i;
    this->openOrders.append(order.entityId, order);
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
    botSession.handleTrade(trade, tradeAge);
    return;
  }

  time = trade.time;

  for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
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

  botSession.handleTrade(trade, 0);
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
  for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
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

      next = i; // update next since order list may have changed in bot session handler
      ++next;
      openOrders.remove(i);

      // update balance
      BotProtocol::Balance marketBalance;
      botConnection.getMarketBalance(marketBalance);
    }
  }
}

void_t LiveBroker::cancelTimedOutOrders(Bot::Session& botSession)
{
  for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const BotProtocol::Order& order = *i;
    if(order.timeout > 0 && time >= order.timeout)
    {
      if(botConnection.removeMarketOrder(order.entityId))
      {
        botConnection.removeSessionOrder(order.entityId);

        if(order.type == BotProtocol::Order::buy)
          botSession.handleBuyTimeout(order.entityId);
        else
          botSession.handleSellTimeout(order.entityId);

        next = i; // update next since order list may have changed in bot session handler
        ++next;
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
  order.timeout = timeout > 0 ? time + timeout : 0;
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

  openOrders.append(order.entityId, order);
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
  order.timeout = timeout > 0 ? time + timeout : 0;
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

  openOrders.append(order.entityId, order);
  return true;
}

bool_t LiveBroker::cancelOder(uint32_t id)
{
  if(!botConnection.removeMarketOrder(id))
  {
    error = botConnection.getErrorString();
    return false;
  }
  botConnection.removeSessionOrder(id);
  openOrders.remove(id);
  return true;
}

const BotProtocol::Order* LiveBroker::getOrder(uint32_t id) const
{
  HashMap<uint32_t, BotProtocol::Order>::Iterator it = openOrders.find(id);
  if(it == openOrders.end())
    return 0;
  return &*it;
}

uint_t LiveBroker::getOpenBuyOrderCount() const
{
  size_t openBuyOrders = 0;
  for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
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
  for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const BotProtocol::Order& order = *i;
    if(order.type == BotProtocol::Order::sell)
      ++openSellOrders;
  }
  return openSellOrders;
}

const BotProtocol::SessionAsset* LiveBroker::getAsset(uint32_t id) const
{
  HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator it = assets.find(id);
  if(it == assets.end())
    return 0;
  return &*it;
}

bool_t LiveBroker::createAsset(BotProtocol::SessionAsset& asset)
{
  if(!botConnection.createSessionAsset(asset))
  {
    error = botConnection.getErrorString();
    return false;
  }
  assets.append(asset.entityId, asset);
  return true;
}

void_t LiveBroker::removeAsset(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator it = assets.find(id);
  if(it == assets.end())
    return;
  const BotProtocol::SessionAsset& asset = *it;
  botConnection.removeSessionAsset(asset.entityId);
  assets.remove(it);
}

void_t LiveBroker::updateAsset(const BotProtocol::SessionAsset& asset)
{
  HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator it = assets.find(asset.entityId);
  if(it == assets.end())
    return;
  BotProtocol::SessionAsset& destAsset = *it;
  destAsset = asset;
  destAsset.entityType = BotProtocol::sessionAsset;
  destAsset.entityId = asset.entityId;
  botConnection.updateSessionAsset(destAsset);
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
    {
      botConnection.updateSessionProperty(property);
      *i = property;
    }
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

void_t LiveBroker::addMarker(BotProtocol::Marker::Type markerType)
{
  BotProtocol::Marker marker;
  marker.entityType = BotProtocol::sessionMarker;
  marker.date = time;
  marker.type = markerType;
  botConnection.createSessionMarker(marker);
}

void_t LiveBroker::warning(const String& message)
{
  botConnection.addLogMessage(Time::time(), message);
}
