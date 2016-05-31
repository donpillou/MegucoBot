
#include <nstd/Debug.h>
#include <nstd/Time.h>
#include <nstd/Math.h>

#include "SimBroker.h"
#include "Main.h"

SimBroker::SimBroker(Main& main, const String& currencyBase, const String& currencyComm, double tradeFee, int64_t maxTradeAge) :
  main(main), currencyBase(currencyBase), currencyComm(currencyComm),
  time(0), lastBuyTime(0), lastSellTime(0), tradeFee(tradeFee), startTime(0), maxTradeAge(maxTradeAge) {}

void_t SimBroker::handleTrade(Bot::Session& botSession, const meguco_trade_entity& trade, bool_t replayed)
{
  if(startTime == 0)
    startTime = trade.entity.time;
  if((int64_t)(trade.entity.time - startTime) <= maxTradeAge)
  {
    botSession.handleTrade(trade, startTime + maxTradeAge - trade.entity.time);
    return; 
  }

  //if(trade.flags & DataProtocol::syncFlag)
  //  warning("sync");

  time = trade.entity.time;

  for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const meguco_user_broker_order_entity& order = *i;
    if(order.timeout > 0 && time >= (int64_t)order.timeout)
    {
      main.removeSessionOrder(order.entity.id);

      if(order.type == meguco_user_broker_order_buy)
        botSession.handleBuyTimeout(order.entity.id);
      else
        botSession.handleSellTimeout(order.entity.id);

      next = i; // update next since order list may have changed in bot session handler
      ++next;
      openOrders.remove(i);
    }
    else if((order.type == meguco_user_broker_order_buy && trade.price < order.price) ||
            ( order.type == meguco_user_broker_order_sell && trade.price > order.price) )
    {
      meguco_user_broker_transaction_entity transaction;
      ZlimdbConnection::setEntityHeader(transaction.entity, 0, time, sizeof(transaction));
      transaction.type = order.type == meguco_user_broker_order_buy ? meguco_user_broker_transaction_buy : meguco_user_broker_transaction_sell;
      transaction.price = order.price;
      transaction.amount = order.amount;
      transaction.total = order.total;
      main.createSessionTransaction(transaction);
      transactions.append(transaction.entity.id, transaction);

      if(order.type == meguco_user_broker_order_buy)
        lastBuyTime = time;
      else
        lastSellTime = time;

      main.removeSessionOrder(order.entity.id);

      meguco_user_session_marker_entity marker;
      ZlimdbConnection::setEntityHeader(marker.entity, 0, time, sizeof(marker));
      if(order.type == meguco_user_broker_order_buy)
      {
        marker.type = meguco_user_session_marker_buy;
        botSession.handleBuy(order.entity.id, transaction);
      }
      else
      {
        marker.type = meguco_user_session_marker_sell;
        botSession.handleSell(order.entity.id, transaction);
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

  meguco_user_broker_order_entity order;
  ZlimdbConnection::setEntityHeader(order.entity, 0, time, sizeof(order));
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
    *id = order.entity.id;
  if(orderedAmount)
    *orderedAmount = order.amount;

  meguco_user_session_marker_entity marker;
  ZlimdbConnection::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = meguco_user_session_marker_buy_attempt;
  main.createSessionMarker(marker);

  openOrders.append(order.entity.id, order);
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

  meguco_user_broker_order_entity order;
  ZlimdbConnection::setEntityHeader(order.entity, 0, time, sizeof(order));
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
    *id = order.entity.id;
  if(orderedAmount)
    *orderedAmount = order.amount;

  meguco_user_session_marker_entity marker;
  ZlimdbConnection::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = meguco_user_session_marker_sell_attempt;
  main.createSessionMarker(marker);

  openOrders.append(order.entity.id, order);
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

const meguco_user_broker_order_entity* SimBroker::getOrder(uint64_t id) const
{
  HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = openOrders.find(id);
  if(it == openOrders.end())
    return 0;
  return &*it;
}

size_t SimBroker::getOpenBuyOrderCount() const
{
  size_t openBuyOrders = 0;
  for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const meguco_user_broker_order_entity& order = *i;
    if(order.type == meguco_user_broker_order_buy)
      ++openBuyOrders;
  }
  return openBuyOrders;
}

size_t SimBroker::getOpenSellOrderCount() const
{
  size_t openSellOrders = 0;
  for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const meguco_user_broker_order_entity& order = *i;
    if(order.type == meguco_user_broker_order_sell)
      ++openSellOrders;
  }
  return openSellOrders;
}

const meguco_user_session_asset_entity* SimBroker::getAsset(uint64_t id) const
{
  HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator it = assets.find(id);
  if(it == assets.end())
    return 0;
  return &*it;
}

bool_t SimBroker::createAsset(meguco_user_session_asset_entity& asset)
{
  if(!main.createSessionAsset(asset))
  {
    error = main.getErrorString();
    return false;
  }
  assets.append(asset.entity.id, asset);
  return true;
}

void_t SimBroker::removeAsset(uint64_t id)
{
  HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator it = assets.find(id);
  if(it == assets.end())
    return;
  const meguco_user_session_asset_entity& asset = *it;
  main.removeSessionAsset(asset.entity.id);
  assets.remove(it);
}

void_t SimBroker::updateAsset(const meguco_user_session_asset_entity& asset)
{
  HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator it = assets.find(asset.entity.id);
  if(it == assets.end())
    return;
  meguco_user_session_asset_entity& destAsset = *it;
  destAsset = asset;
  destAsset.entity.id = asset.entity.id;
  main.updateSessionAsset(destAsset);
}

double SimBroker::getProperty(const String& name, double defaultValue) const
{
  HashMap<String, Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
    return defaultValue;
  return (*it)->value.toDouble();
}

String SimBroker::getProperty(const String& name, const String& defaultValue) const
{
  HashMap<String, Property*>::Iterator it = propertiesByName.find(name);
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
  HashMap<String, Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
  {
    meguco_user_session_property_entity newProperty;
    ZlimdbConnection::setEntityHeader(newProperty.entity, 0, 0, sizeof(meguco_user_session_property_entity));
    newProperty.flags = flags;
    newProperty.type = type;
    main.createSessionProperty(newProperty, name, value, unit);
    Property& property = properties.append(newProperty.entity.id, Property());
    property.property = newProperty;
    property.name = name;
    property.unit = unit;
    property.value = value;
    propertiesByName.append(name, &property);
  }
  else
  {
    Property& property = **it;
    if(flags != property.property.flags || type != property.property.type || property.unit != unit)
    { // update property attributes, but keep value
      property.property.flags = flags;
      property.property.type = type;
      main.updateSessionProperty(property.property, name, property.value, unit);
      property.unit = unit;
    }
  }
}

void SimBroker::setProperty(const String& name, double value)
{
  setProperty(name, String::fromDouble(value));
}

void SimBroker::setProperty(const String& name, const String& value)
{
  HashMap<String, Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
    return;
  Property& property = **it;
  main.updateSessionProperty(property.property, name, value, property.unit);
  property.value = value;
}

void_t SimBroker::addMarker(meguco_user_session_marker_type markerType)
{
  meguco_user_session_marker_entity marker;
  ZlimdbConnection::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = markerType;
  main.createSessionMarker(marker);
}

void_t SimBroker::warning(const String& message)
{
  main.addLogMessage(time, message);
}

void_t SimBroker::registerTransaction(const meguco_user_broker_transaction_entity& transaction)
{
  transactions.append(transaction.entity.id, transaction);
}

void_t SimBroker::registerOrder(const meguco_user_broker_order_entity& order)
{
  openOrders.append(order.entity.id, order);
}

void_t SimBroker::registerAsset(const meguco_user_session_asset_entity& asset)
{
  assets.append(asset.entity.id, asset);
}

void_t SimBroker::unregisterAsset(uint64_t id)
{
  assets.remove(id);
}

void_t SimBroker::registerProperty(const meguco_user_session_property_entity& property, const String& name, const String& value, const String& unit)
{
  Property& newProperty = properties.append(property.entity.id, Property());
  newProperty.property = property;
  newProperty.name = name;
  newProperty.unit = unit;
  newProperty.value = value;
  propertiesByName.append(name, &newProperty);
}

const meguco_user_session_property_entity* SimBroker::getProperty(uint64_t id, String& name, String& value, String& unit)
{
  HashMap<uint64_t, Property>::Iterator it = properties.find(id);
  if(it == properties.end())
    return 0;
  Property& property = *it;
  name = property.name;
  value = property.value;
  unit = property.unit;
  return &property.property;
}
