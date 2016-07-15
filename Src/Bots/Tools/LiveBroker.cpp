
#include <nstd/Time.h>
#include <nstd/Math.h>
#include <nstd/Map.h>
#include <megucoprotocol.h>
#include <zlimdbprotocol.h>

#include "LiveBroker.h"
#include "Main.h"

LiveBroker::LiveBroker(Main& main, const String& currencyBase, const String& currencyComm) :
  main(main), currencyBase(currencyBase), currencyComm(currencyComm),
  time(0), lastBuyTime(0), lastSellTime(0), lastOrderRefreshTime(0) {}

void_t LiveBroker::handleTrade2(Bot::Session& botSession, const Bot::Trade& trade, bool_t replayed)
{
  if(replayed)
  {
    int64_t tradeAge = Time::time() - trade.time;
    if(tradeAge <= 0LL)
      tradeAge = 1LL;
    botSession.handleTrade2(trade, tradeAge);
    return;
  }

  time = trade.time;

  for(HashMap<uint64_t, Bot::Order>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const Bot::Order& order = *i;
    if (Math::abs(order.price - trade.price) <= 0.01)
    {
      refreshOrders(botSession);
      lastOrderRefreshTime = Time::time();
      goto doneRefreshing;
    }
  }
  if(!openOrders.isEmpty())
  {
    int64_t now = Time::time();
    if(now - lastOrderRefreshTime >= 120 * 1000)
    {
      lastOrderRefreshTime = now;
      refreshOrders(botSession);
    }
  }
doneRefreshing:

  cancelTimedOutOrders(botSession);

  botSession.handleTrade2(trade, 0);
}

void_t LiveBroker::refreshOrders(Bot::Session& botSession)
{
  List<meguco_user_broker_order_entity> orders;
  if(!main.getBrokerOrders(orders))
    return;
  Map<uint64_t, const meguco_user_broker_order_entity*> openOrderIds;
  for(List<meguco_user_broker_order_entity>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
  {
    const meguco_user_broker_order_entity& order = *i;
    openOrderIds.insert(order.entity.id, &order);
  }
  for(HashMap<uint64_t, Bot::Order>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const Bot::Order& order = *i;
    if(!openOrderIds.contains(order.id))
    {
      Bot::Transaction transaction;
      transaction.type = order.type == meguco_user_broker_order_buy ? meguco_user_broker_transaction_buy : meguco_user_broker_transaction_sell;
      transaction.price = order.price;
      transaction.amount = order.amount;
      transaction.total = order.total;
      main.createSessionTransaction2(transaction);
      transactions.append(transaction.id, transaction);

      if(order.type == meguco_user_broker_order_buy)
        lastBuyTime = time;
      else
        lastSellTime = time;

      main.removeSessionOrder(order.id);

      Bot::Marker marker;
      if(order.type == meguco_user_broker_order_buy)
      {
        marker.type = meguco_user_session_marker_buy;
        botSession.handleBuy2(order.id, transaction);
      }
      else
      {
        marker.type = meguco_user_session_marker_sell;
        botSession.handleSell2(order.id, transaction);
      }
      main.createSessionMarker2(marker);

      next = i; // update next since order list may have changed in bot session handler
      ++next;
      openOrders.remove(i);

      // update balance
      meguco_user_broker_balance_entity marketBalance;
      main.getBrokerBalance(marketBalance);
    }
  }
}

void_t LiveBroker::cancelTimedOutOrders(Bot::Session& botSession)
{
  for(HashMap<uint64_t, Bot::Order>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const Bot::Order& order = *i;
    if(order.timeout > 0 && time >= (int64_t)order.timeout)
    {
      if(main.removeBrokerOrder(order.id))
      {
        main.removeSessionOrder(order.id);

        if(order.type == meguco_user_broker_order_buy)
          botSession.handleBuyTimeout(order.id);
        else
          botSession.handleSellTimeout(order.id);

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

bool_t LiveBroker::buy(double price, double amount, double total, int64_t timeout, uint64_t* id, double* orderedAmount)
{
  Bot::Order order;
  order.type = meguco_user_broker_order_buy;
  order.price = price;
  order.amount = amount;
  order.total = total;
  if(!main.createBrokerOrder2(order))
  {
    error = main.getErrorString();
    return false;
  }
  lastOrderRefreshTime = Time::time();
  order.timeout = timeout > 0 ? time + timeout : 0;
  if(id)
    *id = order.id;
  if(orderedAmount)
    *orderedAmount = order.amount;

  main.createSessionOrder2(order);

  Bot::Marker marker;
  marker.type = meguco_user_session_marker_buy_attempt;
  main.createSessionMarker2(marker);

  openOrders.append(order.id, order);
  return true;
}

bool_t LiveBroker::sell(double price, double amount, double total, int64_t timeout, uint64_t* id, double* orderedAmount)
{
  Bot::Order order;
  order.type = meguco_user_broker_order_sell;
  order.price = price;
  order.amount = amount;
  order.total = total;
  if(!main.createBrokerOrder2(order))
  {
    error = main.getErrorString();
    return false;
  }
  lastOrderRefreshTime = Time::time();
  order.timeout = timeout > 0 ? time + timeout : 0;
  if(id)
    *id = order.id;
  if(orderedAmount)
    *orderedAmount = order.amount;

  main.createSessionOrder2(order);

  Bot::Marker marker;
  marker.type = meguco_user_session_marker_sell_attempt;
  main.createSessionMarker2(marker);

  openOrders.append(order.id, order);
  return true;
}

bool_t LiveBroker::cancelOder(uint64_t id)
{
  if(!main.removeBrokerOrder(id))
  {
    error = main.getErrorString();
    return false;
  }
  main.removeSessionOrder(id);
  openOrders.remove(id);
  return true;
}

const Bot::Order* LiveBroker::getOrder(uint64_t id) const
{
  HashMap<uint64_t, Bot::Order>::Iterator it = openOrders.find(id);
  if(it == openOrders.end())
    return 0;
  return &*it;
}

size_t LiveBroker::getOpenBuyOrderCount() const
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

size_t LiveBroker::getOpenSellOrderCount() const
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

const Bot::Asset* LiveBroker::getAsset(uint64_t id) const
{
  HashMap<uint64_t, Bot::Asset>::Iterator it = assets.find(id);
  if(it == assets.end())
    return 0;
  return &*it;
}

bool_t LiveBroker::createAsset2(Bot::Asset& asset)
{
  if(!main.createSessionAsset2(asset))
  {
    error = main.getErrorString();
    return false;
  }
  assets.append(asset.id, asset);
  return true;
}

void_t LiveBroker::removeAsset(uint64_t id)
{
  HashMap<uint64_t, Bot::Asset>::Iterator it = assets.find(id);
  if(it == assets.end())
    return;
  const Bot::Asset& asset = *it;
  main.removeSessionAsset(asset.id);
  assets.remove(it);
}

void_t LiveBroker::updateAsset2(const Bot::Asset& asset)
{
  HashMap<uint64_t, Bot::Asset>::Iterator it = assets.find(asset.id);
  if(it == assets.end())
    return;
  Bot::Asset& destAsset = *it;
  destAsset = asset;
  main.updateSessionAsset2(asset);
}

double LiveBroker::getProperty(const String& name, double defaultValue) const
{
  HashMap<String, Bot::Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
    return defaultValue;
  return (*it)->value.toDouble();
}

String LiveBroker::getProperty(const String& name, const String& defaultValue) const
{
  HashMap<String, Bot::Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
    return defaultValue;
  return (*it)->value;
}

void LiveBroker::registerProperty(const String& name, double value, uint32_t flags, const String& unit)
{
  registerProperty(name, String::fromDouble(value), meguco_user_session_property_number, flags, unit);
}

void LiveBroker::registerProperty(const String& name, const String& value, uint32_t flags, const String& unit)
{
  registerProperty(name, value, meguco_user_session_property_string, flags, unit);
}

void LiveBroker::registerProperty(const String& name, const String& value, meguco_user_session_property_type type, uint32_t flags, const String& unit)
{
  HashMap<String, Bot::Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
  {
    Bot::Property newProperty;
    newProperty.flags = flags;
    newProperty.type = type;
    newProperty.name = name;
    newProperty.unit = unit;
    newProperty.value = value;
    main.createSessionProperty(newProperty);
    Bot::Property& property = properties.append(newProperty.id, Bot::Property());
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

void LiveBroker::setProperty(const String& name, double value)
{
  setProperty(name, String::fromDouble(value));
}

void LiveBroker::setProperty(const String& name, const String& value)
{
  HashMap<String, Bot::Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
    return;
  Bot::Property& property = **it;
  property.value = value;
  main.updateSessionProperty(property);
}

void_t LiveBroker::addMarker(meguco_user_session_marker_type markerType)
{
  Bot::Marker marker;
  marker.type = markerType;
  main.createSessionMarker2(marker);
}

void_t LiveBroker::warning(const String& message)
{
  main.addLogMessage(Time::time(), message);
}

void_t LiveBroker::registerTransaction2(const Bot::Transaction& transaction)
{
  transactions.append(transaction.id, transaction);
}

void_t LiveBroker::registerOrder2(const Bot::Order& order)
{
  openOrders.append(order.id, order);
}

void_t LiveBroker::registerAsset2(const Bot::Asset& asset)
{
  assets.append(asset.id, asset);
}

void_t LiveBroker::unregisterAsset(uint64_t id)
{
  assets.remove(id);
}

void_t LiveBroker::registerProperty2(const Bot::Property& property)
{
  Bot::Property& newProperty = properties.append(property.id, property);
  propertiesByName.append(property.name, &newProperty);
}

const Bot::Property* LiveBroker::getProperty(uint64_t id)
{
  HashMap<uint64_t, Bot::Property>::Iterator it = properties.find(id);
  if(it == properties.end())
    return 0;
  return &*it;
}
