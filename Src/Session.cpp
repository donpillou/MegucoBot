
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
  state(BotProtocol::Session::stopped), pid(0), handlerClient(0), entityClient(0), nextEntityId(1) {}

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

      double fee = transactionVar.find("fee")->toDouble(); // todo: this is code for backward compatibility; remove this
      if(fee != 0)
        transaction.total = transaction.type == BotProtocol::Transaction::buy ? (Math::abs(transaction.price * transaction.amount) + fee) : (Math::abs(transaction.price * transaction.amount) - fee);

      if(transactions.find(transaction.entityId) != transactions.end())
        continue;
      transactions.append(transaction.entityId, transaction);
      if(transaction.entityId >= nextEntityId)
        nextEntityId = transaction.entityId + 1;
    }
  }
  {
    const List<Variant>& itemsVar = data.find("items")->toList();
    BotProtocol::SessionItem item;
    item.entityType = BotProtocol::sessionItem;
    for(List<Variant>::Iterator i = itemsVar.begin(), end = itemsVar.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& itemVar = i->toMap();
      item.entityId = itemVar.find("id")->toUInt();
      item.type = itemVar.find("type")->toUInt();
      item.state = itemVar.find("state")->toUInt();
      item.date = itemVar.find("date")->toInt64();
      item.price = itemVar.find("price")->toDouble();
      double amount = itemVar.find("amount")->toDouble();
      double total = itemVar.find("total")->toDouble();
      item.balanceBase = itemVar.find("balanceBase")->toDouble();
      item.balanceComm = itemVar.find("balanceComm")->toDouble();
      item.profitablePrice = itemVar.find("profitablePrice")->toDouble();
      item.flipPrice = itemVar.find("flipPrice")->toDouble();
      item.orderId = itemVar.find("orderId")->toUInt();

      if(total == 0. && item.price != 0. && amount != 0.) // todo: this is code for backward compatibility; remove this
      {
        BotProtocol::SessionItem::Type type = (BotProtocol::SessionItem::Type)item.type;
        switch(item.state)
        {
        case BotProtocol::SessionItem::waitBuy:
        case BotProtocol::SessionItem::buying:
          type = BotProtocol::SessionItem::buy;
          break;
        case BotProtocol::SessionItem::waitSell:
        case BotProtocol::SessionItem::selling:
          type = BotProtocol::SessionItem::sell;
          break;
        default:
          break;
        }
        total = type == BotProtocol::SessionItem::buy ? Math::ceil(item.price * amount * (1. + .005) * 100.) / 100. : Math::floor(item.price * amount * (1. - .005) * 100.) / 100.;
      }

      if(item.balanceBase == 0. && item.balanceComm == 0.) // todo: this is code for backward compatibility; remove this
      {
        BotProtocol::SessionItem::Type type = (BotProtocol::SessionItem::Type)item.type;
        switch(item.state)
        {
        case BotProtocol::SessionItem::waitBuy:
        case BotProtocol::SessionItem::buying:
          type = BotProtocol::SessionItem::buy;
          break;
        case BotProtocol::SessionItem::waitSell:
        case BotProtocol::SessionItem::selling:
          type = BotProtocol::SessionItem::sell;
          break;
        default:
          break;
        }
        if(type == BotProtocol::SessionItem::buy)
          item.balanceBase = total != 0. ? total : Math::ceil(item.flipPrice * amount * (1. + .005) * 100.) / 100.;
        else
          item.balanceComm = amount;
      }

      if(items.find(item.entityId) != items.end())
        continue;
      items.append(item.entityId, item);
      if(item.entityId >= nextEntityId)
        nextEntityId = item.entityId + 1;
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

      double fee = orderVar.find("fee")->toDouble(); // todo: this is code for backward compatibility; remove this
      if(fee != 0)
        order.total = order.type == BotProtocol::Order::buy ? (Math::abs(order.price * order.amount) + fee) : (Math::abs(order.price * order.amount) - fee);

      if(orders.find(order.entityId) != orders.end())
        continue;
      orders.append(order.entityId, order);
      if(order.entityId >= nextEntityId)
        nextEntityId = order.entityId + 1;
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
  //if(market)
  //  market->unregisterSession(*this);
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
    List<Variant>& itemsVar = data.append("items", Variant()).toList();
    for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
    {
      const BotProtocol::SessionItem& item = *i;
      HashMap<String, Variant>& itemVar = itemsVar.append(Variant()).toMap();
      itemVar.append("id", item.entityId);
      itemVar.append("type", (uint32_t)item.type);
      itemVar.append("state", (uint32_t)item.state);
      itemVar.append("date", item.date);
      itemVar.append("price", item.price);
      itemVar.append("balanceComm", item.balanceComm);
      itemVar.append("balanceBase", item.balanceBase);
      itemVar.append("profitablePrice", item.profitablePrice);
      itemVar.append("flipPrice", item.flipPrice);
      itemVar.append("orderId", item.orderId);
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
  backupItems.swap(items);
  backupProperties.swap(properties);
  backupOrders.swap(orders);
  backupMarkers.swap(markers);
  backupLogMessages.swap(logMessages);

  // but, keep items and properties
  items = backupItems;
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
    backupItems.swap(items);
    backupProperties.swap(properties);
    backupOrders.swap(orders);
    backupMarkers.swap(markers);
    backupLogMessages.swap(logMessages);

    backupTransactions.clear();
    backupItems.clear();
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

BotProtocol::Transaction* Session::createTransaction(const BotProtocol::Transaction& transaction)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::Transaction& result = transactions.append(entityId, transaction);
  result.entityId = entityId;
  return &result;
}

bool_t Session::deleteTransaction(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Transaction>::Iterator it = transactions.find(id);
  if(it == transactions.end())
    return false;
  transactions.remove(it);
  return true;
}

const BotProtocol::SessionItem* Session::getItem(uint32_t id) const
{
  HashMap<uint32_t, BotProtocol::SessionItem>::Iterator it = items.find(id);
  if(it == items.end())
    return 0;
  return &*it;
}

BotProtocol::SessionItem* Session::createItem(const BotProtocol::SessionItem& item)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::SessionItem& result = items.append(entityId, item);
  result.entityId = entityId;
  result.date = Time::time();
  return &result;
}

bool_t Session::deleteItem(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::SessionItem>::Iterator it = items.find(id);
  if(it == items.end())
    return false;
  items.remove(it);
  return true;
}

const BotProtocol::SessionProperty* Session::getProperty(uint32_t id) const
{
  HashMap<uint32_t, BotProtocol::SessionProperty>::Iterator it = properties.find(id);
  if(it == properties.end())
    return 0;
  return &*it;
}

BotProtocol::SessionProperty* Session::createProperty(const BotProtocol::SessionProperty& property)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::SessionProperty& result = properties.append(entityId, property);
  result.entityId = entityId;
  return &result;
}

bool_t Session::deleteProperty(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::SessionProperty>::Iterator it = properties.find(id);
  if(it == properties.end())
    return false;
  properties.remove(it);
  return true;
}

BotProtocol::Order* Session::createOrder(const BotProtocol::Order& order)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::Order& result = orders.append(entityId, order);
  result.entityId = entityId;
  result.date = Time::time();
  return &result;
}

bool_t Session::deleteOrder(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Order>::Iterator it = orders.find(id);
  if(it == orders.end())
    return false;
  orders.remove(it);
  return true;
}

BotProtocol::Marker* Session::createMarker(const BotProtocol::Marker& marker)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::Marker& result = markers.append(entityId, marker);
  result.entityId = entityId;
  return &result;
}

BotProtocol::SessionLogMessage* Session::addLogMessage(const BotProtocol::SessionLogMessage& logMessage)
{
  BotProtocol::SessionLogMessage* result = &logMessages.append(logMessage);
  result->entityId = 0;
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
