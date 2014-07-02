
#include <nstd/Time.h>

#include "Session.h"
#include "ServerHandler.h"
#include "ClientHandler.h"
#include "BotEngine.h"
#include "MarketAdapter.h"
#include "User.h"
#include "Market.h"

Session::Session(ServerHandler& serverHandler, User& user, uint32_t id, const String& name, BotEngine& engine, Market& market, double initialBalanceBase, double initialBalanceComm) :
  serverHandler(serverHandler), user(user),
  __id(id), name(name), engine(&engine), market(&market),
  simulation(true), initialBalanceBase(initialBalanceBase), initialBalanceComm(initialBalanceComm),
  state(BotProtocol::Session::stopped), pid(0), handlerClient(0), entityClient(0), nextEntityId(1)
{
  balance.entityType = BotProtocol::sessionBalance;
  balance.entityId = 0;
  balance.reservedUsd = 0.;
  balance.reservedBtc = 0.;
  balance.availableUsd = initialBalanceBase;
  balance.availableBtc = initialBalanceComm;
  balance.fee = 0.;
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
  initialBalanceBase = data.find("investmentBase")->toDouble();
  initialBalanceComm = data.find("investmentComm")->toDouble();
  balance.entityType = BotProtocol::sessionBalance;
  balance.entityId = 0;
  balance.availableUsd = data.find("balanceBase")->toDouble();
  balance.availableBtc = data.find("balanceComm")->toDouble();
  balance.reservedUsd = data.find("reservedBase")->toDouble();
  balance.reservedBtc = data.find("reservedComm")->toDouble();
  balance.fee = data.find("fee")->toDouble();
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
      transaction.fee = transactionVar.find("fee")->toDouble();
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
      item.amount = itemVar.find("amount")->toDouble();
      item.flipPrice = itemVar.find("flipPrice")->toDouble();
      if(items.find(item.entityId) != items.end())
        continue;
      items.append(item.entityId, item);
      if(item.entityId >= nextEntityId)
        nextEntityId = item.entityId + 1;
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
      order.fee = orderVar.find("fee")->toDouble();
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
  data.append("investmentBase", initialBalanceBase);
  data.append("investmentComm", initialBalanceComm);
  data.append("balanceBase", balance.availableUsd);
  data.append("balanceComm", balance.availableBtc);
  data.append("reservedBase", balance.reservedUsd);
  data.append("reservedComm", balance.reservedBtc);
  data.append("fee", balance.fee);
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
      transactionVar.append("fee", transaction.fee);
    }
  }
  {
    List<Variant>& itemsVar = data.append("items", Variant()).toList();
    for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
    {
      const BotProtocol::SessionItem& item = *i;
      HashMap<String, Variant>& transactionVar = itemsVar.append(Variant()).toMap();
      transactionVar.append("id", item.entityId);
      transactionVar.append("type", (uint32_t)item.type);
      transactionVar.append("state", (uint32_t)item.state);
      transactionVar.append("date", item.date);
      transactionVar.append("price", item.price);
      transactionVar.append("amount", item.amount);
      transactionVar.append("flipPrice", item.flipPrice);
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
      orderVar.append("fee", order.fee);
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
  backupBalance = balance;
  balance.availableUsd = initialBalanceBase;
  balance.availableBtc = initialBalanceComm;
  balance.reservedUsd = 0.;
  balance.reservedBtc = 0.;
  balance.fee = 0.;
  backupTransactions.swap(transactions);
  backupItems.swap(items);
  backupOrders.swap(orders);
  backupMarkers.swap(markers);
  backupLogMessages.swap(logMessages);

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
    balance = backupBalance;
    backupTransactions.swap(transactions);
    backupItems.swap(items);
    backupOrders.swap(orders);
    backupMarkers.swap(markers);
    backupLogMessages.swap(logMessages);

    backupTransactions.clear();
    backupItems.clear();
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
  result.date = Time::time();
  return &result;
}

BotProtocol::Transaction* Session::updateTransaction(const BotProtocol::Transaction& transaction)
{
  return &transactions.append(transaction.entityId, transaction);
}

BotProtocol::SessionItem* Session::createItem(const BotProtocol::SessionItem& item)
{
  uint32_t entityId = nextEntityId++;
  BotProtocol::SessionItem& result = items.append(entityId, item);
  result.entityId = entityId;
  result.date = Time::time();
  return &result;
}

BotProtocol::SessionItem* Session::updateItem(const BotProtocol::SessionItem& item)
{
  return &items.append(item.entityId, item);
}

bool_t Session::deleteTransaction(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Transaction>::Iterator it = transactions.find(id);
  if(it == transactions.end())
    return false;
  transactions.remove(it);
  return true;
}

bool_t Session::deleteItem(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::SessionItem>::Iterator it = items.find(id);
  if(it == items.end())
    return false;
  items.remove(it);
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
  session.balanceBase = initialBalanceBase;
  session.balanceComm = initialBalanceComm;
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
