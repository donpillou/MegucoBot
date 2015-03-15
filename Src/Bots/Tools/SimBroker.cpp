
#include <nstd/Debug.h>
#include <nstd/Time.h>
#include <nstd/Math.h>

#include "SimBroker.h"
#include "BotConnection.h"

SimBroker::SimBroker(BotConnection& botConnection, const String& currencyBase, const String& currencyComm, double tradeFee, const BotProtocol::Balance& balance, const List<BotProtocol::Transaction>& transactions, const List<BotProtocol::SessionAsset>& assets, const List<BotProtocol::Order>& orders, const List<BotProtocol::SessionProperty>& properties, timestamp_t maxTradeAge) :
  botConnection(botConnection), currencyBase(currencyBase), currencyComm(currencyComm),
  time(0), lastBuyTime(0), lastSellTime(0), tradeFee(tradeFee), startTime(0), maxTradeAge(maxTradeAge)
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

void_t SimBroker::handleTrade(Bot::Session& botSession, const DataProtocol::Trade& trade)
{
  if(startTime == 0)
    startTime = trade.time;
  if((timestamp_t)(trade.time - startTime) <= maxTradeAge)
  {
    botSession.handleTrade(trade, startTime + maxTradeAge - trade.time);
    return; 
  }

  if(trade.flags & DataProtocol::syncFlag)
    warning("sync");

  time = trade.time;

  for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const BotProtocol::Order& order = *i;
    if(order.timeout > 0 && time >= order.timeout)
    {
      botConnection.removeSessionOrder(order.entityId);

      if(order.type == BotProtocol::Order::buy)
        botSession.handleBuyTimeout(order.entityId);
      else
        botSession.handleSellTimeout(order.entityId);

      next = i; // update next since order list may have changed in bot session handler
      ++next;
      openOrders.remove(i);
    }
    else if((order.type == BotProtocol::Order::buy && trade.price < order.price) ||
            ( order.type == BotProtocol::Order::sell && trade.price > order.price) )
    {
      BotProtocol::Transaction transaction;
      transaction.entityType = BotProtocol::sessionTransaction;
      transaction.type = order.type == BotProtocol::Order::buy ? BotProtocol::Transaction::buy : BotProtocol::Transaction::sell;
      transaction.date = time;
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
    }
  }

  botSession.handleTrade(trade, 0);
}

bool_t SimBroker::buy(double price, double amount, double total, timestamp_t timeout, uint32_t* id, double* orderedAmount)
{
  if(amount != 0.)
    total = Math::ceil(amount * price * (1. + tradeFee) * 100.) / 100.;

  //if(total > balance.availableUsd)
  //{
  //  error = "Insufficient balance.";
  //  return false;
  //}

  amount = total / (price * (1. + tradeFee));
  amount = Math::floor(amount * 100000000.) / 100000000.;

  BotProtocol::Order order;
  order.entityType = BotProtocol::sessionOrder;
  order.type = BotProtocol::Order::buy;
  order.date = time;
  order.amount = amount;
  order.price = price;
  order.total = total;
  timestamp_t orderTimeout = timeout > 0 ? time + timeout : 0;
  order.timeout = orderTimeout;
  if(!botConnection.createSessionOrder(order))
  {
    error = botConnection.getErrorString();
    return false;
  }
  ASSERT(order.timeout == orderTimeout);
  if(id)
    *id = order.entityId;
  if(orderedAmount)
    *orderedAmount = order.amount;

  BotProtocol::Marker marker;
  marker.entityType = BotProtocol::sessionMarker;
  marker.date = time;
  marker.type = BotProtocol::Marker::buyAttempt;
  botConnection.createSessionMarker(marker);

  openOrders.append(order.entityId, order);
  return true;
}

bool_t SimBroker::sell(double price, double amount, double total, timestamp_t timeout, uint32_t* id, double* orderedAmount)
{
  if(amount != 0.)
    total = Math::floor(amount * price * (1. - tradeFee) * 100.) / 100.;

  amount = total / (price * (1. - tradeFee));
  amount = Math::ceil(amount * 100000000.) / 100000000.;

  //if(amount > balance.availableBtc)
  //{
  //  error = "Insufficient balance.";
  //  return false;
  //}

  BotProtocol::Order order;
  order.entityType = BotProtocol::sessionOrder;
  order.type = BotProtocol::Order::sell;
  order.date = time;
  order.amount = amount;
  order.price = price;
  order.total = Math::floor(price * amount * (1 - tradeFee) * 100.) / 100.;
  timestamp_t orderTimeout = timeout > 0 ? time + timeout : 0;
  order.timeout = orderTimeout;
  if(!botConnection.createSessionOrder(order))
  {
    error = botConnection.getErrorString();
    return false;
  }
  ASSERT(order.timeout == orderTimeout);
  if(id)
    *id = order.entityId;
  if(orderedAmount)
    *orderedAmount = order.amount;

  BotProtocol::Marker marker;
  marker.entityType = BotProtocol::sessionMarker;
  marker.date = time;
  marker.type = BotProtocol::Marker::sellAttempt;
  botConnection.createSessionMarker(marker);

  openOrders.append(order.entityId, order);
  return true;
}

bool_t SimBroker::cancelOder(uint32_t id)
{
  if(!botConnection.removeSessionOrder(id))
  {
    error = botConnection.getErrorString();
    return false;
  }
  openOrders.remove(id);
  return true;
}

const BotProtocol::Order* SimBroker::getOrder(uint32_t id) const
{
  HashMap<uint32_t, BotProtocol::Order>::Iterator it = openOrders.find(id);
  if(it == openOrders.end())
    return 0;
  return &*it;
}

size_t SimBroker::getOpenBuyOrderCount() const
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

size_t SimBroker::getOpenSellOrderCount() const
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

const BotProtocol::SessionAsset* SimBroker::getAsset(uint32_t id) const
{
  HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator it = assets.find(id);
  if(it == assets.end())
    return 0;
  return &*it;
}

bool_t SimBroker::createAsset(BotProtocol::SessionAsset& asset)
{
  if(!botConnection.createSessionAsset(asset))
  {
    error = botConnection.getErrorString();
    return false;
  }
  assets.append(asset.entityId, asset);
  return true;
}

void_t SimBroker::removeAsset(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator it = assets.find(id);
  if(it == assets.end())
    return;
  const BotProtocol::SessionAsset& asset = *it;
  botConnection.removeSessionAsset(asset.entityId);
  assets.remove(it);
}

void_t SimBroker::updateAsset(const BotProtocol::SessionAsset& asset)
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

const BotProtocol::SessionProperty* SimBroker::getProperty(uint32_t id) const
{
  for(HashMap<String, BotProtocol::SessionProperty>::Iterator i = properties.begin(), end = properties.end(); i != end; ++i)
    if(i->entityId == id)
      return &*i;
  return 0;
}

void_t SimBroker::updateProperty(const BotProtocol::SessionProperty& property)
{
  for(HashMap<String, BotProtocol::SessionProperty>::Iterator i = properties.begin(), end = properties.end(); i != end; ++i)
    if(i->entityId == property.entityId)
    {
      botConnection.updateSessionProperty(property);
      *i = property;
    }
}

double SimBroker::getProperty(const String& name, double defaultValue) const
{
  HashMap<String, BotProtocol::SessionProperty>::Iterator it = properties.find(name);
  if(it == properties.end())
    return defaultValue;
  BotProtocol::SessionProperty& property = *it;
  return BotProtocol::getString(property.value).toDouble();
}

String SimBroker::getProperty(const String& name, const String& defaultValue) const
{
  HashMap<String, BotProtocol::SessionProperty>::Iterator it = properties.find(name);
  if(it == properties.end())
    return defaultValue;
  BotProtocol::SessionProperty& property = *it;
  return BotProtocol::getString(property.value);
}

void SimBroker::registerProperty(const String& name, double value, uint32_t flags, const String& unit)
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

void SimBroker::registerProperty(const String& name, const String& value, uint32_t flags, const String& unit)
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

void SimBroker::setProperty(const String& name, double value, uint32_t flags, const String& unit)
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

void SimBroker::setProperty(const String& name, const String& value, uint32_t flags, const String& unit)
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

void SimBroker::removeProperty(const String& name)
{
  HashMap<String, BotProtocol::SessionProperty>::Iterator it = properties.find(name);
  if(it == properties.end())
    return;
  botConnection.removeSessionProperty(it->entityId);
  properties.remove(it);
}

void_t SimBroker::addMarker(BotProtocol::Marker::Type markerType)
{
  BotProtocol::Marker marker;
  marker.entityType = BotProtocol::sessionMarker;
  marker.date = time;
  marker.type = markerType;
  botConnection.createSessionMarker(marker);
}

void_t SimBroker::warning(const String& message)
{
  botConnection.addLogMessage(time, message);
}
