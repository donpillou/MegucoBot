
#include <nstd/Debug.h>
#include <nstd/Time.h>
#include <nstd/Math.h>

#include "SimBroker.h"
#include "Main.h"

SimBroker::SimBroker(Main& main, const String& currencyBase, const String& currencyComm, double tradeFee, int64_t maxTradeAge) :
  main(main), currencyBase(currencyBase), currencyComm(currencyComm),
  time(0), lastBuyTime(0), lastSellTime(0), tradeFee(tradeFee), startTime(0), maxTradeAge(maxTradeAge) {}

void_t SimBroker::handleTrade(Bot::Session& botSession, const Bot::Trade& trade, bool_t replayed)
{
  if(startTime == 0)
    startTime = trade.time;
  if((int64_t)(trade.time - startTime) <= maxTradeAge)
  {
    botSession.handleTrade(trade, startTime + maxTradeAge - trade.time);
    return; 
  }

  //if(trade.flags & DataProtocol::syncFlag)
  //  warning("sync");

  time = trade.time;

  for(HashMap<uint64_t, Bot::Order>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const Bot::Order& order = *i;
    if(order.timeout > 0 && time >= (int64_t)order.timeout)
    {
      main.removeSessionOrder(order.id);

      if(order.type == meguco_user_broker_order_buy)
        botSession.handleBuyTimeout(order.id);
      else
        botSession.handleSellTimeout(order.id);

      next = i; // update next since order list may have changed in bot session handler
      ++next;
      openOrders.remove(i);
    }
    else if((order.type == meguco_user_broker_order_buy && trade.price < order.price) ||
            ( order.type == meguco_user_broker_order_sell && trade.price > order.price) )
    {
      Bot::Transaction transaction;
      transaction.type = order.type == meguco_user_broker_order_buy ? meguco_user_broker_transaction_buy : meguco_user_broker_transaction_sell;
      transaction.price = order.price;
      transaction.amount = order.amount;
      transaction.total = order.total;
      main.createSessionTransaction(transaction);

      if(order.type == meguco_user_broker_order_buy)
        lastBuyTime = time;
      else
        lastSellTime = time;

      main.removeSessionOrder(order.id);

      Bot::Marker marker;
      if(order.type == meguco_user_broker_order_buy)
      {
        marker.type = meguco_user_session_marker_buy;
        botSession.handleBuy(order.id, transaction);
      }
      else
      {
        marker.type = meguco_user_session_marker_sell;
        botSession.handleSell(order.id, transaction);
      }
      main.createSessionMarker(marker);

      next = i; // update next since order list may have changed in bot session handler
      ++next;
      openOrders.remove(i);
    }
  }

  botSession.handleTrade(trade, 0);
}

bool_t SimBroker::buy(double price, double amount, double total, int64_t timeout, uint64_t* id, double* orderedAmount)
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

  Bot::Order order;
  order.type = meguco_user_broker_order_buy;
  order.amount = amount;
  order.price = price;
  order.total = total;
  int64_t orderTimeout = timeout > 0 ? time + timeout : 0;
  order.timeout = orderTimeout;
  if(!main.createSessionOrder(order))
  {
    error = main.getErrorString();
    return false;
  }
  ASSERT(order.timeout == (uint64_t)orderTimeout);
  if(id)
    *id = order.id;
  if(orderedAmount)
    *orderedAmount = order.amount;

  Bot::Marker marker;
  marker.type = meguco_user_session_marker_buy_attempt;
  main.createSessionMarker(marker);

  openOrders.append(order.id, order);
  return true;
}

bool_t SimBroker::sell(double price, double amount, double total, int64_t timeout, uint64_t* id, double* orderedAmount)
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

  Bot::Order order;
  order.type = meguco_user_broker_order_sell;
  order.amount = amount;
  order.price = price;
  order.total = Math::floor(price * amount * (1 - tradeFee) * 100.) / 100.;
  int64_t orderTimeout = timeout > 0 ? time + timeout : 0;
  order.timeout = orderTimeout;
  if(!main.createSessionOrder(order))
  {
    error = main.getErrorString();
    return false;
  }
  ASSERT(order.timeout == (uint64_t)orderTimeout);
  if(id)
    *id = order.id;
  if(orderedAmount)
    *orderedAmount = order.amount;

  Bot::Marker marker;
  marker.type = meguco_user_session_marker_sell_attempt;
  main.createSessionMarker(marker);

  openOrders.append(order.id, order);
  return true;
}

bool_t SimBroker::cancelOder(uint64_t id)
{
  if(!main.removeSessionOrder(id))
  {
    error = main.getErrorString();
    return false;
  }
  openOrders.remove(id);
  return true;
}

const Bot::Order* SimBroker::getOrder(uint64_t id) const
{
  HashMap<uint64_t, Bot::Order>::Iterator it = openOrders.find(id);
  if(it == openOrders.end())
    return 0;
  return &*it;
}

size_t SimBroker::getOpenBuyOrderCount() const
{
  size_t openBuyOrders = 0;
  for(HashMap<uint64_t, Bot::Order>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const Bot::Order& order = *i;
    if(order.type == meguco_user_broker_order_buy)
      ++openBuyOrders;
  }
  return openBuyOrders;
}

size_t SimBroker::getOpenSellOrderCount() const
{
  size_t openSellOrders = 0;
  for(HashMap<uint64_t, Bot::Order>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const Bot::Order& order = *i;
    if(order.type == meguco_user_broker_order_sell)
      ++openSellOrders;
  }
  return openSellOrders;
}

const Bot::Asset* SimBroker::getAsset(uint64_t id) const
{
  HashMap<uint64_t, Bot::Asset>::Iterator it = assets.find(id);
  if(it == assets.end())
    return 0;
  return &*it;
}

bool_t SimBroker::createAsset(Bot::Asset& asset)
{
  if(!main.createSessionAsset(asset))
  {
    error = main.getErrorString();
    return false;
  }
  assets.append(asset.id, asset);
  return true;
}

void_t SimBroker::removeAsset(uint64_t id)
{
  HashMap<uint64_t, Bot::Asset>::Iterator it = assets.find(id);
  if(it == assets.end())
    return;
  const Bot::Asset& asset = *it;
  main.removeSessionAsset(asset.id);
  assets.remove(it);
}

void_t SimBroker::updateAsset(const Bot::Asset& asset)
{
  HashMap<uint64_t, Bot::Asset>::Iterator it = assets.find(asset.id);
  if(it == assets.end())
    return;
  Bot::Asset& destAsset = *it;
  destAsset = asset;
  main.updateSessionAsset(asset);
}

double SimBroker::getProperty(const String& name, double defaultValue) const
{
  HashMap<String, Bot::Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
    return defaultValue;
  return (*it)->value.toDouble();
}

String SimBroker::getProperty(const String& name, const String& defaultValue) const
{
  HashMap<String, Bot::Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
    return defaultValue;
  return (*it)->value;
}

void SimBroker::registerProperty(const String& name, double value, uint32_t flags, const String& unit)
{
  registerProperty(name, String::fromDouble(value), meguco_user_session_property_number, flags, unit);
}

void SimBroker::registerProperty(const String& name, const String& value, uint32_t flags, const String& unit)
{
  registerProperty(name, value, meguco_user_session_property_string, flags, unit);
}

void SimBroker::registerProperty(const String& name, const String& value, meguco_user_session_property_type type, uint32_t flags, const String& unit)
{
  HashMap<String, Bot::Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
  {
    Bot::Property newProperty;
    newProperty.flags = flags;
    newProperty.type = type;
    newProperty.name = name;
    newProperty.value = value;
    newProperty.unit = unit;
    main.createSessionProperty(newProperty);
    Bot::Property& property = properties.append(newProperty.id, newProperty);
    propertiesByName.append(name, &property);
  }
  else
  {
    Bot::Property& property = **it;
    if(flags != property.flags || type != property.type || property.unit != unit)
    { // update property attributes, but keep value
      property.flags = flags;
      property.type = type;
      property.unit = unit;
      main.updateSessionProperty(property);
    }
  }
}

void SimBroker::setProperty(const String& name, double value)
{
  setProperty(name, String::fromDouble(value));
}

void SimBroker::setProperty(const String& name, const String& value)
{
  HashMap<String, Bot::Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
    return;
  Bot::Property& property = **it;
  property.value = value;
  main.updateSessionProperty(property);
}

void_t SimBroker::addMarker(meguco_user_session_marker_type markerType)
{
  Bot::Marker marker;
  marker.type = markerType;
  main.createSessionMarker(marker);
}

void_t SimBroker::warning(const String& message)
{
  main.addLogMessage(time, message);
}

void_t SimBroker::registerOrder(const Bot::Order& order)
{
  openOrders.append(order.id, order);
}

void_t SimBroker::registerAsset(const Bot::Asset& asset)
{
  assets.append(asset.id, asset);
}

void_t SimBroker::unregisterAsset(uint64_t id)
{
  assets.remove(id);
}

void_t SimBroker::registerProperty(const Bot::Property& property)
{
  Bot::Property& newProperty = properties.append(property.id, property);
  propertiesByName.append(property.name, &newProperty);
}

const Bot::Property* SimBroker::getProperty(uint64_t id)
{
  HashMap<uint64_t, Bot::Property>::Iterator it = properties.find(id);
  if(it == properties.end())
    return 0;
  return &*it;
}
