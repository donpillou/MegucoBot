
#include <nstd/Time.h>
#include <nstd/Math.h>
#include <nstd/Map.h>

#include "Tools/ZlimdbProtocol.h"

#include "ConnectionHandler.h"
#include "LiveBroker.h"

LiveBroker::LiveBroker(ConnectionHandler& connectionHandler, const String& currencyBase, const String& currencyComm, double tradeFee, timestamp_t maxTradeAge) :
  connectionHandler(connectionHandler), currencyBase(currencyBase), currencyComm(currencyComm),
  time(0), lastBuyTime(0), lastSellTime(0), lastOrderRefreshTime(0) {}

void_t LiveBroker::handleTrade(Bot::Session& botSession, const meguco_trade_entity& trade, bool_t replayed)
{
  if(replayed)
  {
    timestamp_t tradeAge = Time::time() - trade.entity.time;
    if(tradeAge <= 0LL)
      tradeAge = 1LL;
    botSession.handleTrade(trade, tradeAge);
    return;
  }

  time = trade.entity.time;

  for(HashMap<uint64_t, meguco_user_market_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(); i != end; ++i)
  {
    const meguco_user_market_order_entity& order = *i;
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
  List<meguco_user_market_order_entity> orders;
  if(!connectionHandler.getMarketOrders(orders))
    return;
  Map<uint64_t, const meguco_user_market_order_entity*> openOrderIds;
  for(List<meguco_user_market_order_entity>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
  {
    const meguco_user_market_order_entity& order = *i;
    openOrderIds.insert(order.entity.id, &order);
  }
  for(HashMap<uint64_t, meguco_user_market_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const meguco_user_market_order_entity& order = *i;
    if(!openOrderIds.contains(order.entity.id))
    {
      meguco_user_market_transaction_entity transaction;
      ZlimdbProtocol::setEntityHeader(transaction.entity, 0, 0, sizeof(transaction));
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

      // update balance
      meguco_user_market_balance_entity marketBalance;
      connectionHandler.getMarketBalance(marketBalance);
    }
  }
}

void_t LiveBroker::cancelTimedOutOrders(Bot::Session& botSession)
{
  for(HashMap<uint64_t, meguco_user_market_order_entity>::Iterator i = openOrders.begin(), end = openOrders.end(), next; i != end; i = next)
  {
    next = i;
    ++next;

    const meguco_user_market_order_entity& order = *i;
    if(order.timeout > 0 && time >= (timestamp_t)order.timeout)
    {
      if(connectionHandler.removeMarketOrder(order.entity.id))
      {
        connectionHandler.removeSessionOrder(order.entity.id);

        if(order.type == meguco_user_market_order_buy)
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

bool_t LiveBroker::buy(double price, double amount, double total, timestamp_t timeout, uint64_t* id, double* orderedAmount)
{
  meguco_user_market_order_entity order;
  ZlimdbProtocol::setEntityHeader(order.entity, 0, 0, sizeof(order));
  order.type = meguco_user_market_order_buy;
  order.price = price;
  order.amount = amount;
  order.total = total;
  if(!connectionHandler.createMarketOrder(order))
  {
    error = connectionHandler.getLastError();
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

  connectionHandler.createSessionOrder(order);

  meguco_user_session_marker_entity marker;
  ZlimdbProtocol::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = meguco_user_session_marker_buy_attempt;
  connectionHandler.createSessionMarker(marker);

  openOrders.append(order.entity.id, order);
  return true;
}

bool_t LiveBroker::sell(double price, double amount, double total, timestamp_t timeout, uint64_t* id, double* orderedAmount)
{
  meguco_user_market_order_entity order;
  ZlimdbProtocol::setEntityHeader(order.entity, 0, 0, sizeof(order));
  order.type = meguco_user_market_order_sell;
  order.price = price;
  order.amount = amount;
  order.total = total;
  if(!connectionHandler.createMarketOrder(order))
  {
    error = connectionHandler.getLastError();
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
  
  connectionHandler.createSessionOrder(order);

  meguco_user_session_marker_entity marker;
  ZlimdbProtocol::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = meguco_user_session_marker_sell_attempt;
  connectionHandler.createSessionMarker(marker);

  openOrders.append(order.entity.id, order);
  return true;
}

bool_t LiveBroker::cancelOder(uint64_t id)
{
  if(!connectionHandler.removeMarketOrder(id))
  {
    error = connectionHandler.getLastError();
    return false;
  }
  connectionHandler.removeSessionOrder(id);
  openOrders.remove(id);
  return true;
}

const meguco_user_market_order_entity* LiveBroker::getOrder(uint64_t id) const
{
  HashMap<uint64_t, meguco_user_market_order_entity>::Iterator it = openOrders.find(id);
  if(it == openOrders.end())
    return 0;
  return &*it;
}

size_t LiveBroker::getOpenBuyOrderCount() const
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

size_t LiveBroker::getOpenSellOrderCount() const
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

const meguco_user_session_asset_entity* LiveBroker::getAsset(uint64_t id) const
{
  HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator it = assets.find(id);
  if(it == assets.end())
    return 0;
  return &*it;
}

bool_t LiveBroker::createAsset(meguco_user_session_asset_entity& asset)
{
  if(!connectionHandler.createSessionAsset(asset))
  {
    error = connectionHandler.getLastError();
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
  connectionHandler.removeSessionAsset(asset.entity.id);
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
  connectionHandler.updateSessionAsset(destAsset);
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
  HashMap<String, Buffer>::Iterator it = properties.find(name);
  if(it == properties.end())
    return defaultValue;
  meguco_user_session_property_entity* property = (meguco_user_session_property_entity*)(byte_t*)*it;
  String value;
  if(!ZlimdbProtocol::getString(property->entity, sizeof(*property) + property->name_size, property->value_size, value))
    return defaultValue;
  return value.toDouble();
}

String LiveBroker::getProperty(const String& name, const String& defaultValue) const
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

void LiveBroker::registerProperty(const String& name, double value, uint32_t flags, const String& unit)
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

void LiveBroker::registerProperty(const String& name, const String& value, uint32_t flags, const String& unit)
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

void LiveBroker::setProperty(const String& name, double value, uint32_t flags, const String& unit)
{
  setProperty(name, String::fromDouble(value), flags, unit);
}

void LiveBroker::setProperty(const String& name, const String& value, uint32_t flags, const String& unit)
{
  setProperty(name, String::fromDouble(value), flags, unit);
}

void LiveBroker::setProperty(const String& name, const String& value, meguco_user_session_property_type type, uint32_t flags, const String& unit)
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

void LiveBroker::removeProperty(const String& name)
{
  HashMap<String, Buffer>::Iterator it = properties.find(name);
  if(it == properties.end())
    return;
  uint64_t entityId  = ((meguco_user_session_property_entity*)(byte_t*)*it)->entity.id;
  connectionHandler.removeSessionProperty(entityId);
  properties.remove(it);
}

void_t LiveBroker::addMarker(meguco_user_session_marker_type markerType)
{
  meguco_user_session_marker_entity marker;
  ZlimdbProtocol::setEntityHeader(marker.entity, 0, time, sizeof(marker));
  marker.type = markerType;
  connectionHandler.createSessionMarker(marker);
}

void_t LiveBroker::warning(const String& message)
{
  connectionHandler.addLogMessage(Time::time(), message);
}
