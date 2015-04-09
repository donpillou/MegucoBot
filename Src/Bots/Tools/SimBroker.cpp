
#include <nstd/Debug.h>
#include <nstd/Time.h>
#include <nstd/Math.h>

#include "Tools/ZlimdbProtocol.h"

#include "ConnectionHandler.h"
#include "SimBroker.h"

SimBroker::SimBroker(ConnectionHandler& connectionHandler, const String& currencyBase, const String& currencyComm, double tradeFee, timestamp_t maxTradeAge) :
  connectionHandler(connectionHandler), currencyBase(currencyBase), currencyComm(currencyComm),
  time(0), lastBuyTime(0), lastSellTime(0), tradeFee(tradeFee), startTime(0), maxTradeAge(maxTradeAge) {}

void_t SimBroker::handleTrade(Bot::Session& botSession, const meguco_trade_entity& trade, bool_t replayed)
{
  if(startTime == 0)
    startTime = trade.entity.time;
  if((timestamp_t)(trade.entity.time - startTime) <= maxTradeAge)
  {
    botSession.handleTrade(trade, startTime + maxTradeAge - trade.entity.time);
    return; 
  }

  //if(trade.flags & DataProtocol::syncFlag)
  //  warning("sync");

  time = trade.entity.time;

  for(HashMap<uint64_t, meguco_user_market_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const meguco_user_market_order_entity& order = *i;
    if(order.timeout > 0 && time >= (timestamp_t)order.timeout)
    {
      connectionHandler.removeSessionOrder(order.entity.id);

      if(order.type == meguco_user_market_order_buy)
        botSession.handleBuyTimeout(order.entity.id);
      else
        botSession.handleSellTimeout(order.entity.id);

      next = i; // update next since order list may have changed in bot session handler
      ++next;
      openOrders.remove(i);
    }
    else if((order.type == meguco_user_market_order_buy && trade.price < order.price) ||
            ( order.type == meguco_user_market_order_sell && trade.price > order.price) )
    {
      meguco_user_market_transaction_entity transaction;
      ZlimdbProtocol::setEntityHeader(transaction.entity, 0, time, sizeof(transaction));
      transaction.type = order.type == meguco_user_market_order_buy ? meguco_user_market_transaction_buy : meguco_user_market_transaction_sell;
      transaction.price = order.price;
      transaction.amount = order.amount;
      transaction.total = order.total;
      connectionHandler.createSessionTransaction(transaction);
      transactions.append(transaction.entity.id, transaction);

      if(order.type == meguco_user_market_order_buy)
        lastBuyTime = time;
      else
        lastSellTime = time;

      connectionHandler.removeSessionOrder(order.entity.id);

      meguco_user_session_marker_entity marker;
      ZlimdbProtocol::setEntityHeader(marker.entity, 0, time, sizeof(marker));
      if(order.type == meguco_user_market_order_buy)
      {
        marker.type = meguco_user_session_marker_buy;
        botSession.handleBuy(order.entity.id, transaction);
      }
      else
      {
        marker.type = meguco_user_session_marker_sell;
        botSession.handleSell(order.entity.id, transaction);
      }
      connectionHandler.createSessionMarker(marker);

      next = i; // update next since order list may have changed in bot session handler
      ++next;
      openOrders.remove(i);
    }
  }

  botSession.handleTrade(trade, 0);
}

bool_t SimBroker::buy(double price, double amount, double total, timestamp_t timeout, uint64_t* id, double* orderedAmount)
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

  meguco_user_market_order_entity order;
  ZlimdbProtocol::setEntityHeader(order.entity, 0, time, sizeof(order));
  order.type = meguco_user_market_order_buy;
  order.amount = amount;
  order.price = price;
  order.total = total;
  timestamp_t orderTimeout = timeout > 0 ? time + timeout : 0;
  order.timeout = orderTimeout;
  if(!connectionHandler.createSessionOrder(order))
  {
    error = connectionHandler.getLastError();
    return false;
  }
  ASSERT(order.timeout == orderTimeout);
  if(id)
    *id = order.entity.id;
  if(orderedAmount)
    *orderedAmount = order.amount;

  meguco_user_session_marker_entity marker;
  ZlimdbProtocol::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = meguco_user_session_marker_buy_attempt;
  connectionHandler.createSessionMarker(marker);

  openOrders.append(order.entity.id, order);
  return true;
}

bool_t SimBroker::sell(double price, double amount, double total, timestamp_t timeout, uint64_t* id, double* orderedAmount)
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

  meguco_user_market_order_entity order;
  ZlimdbProtocol::setEntityHeader(order.entity, 0, time, sizeof(order));
  order.type = meguco_user_market_order_sell;
  order.amount = amount;
  order.price = price;
  order.total = Math::floor(price * amount * (1 - tradeFee) * 100.) / 100.;
  timestamp_t orderTimeout = timeout > 0 ? time + timeout : 0;
  order.timeout = orderTimeout;
  if(!connectionHandler.createSessionOrder(order))
  {
    error = connectionHandler.getLastError();
    return false;
  }
  ASSERT(order.timeout == orderTimeout);
  if(id)
    *id = order.entity.id;
  if(orderedAmount)
    *orderedAmount = order.amount;

  meguco_user_session_marker_entity marker;
  ZlimdbProtocol::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = meguco_user_session_marker_sell_attempt;
  connectionHandler.createSessionMarker(marker);

  openOrders.append(order.entity.id, order);
  return true;
}

bool_t SimBroker::cancelOder(uint64_t id)
{
  if(!connectionHandler.removeSessionOrder(id))
  {
    error = connectionHandler.getLastError();
    return false;
  }
  openOrders.remove(id);
  return true;
}

const meguco_user_market_order_entity* SimBroker::getOrder(uint64_t id) const
{
  HashMap<uint64_t, meguco_user_market_order_entity>::Iterator it = openOrders.find(id);
  if(it == openOrders.end())
    return 0;
  return &*it;
}

size_t SimBroker::getOpenBuyOrderCount() const
{
  size_t openBuyOrders = 0;
  for(HashMap<uint64_t, meguco_user_market_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const meguco_user_market_order_entity& order = *i;
    if(order.type == meguco_user_market_order_buy)
      ++openBuyOrders;
  }
  return openBuyOrders;
}

size_t SimBroker::getOpenSellOrderCount() const
{
  size_t openSellOrders = 0;
  for(HashMap<uint64_t, meguco_user_market_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const meguco_user_market_order_entity& order = *i;
    if(order.type == meguco_user_market_order_sell)
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
  if(!connectionHandler.createSessionAsset(asset))
  {
    error = connectionHandler.getLastError();
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
  connectionHandler.removeSessionAsset(asset.entity.id);
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
  connectionHandler.updateSessionAsset(destAsset);
}

//const Bot::Broker::SessionProperty* SimBroker::getProperty(uint64_t id) const
//{
//  for(HashMap<String, SessionProperty>::Iterator i = properties.begin(), end = properties.end(); i != end; ++i)
//    if(i->entity.id == id)
//      return &*i;
//  return 0;
//}

//void_t SimBroker::updateProperty(const SessionProperty& property)
//{
//  for(HashMap<String, SessionProperty>::Iterator i = properties.begin(), end = properties.end(); i != end; ++i)
//    if(i->property.entity.id == property.property.entity.id)
//    {
//      connectionHandler.updateSessionProperty(property.property, property.name, property.value, property.unit);
//      *i = property;
//    }
//}

double SimBroker::getProperty(const String& name, double defaultValue) const
{
  HashMap<String, Buffer>::Iterator it = properties.find(name);
  if(it == properties.end())
    return defaultValue;
  meguco_user_session_property_entity* property = (meguco_user_session_property_entity*)(byte_t*)*it;
  String value;
  if(!ZlimdbProtocol::getString(property->entity, sizeof(*property) + property->name_size, property->value_size, value))
    return defaultValue;
  return value.toDouble();
}

String SimBroker::getProperty(const String& name, const String& defaultValue) const
{
  HashMap<String, Buffer>::Iterator it = properties.find(name);
  if(it == properties.end())
    return defaultValue;
  meguco_user_session_property_entity* property = (meguco_user_session_property_entity*)(byte_t*)*it;
  String value;
  if(!ZlimdbProtocol::getString(property->entity, sizeof(*property) + property->name_size, property->value_size, value))
    return defaultValue;
  return value;
}

void SimBroker::registerProperty(const String& name, double value, uint32_t flags, const String& unit)
{
  HashMap<String, Buffer>::Iterator it = properties.find(name);
  if(it == properties.end())
    setProperty(name, value, flags, unit);
  else
  {
    meguco_user_session_property_entity* property = (meguco_user_session_property_entity*)(byte_t*)*it;
    String oldUnit;
    ZlimdbProtocol::getString(property->entity, sizeof(*property) + property->name_size + property->value_size, property->unit_size, oldUnit);
    if(property->flags = flags || oldUnit != unit)
    {
      String value;
      ZlimdbProtocol::getString(property->entity, sizeof(*property) + property->name_size, property->value_size, value);
      setProperty(name, value, flags, unit);
    }
  }
}

void SimBroker::registerProperty(const String& name, const String& value, uint32_t flags, const String& unit)
{
  HashMap<String, Buffer>::Iterator it = properties.find(name);
  if(it == properties.end())
    setProperty(name, value, flags, unit);
  else
  {
    meguco_user_session_property_entity* property = (meguco_user_session_property_entity*)(byte_t*)*it;
    String oldUnit;
    ZlimdbProtocol::getString(property->entity, sizeof(*property) + property->name_size + property->value_size, property->unit_size, oldUnit);
    if(property->flags = flags || oldUnit != unit)
    {
      String value;
      ZlimdbProtocol::getString(property->entity, sizeof(*property) + property->name_size, property->value_size, value);
      setProperty(name, value, flags, unit);
    }
  }
}

void SimBroker::setProperty(const String& name, double value, uint32_t flags, const String& unit)
{
  setProperty(name, String::fromDouble(value), flags, unit);
}

void SimBroker::setProperty(const String& name, const String& value, uint32_t flags, const String& unit)
{
  setProperty(name, String::fromDouble(value), flags, unit);
}

void SimBroker::setProperty(const String& name, const String& value, meguco_user_session_property_type type, uint32_t flags, const String& unit)
{
  HashMap<String, Buffer>::Iterator it = properties.find(name);
  if(it == properties.end())
  {
    Buffer& buffer = properties.append(name, Buffer());
    buffer.resize(sizeof(meguco_user_session_property_entity) + name.length() + value.length() + unit.length());
    meguco_user_session_property_entity* property = (meguco_user_session_property_entity*)(byte_t*)buffer;
    ZlimdbProtocol::setEntityHeader(property->entity, 0, 0, sizeof(*property) + name.length() + value.length() + unit.length());
    property->flags = flags;
    property->type = type;
    ZlimdbProtocol::setString(property->entity, property->name_size, sizeof(*property), name);
    ZlimdbProtocol::setString(property->entity, property->value_size, sizeof(*property) + name.length(), value);
    ZlimdbProtocol::setString(property->entity, property->unit_size, sizeof(*property) + name.length() + value.length(), unit);
    connectionHandler.createSessionProperty(*property);
  }
  else
  {
    uint64_t entityId  = ((meguco_user_session_property_entity*)(byte_t*)*it)->entity.id;
    Buffer& buffer = *it;
    buffer.resize(sizeof(meguco_user_session_property_entity) + name.length() + value.length() + unit.length());
    meguco_user_session_property_entity* property = (meguco_user_session_property_entity*)(byte_t*)buffer;
    ZlimdbProtocol::setEntityHeader(property->entity, entityId, 0, sizeof(*property) + name.length() + value.length() + unit.length());
    property->flags = flags;
    property->type = type;
    ZlimdbProtocol::setString(property->entity, property->name_size, sizeof(*property), name);
    ZlimdbProtocol::setString(property->entity, property->value_size, sizeof(*property) + name.length(), value);
    ZlimdbProtocol::setString(property->entity, property->unit_size, sizeof(*property) + name.length() + value.length(), unit);
    connectionHandler.updateSessionProperty(*property);
  }
}

void SimBroker::removeProperty(const String& name)
{
  HashMap<String, Buffer>::Iterator it = properties.find(name);
  if(it == properties.end())
    return;
  uint64_t entityId  = ((meguco_user_session_property_entity*)(byte_t*)*it)->entity.id;
  connectionHandler.removeSessionProperty(entityId);
  properties.remove(it);
}

void_t SimBroker::addMarker(meguco_user_session_marker_type markerType)
{
  meguco_user_session_marker_entity marker;
  ZlimdbProtocol::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = markerType;
  connectionHandler.createSessionMarker(marker);
}

void_t SimBroker::warning(const String& message)
{
  connectionHandler.addLogMessage(time, message);
}
