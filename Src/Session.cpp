
#include <nstd/Time.h>
#include <nstd/Math.h>

#include "Session.h"
#include "ServerHandler.h"
#include "ClientHandler.h"
#include "BotEngine.h"
#include "MarketAdapter.h"
#include "User.h"
#include "Market.h"

Session::Session(ServerHandler& serverHandler, User& user, uint32_t id, const String& name, BotEngine& engine, Market& market) :
  serverHandler(serverHandler), user(user),
  __id(id), name(name), engine(&engine), market(&market), simulation(true),
  state(BotProtocol::Session::stopped), pid(0), handlerClient(0), entityClient(0), nextEntityId(1)
{
  market.registerSession(*this);
}

Session::Session(ServerHandler& serverHandler, User& user, const Variant& variant) :
  serverHandler(serverHandler), user(user),
  state(BotProtocol::Session::stopped), pid(0), handlerClient(0), entityClient(0), nextEntityId(1)
{
  const HashMap<String, Variant>& data = variant.toMap();
  __id = data.find("id")->toUInt();
  name = data.find("name")->toString();
  engine = serverHandler.findBotEngine(data.find("engine")->toString());
  market = user.findMarket(data.find("marketId")->toUInt());
  {
    const List<Variant>& transactionsVar = data.find("transactions")->toList();
    BotProtocol::Transaction transaction;
    transaction.entityType = BotProtocol::sessionTransaction;
    for(List<Variant>::Iterator i = transactionsVar.begin(), end = transactionsVar.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& transactionVar = i->toMap();
      transaction.entityId = transactionVar.find("id")->toUInt();
      transaction.type = transactionVar.find("type")->toUInt();
      transaction.date = transactionVar.find("date")->toInt64();
      transaction.price = transactionVar.find("price")->toDouble();
      transaction.amount = transactionVar.find("amount")->toDouble();
      transaction.total = transactionVar.find("total")->toDouble();

      if(transactions.find(transaction.entityId) != transactions.end())
        continue;
      transactions.append(transaction.entityId, transaction);
      if(transaction.entityId >= nextEntityId)
        nextEntityId = transaction.entityId + 1;
    }
  }
  {
    const List<Variant>& itemsVar = data.find("assets")->toList();
    BotProtocol::SessionAsset asset;
    asset.entityType = BotProtocol::sessionAsset;
    for(List<Variant>::Iterator i = itemsVar.begin(), end = itemsVar.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& assetVar = i->toMap();
      asset.entityId = assetVar.find("id")->toUInt();
      asset.type = assetVar.find("type")->toUInt();
      asset.state = assetVar.find("state")->toUInt();
      asset.date = assetVar.find("date")->toInt64();
      asset.price = assetVar.find("price")->toDouble();
      asset.investComm = assetVar.find("investComm")->toDouble();
      asset.investBase = assetVar.find("investBase")->toDouble();
      asset.balanceComm = assetVar.find("balanceComm")->toDouble();
      asset.balanceBase = assetVar.find("balanceBase")->toDouble();
      asset.profitablePrice = assetVar.find("profitablePrice")->toDouble();
      asset.flipPrice = assetVar.find("flipPrice")->toDouble();
      asset.orderId = assetVar.find("orderId")->toUInt();

      if(asset.investComm == 0. && asset.investBase == 0.) // todo: this is compatibilty loading code; remove this
      {
        if(asset.state == BotProtocol::SessionAsset::waitBuy || asset.state == BotProtocol::SessionAsset::buying)
          asset.investComm = Math::ceil(asset.balanceBase / asset.price * (1 + 0.005) * 100000000.) / 100000000.;
        if(asset.state == BotProtocol::SessionAsset::waitSell || asset.state == BotProtocol::SessionAsset::selling)
          asset.investBase = Math::ceil(asset.balanceComm * asset.price * (1 + 0.005) * 100.) / 100.;
      }

      if(assets.find(asset.entityId) != assets.end())
        continue;
      assets.append(asset.entityId, asset);
      if(asset.entityId >= nextEntityId)
        nextEntityId = asset.entityId + 1;
    }
  }
  {
    const List<Variant>& propertiesVar = data.find("properties")->toList();
    BotProtocol::SessionProperty property;
    property.entityType = BotProtocol::sessionProperty;
    for(List<Variant>::Iterator i = propertiesVar.begin(), end = propertiesVar.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& propertyVar = i->toMap();
      property.entityId = propertyVar.find("id")->toUInt();
      property.type = propertyVar.find("type")->toUInt();
      property.flags = propertyVar.find("flags")->toUInt();
      BotProtocol::setString(property.name, propertyVar.find("name")->toString());
      BotProtocol::setString(property.value, propertyVar.find("value")->toString());
      BotProtocol::setString(property.unit, propertyVar.find("unit")->toString());
      if(properties.find(property.entityId) != properties.end())
        continue;
      properties.append(property.entityId, property);
      if(property.entityId >= nextEntityId)
        nextEntityId = property.entityId + 1;
    }
  }
  {
    const List<Variant>& ordersVar = data.find("orders")->toList();
    BotProtocol::Order order;
    order.entityType = BotProtocol::sessionOrder;
    for(List<Variant>::Iterator i = ordersVar.begin(), end = ordersVar.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& orderVar = i->toMap();
      order.entityId = orderVar.find("id")->toUInt();
      order.type = orderVar.find("type")->toUInt();
      order.date = orderVar.find("date")->toInt64();
      order.price = orderVar.find("price")->toDouble();
      order.amount = orderVar.find("amount")->toDouble();
      order.total = orderVar.find("total")->toDouble();

      if(orders.find(order.entityId) != orders.end())
        continue;
      orders.append(order.entityId, order);
      if(order.entityId >= nextEntityId)
        nextEntityId = order.entityId + 1;
    }
  }
  {
    const List<Variant>& markersVar = data.find("markers")->toList();
    BotProtocol::Marker marker;
    marker.entityType = BotProtocol::sessionMarker;
    for(List<Variant>::Iterator i = markersVar.begin(), end = markersVar.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& markerVar = i->toMap();
      marker.entityId = markerVar.find("id")->toUInt();
      marker.type = markerVar.find("type")->toUInt();
      marker.date = markerVar.find("date")->toInt64();

      if(markers.find(marker.entityId) != markers.end())
        continue;
      markers.append(marker.entityId, marker);
      if(marker.entityId >= nextEntityId)
        nextEntityId = marker.entityId + 1;
    }
  }
  {
    const List<Variant>& logMessagesVar = data.find("logMessages")->toList();
    BotProtocol::SessionLogMessage logMessage;
    logMessage.entityType = BotProtocol::sessionLogMessage;
    logMessage.entityId = 0;
    for(List<Variant>::Iterator i = logMessagesVar.begin(), end = logMessagesVar.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& logMessageVar = i->toMap();
      logMessage.date = logMessageVar.find("date")->toInt64();
      BotProtocol::setString(logMessage.message, logMessageVar.find("message")->toString());
      logMessages.append(logMessage);
    }
  }

  if(market)
    market->registerSession(*this);
}

Session::~Session()
{
  if(pid != 0)
    serverHandler.unregisterSession(pid);
  process.kill();
  if(handlerClient)
    handlerClient->deselectSession();
  if(entityClient)
    entityClient->deselectSession();
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->deselectSession();
  if(market)
    market->unregisterSession(*this);
}

void_t Session::toVariant(Variant& variant)
{
  HashMap<String, Variant>& data = variant.toMap();
  data.append("id", __id);
  data.append("name", name);
  data.append("engine", engine->getName());
  data.append("marketId", market->getId());
  {
    List<Variant>& transactionsVar = data.append("transactions", Variant()).toList();
    for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
    {
      const BotProtocol::Transaction& transaction = *i;
      HashMap<String, Variant>& transactionVar = transactionsVar.append(Variant()).toMap();
      transactionVar.append("id", transaction.entityId);
      transactionVar.append("type", (uint32_t)transaction.type);
      transactionVar.append("date", transaction.date);
      transactionVar.append("price", transaction.price);
      transactionVar.append("amount", transaction.amount);
      transactionVar.append("total", transaction.total);
    }
  }
  {
    List<Variant>& itemsVar = data.append("assets", Variant()).toList();
    for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
    {
      const BotProtocol::SessionAsset& asset = *i;
      HashMap<String, Variant>& assetVar = itemsVar.append(Variant()).toMap();
      assetVar.append("id", asset.entityId);
      assetVar.append("type", (uint32_t)asset.type);
      assetVar.append("state", (uint32_t)asset.state);
      assetVar.append("date", asset.date);
      assetVar.append("price", asset.price);
      assetVar.append("investComm", asset.investComm);
      assetVar.append("investBase", asset.investBase);
      assetVar.append("balanceComm", asset.balanceComm);
      assetVar.append("balanceBase", asset.balanceBase);
      assetVar.append("profitablePrice", asset.profitablePrice);
      assetVar.append("flipPrice", asset.flipPrice);
      assetVar.append("orderId", asset.orderId);
    }
  }
  {
    List<Variant>& propertiesVar = data.append("properties", Variant()).toList();
    for(HashMap<uint32_t, BotProtocol::SessionProperty>::Iterator i = properties.begin(), end = properties.end(); i != end; ++i)
    {
      BotProtocol::SessionProperty& property = *i;
      HashMap<String, Variant>& propertyVar = propertiesVar.append(Variant()).toMap();
      propertyVar.append("id", property.entityId);
      propertyVar.append("type", (uint32_t)property.type);
      propertyVar.append("flags", (uint32_t)property.flags);
      propertyVar.append("name", BotProtocol::getString(property.name));
      propertyVar.append("value", BotProtocol::getString(property.value));
      propertyVar.append("unit", BotProtocol::getString(property.unit));
    }
  }
  {
    List<Variant>& ordersVar = data.append("orders", Variant()).toList();
    for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
    {
      const BotProtocol::Order& order = *i;
      HashMap<String, Variant>& orderVar = ordersVar.append(Variant()).toMap();
      orderVar.append("id", order.entityId);
      orderVar.append("type", (uint32_t)order.type);
      orderVar.append("date", order.date);
      orderVar.append("price", order.price);
      orderVar.append("amount", order.amount);
      orderVar.append("total", order.total);
    }
  }
  {
    List<Variant>& markersVar = data.append("markers", Variant()).toList();
    for(HashMap<uint32_t, BotProtocol::Marker>::Iterator i = markers.begin(), end = markers.end(); i != end; ++i)
    {
      const BotProtocol::Marker& marker = *i;
      if(marker.type == BotProtocol::Marker::buy || marker.type == BotProtocol::Marker::sell)
      {
        HashMap<String, Variant>& markerVar = markersVar.append(Variant()).toMap();
        markerVar.append("id", marker.entityId);
        markerVar.append("type", marker.type);
        markerVar.append("date", marker.date);
      }
    }
  }
  {
    List<Variant>& logMessagesVar = data.append("logMessages", Variant()).toList();
    for(List<BotProtocol::SessionLogMessage>::Iterator i = logMessages.begin(), end = logMessages.end(); i != end; ++i)
    {
      BotProtocol::SessionLogMessage& logMessage = *i;
      HashMap<String, Variant>& logMessageVar = logMessagesVar.append(Variant()).toMap();
      logMessageVar.append("date", logMessage.date);
      logMessageVar.append("message", BotProtocol::getString(logMessage.message));
    }
  }
}

bool_t Session::saveData()
{
  if(simulation)
    return true;
  return user.saveData();
}

bool_t Session::startSimulation()
{
  if(pid != 0)
    return false;
  simulation = true;
  pid = process.start(engine->getPath());
  if(!pid)
    return false;
  serverHandler.registerSession(pid, *this);
  state = BotProtocol::Session::starting;

  // clear and save backup of orders, transactions, log messages, and markers
  backupTransactions.swap(transactions);
  backupAssets.swap(assets);
  backupProperties.swap(properties);
  backupOrders.swap(orders);
  backupMarkers.swap(markers);
  backupLogMessages.swap(logMessages);

  // but, keep items and properties
  assets = backupAssets;
  properties = backupProperties;

  return true;
}

bool_t Session::startLive()
{
  if(pid != 0)
    return false;
  simulation = false;
  pid = process.start(engine->getPath());
  if(!pid)
    return false;
  serverHandler.registerSession(pid, *this);
  state = BotProtocol::Session::starting;
  return true;
}

bool_t Session::stop()
{
  if(pid == 0)
    return false;
  if(!process.kill())
    return false;
  pid = 0;
  if(simulation)
  {
    simulation = false;

    // restore backup of orders, transactions, log messages, and markers
    backupTransactions.swap(transactions);
    backupAssets.swap(assets);
    backupProperties.swap(properties);
    backupOrders.swap(orders);
    backupMarkers.swap(markers);
    backupLogMessages.swap(logMessages);

    backupTransactions.clear();
    backupAssets.clear();
    backupProperties.clear();
    backupOrders.clear();
    backupMarkers.clear();
    backupLogMessages.clear();
  }
  state = BotProtocol::Session::stopped;
  return true;
}

bool_t Session::registerClient(ClientHandler& client, ClientType type)
{
  switch(type)
  {
  case handlerType:
    if(handlerClient)
      return false;
    handlerClient = &client;
    state = simulation ? BotProtocol::Session::simulating : BotProtocol::Session::running;
    break;
  case entityType:
    if(entityClient)
      return false;
    entityClient = &client;
    break;
  case userType:
    clients.append(&client);
    break;
  }
  return true;
}

void_t Session::unregisterClient(ClientHandler& client)
{
  if(&client == handlerClient)
  {
    handlerClient = 0;
    stop();
  }
  else if(&client == entityClient)
    entityClient = 0;
  else
    clients.remove(&client);
}

BotProtocol::Transaction& Session::createTransaction(const BotProtocol::Transaction& transaction)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::Transaction& result = transactions.append(entityId, transaction);
  result.entityId = entityId;
  return result;
}

bool_t Session::deleteTransaction(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Transaction>::Iterator it = transactions.find(id);
  if(it == transactions.end())
    return false;
  transactions.remove(it);
  return true;
}

const BotProtocol::SessionAsset* Session::getAsset(uint32_t id) const
{
  HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator it = assets.find(id);
  if(it == assets.end())
    return 0;
  return &*it;
}

BotProtocol::SessionAsset& Session::createAsset(const BotProtocol::SessionAsset& asset)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::SessionAsset& result = assets.append(entityId, asset);
  result.entityId = entityId;
  result.date = Time::time();
  return result;
}

bool_t Session::deleteAsset(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator it = assets.find(id);
  if(it == assets.end())
    return false;
  assets.remove(it);
  return true;
}

const BotProtocol::SessionProperty* Session::getProperty(uint32_t id) const
{
  HashMap<uint32_t, BotProtocol::SessionProperty>::Iterator it = properties.find(id);
  if(it == properties.end())
    return 0;
  return &*it;
}

BotProtocol::SessionProperty& Session::createProperty(const BotProtocol::SessionProperty& property)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::SessionProperty& result = properties.append(entityId, property);
  result.entityId = entityId;
  if(simulation)
    backupProperties.append(entityId, result);
  return result;
}

bool_t Session::deleteProperty(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::SessionProperty>::Iterator it = properties.find(id);
  if(it == properties.end())
    return false;
  properties.remove(it);
  return true;
}

BotProtocol::Order& Session::createOrder(const BotProtocol::Order& order)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::Order& result = orders.append(entityId, order);
  result.entityId = entityId;
  result.date = Time::time();
  return result;
}

bool_t Session::deleteOrder(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Order>::Iterator it = orders.find(id);
  if(it == orders.end())
    return false;
  orders.remove(it);
  return true;
}

BotProtocol::Marker& Session::createMarker(const BotProtocol::Marker& marker)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::Marker& result = markers.append(entityId, marker);
  result.entityId = entityId;
  return result;
}

BotProtocol::SessionLogMessage& Session::addLogMessage(const BotProtocol::SessionLogMessage& logMessage)
{
  BotProtocol::SessionLogMessage& result = logMessages.append(logMessage);
  result.entityId = 0;
  while(logMessages.size() > 100)
    logMessages.removeFront();
  return result;
}

void_t Session::getEntity(BotProtocol::Session& session) const
{
  session.entityType = BotProtocol::session;
  session.entityId = __id;
  BotProtocol::setString(session.name, name);
  session.botEngineId = engine->getId();
  session.marketId = market->getId();
  session.state = state;
}

void_t Session::sendUpdateEntity(const void_t* data, size_t size)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->sendUpdateEntity(0, data, size);
}

void_t Session::sendRemoveEntity(BotProtocol::EntityType type, uint32_t id)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->sendRemoveEntity(0, type, id);
}

void_t Session::sendRemoveAllEntities(BotProtocol::EntityType type)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->sendRemoveAllEntities(type);
}
