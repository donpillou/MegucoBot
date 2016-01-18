
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

void_t LiveBroker::handleTrade(Bot::Session& botSession, const meguco_trade_entity& trade, bool_t replayed)
{
  if(replayed)
  {
    int64_t tradeAge = Time::time() - trade.entity.time;
    if(tradeAge <= 0LL)
      tradeAge = 1LL;
    botSession.handleTrade(trade, tradeAge);
    return;
  }

  time = trade.entity.time;

  for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const meguco_user_broker_order_entity& order = *i;
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

  botSession.handleTrade(trade, 0);
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
  for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const meguco_user_broker_order_entity& order = *i;
    if(!openOrderIds.contains(order.entity.id))
    {
      meguco_user_broker_transaction_entity transaction;
      ZlimdbConnection::setEntityHeader(transaction.entity, 0, 0, sizeof(transaction));
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

      // update balance
      meguco_user_broker_balance_entity marketBalance;
      main.getBrokerBalance(marketBalance);
    }
  }
}

void_t LiveBroker::cancelTimedOutOrders(Bot::Session& botSession)
{
  for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const meguco_user_broker_order_entity& order = *i;
    if(order.timeout > 0 && time >= (int64_t)order.timeout)
    {
      if(main.removeBrokerOrder(order.entity.id))
      {
        main.removeSessionOrder(order.entity.id);

        if(order.type == meguco_user_broker_order_buy)
          botSession.handleBuyTimeout(order.entity.id);
        else
          botSession.handleSellTimeout(order.entity.id);

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
  meguco_user_broker_order_entity order;
  ZlimdbConnection::setEntityHeader(order.entity, 0, 0, sizeof(order));
  order.type = meguco_user_broker_order_buy;
  order.price = price;
  order.amount = amount;
  order.total = total;
  if(!main.createBrokerOrder(order))
  {
    error = main.getErrorString();
    return false;
  }
  lastOrderRefreshTime = Time::time();
  order.timeout = timeout > 0 ? time + timeout : 0;
  if(id)
    *id = order.entity.id;
  if(orderedAmount)
    *orderedAmount = order.amount;

#ifdef BOT_TESTBOT
  List<BotProtocol::Order> marketOrders;
  connectionHandler.getMarketOrders(marketOrders);
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

  main.createSessionOrder(order);

  meguco_user_session_marker_entity marker;
  ZlimdbConnection::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = meguco_user_session_marker_buy_attempt;
  main.createSessionMarker(marker);

  openOrders.append(order.entity.id, order);
  return true;
}

bool_t LiveBroker::sell(double price, double amount, double total, int64_t timeout, uint64_t* id, double* orderedAmount)
{
  meguco_user_broker_order_entity order;
  ZlimdbConnection::setEntityHeader(order.entity, 0, 0, sizeof(order));
  order.type = meguco_user_broker_order_sell;
  order.price = price;
  order.amount = amount;
  order.total = total;
  if(!main.createBrokerOrder(order))
  {
    error = main.getErrorString();
    return false;
  }
  lastOrderRefreshTime = Time::time();
  order.timeout = timeout > 0 ? time + timeout : 0;
  if(id)
    *id = order.entity.id;
  if(orderedAmount)
    *orderedAmount = order.amount;

#ifdef BOT_TESTBOT
  List<BotProtocol::Order> marketOrders;
  connectionHandler.getMarketOrders(marketOrders);
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
  
  main.createSessionOrder(order);

  meguco_user_session_marker_entity marker;
  ZlimdbConnection::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = meguco_user_session_marker_sell_attempt;
  main.createSessionMarker(marker);

  openOrders.append(order.entity.id, order);
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

const meguco_user_broker_order_entity* LiveBroker::getOrder(uint64_t id) const
{
  HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = openOrders.find(id);
  if(it == openOrders.end())
    return 0;
  return &*it;
}

size_t LiveBroker::getOpenBuyOrderCount() const
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

size_t LiveBroker::getOpenSellOrderCount() const
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

const meguco_user_session_asset_entity* LiveBroker::getAsset(uint64_t id) const
{
  HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator it = assets.find(id);
  if(it == assets.end())
    return 0;
  return &*it;
}

bool_t LiveBroker::createAsset(meguco_user_session_asset_entity& asset)
{
  if(!main.createSessionAsset(asset))
  {
    error = main.getErrorString();
    return false;
  }
  assets.append(asset.entity.id, asset);
  return true;
}

void_t LiveBroker::removeAsset(uint64_t id)
{
  HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator it = assets.find(id);
  if(it == assets.end())
    return;
  const meguco_user_session_asset_entity& asset = *it;
  main.removeSessionAsset(asset.entity.id);
  assets.remove(it);
}

void_t LiveBroker::updateAsset(const meguco_user_session_asset_entity& asset)
{
  HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator it = assets.find(asset.entity.id);
  if(it == assets.end())
    return;
  meguco_user_session_asset_entity& destAsset = *it;
  destAsset = asset;
  destAsset.entity.id = asset.entity.id;
  main.updateSessionAsset(destAsset);
}

//const BotProtocol::SessionProperty* LiveBroker::getProperty(uint32_t id) const
//{
//  for(HashMap<String, BotProtocol::SessionProperty>::Iterator i = properties.begin(), end = properties.end(); i != end; ++i)
//    if(i->entityId == id)
//      return &*i;
//  return 0;
//}
//
//void_t LiveBroker::updateProperty(const BotProtocol::SessionProperty& property)
//{
//  for(HashMap<String, BotProtocol::SessionProperty>::Iterator i = properties.begin(), end = properties.end(); i != end; ++i)
//    if(i->entityId == property.entityId)
//    {
//      connectionHandler.updateSessionProperty(property);
//      *i = property;
//    }
//}

double LiveBroker::getProperty(const String& name, double defaultValue) const
{
  HashMap<String, Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
    return defaultValue;
  return (*it)->value.toDouble();
}

String LiveBroker::getProperty(const String& name, const String& defaultValue) const
{
  HashMap<String, Property*>::Iterator it = propertiesByName.find(name);
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

void LiveBroker::setProperty(const String& name, double value)
{
  setProperty(name, String::fromDouble(value));
}

void LiveBroker::setProperty(const String& name, const String& value)
{
  HashMap<String, Property*>::Iterator it = propertiesByName.find(name);
  if(it == propertiesByName.end())
    return;
  Property& property = **it;
  main.updateSessionProperty(property.property, name, value, property.unit);
  property.value = value;
}

void_t LiveBroker::addMarker(meguco_user_session_marker_type markerType)
{
  meguco_user_session_marker_entity marker;
  ZlimdbConnection::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = markerType;
  main.createSessionMarker(marker);
}

void_t LiveBroker::warning(const String& message)
{
  main.addLogMessage(Time::time(), message);
}

void_t LiveBroker::registerTransaction(const meguco_user_broker_transaction_entity& transaction)
{
  transactions.append(transaction.entity.id, transaction);
}

void_t LiveBroker::registerOrder(const meguco_user_broker_order_entity& order)
{
  openOrders.append(order.entity.id, order);
}

void_t LiveBroker::registerAsset(const meguco_user_session_asset_entity& asset)
{
  assets.append(asset.entity.id, asset);
}

void_t LiveBroker::unregisterAsset(uint64_t id)
{
  assets.remove(id);
}

void_t LiveBroker::registerProperty(const meguco_user_session_property_entity& property, const String& name, const String& value, const String& unit)
{
  Property& newProperty = properties.append(property.entity.id, Property());
  newProperty.property = property;
  newProperty.name = name;
  newProperty.unit = unit;
  newProperty.value = value;
  propertiesByName.append(name, &newProperty);
}

const meguco_user_session_property_entity* LiveBroker::getProperty(uint64_t id, String& name, String& value, String& unit)
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
