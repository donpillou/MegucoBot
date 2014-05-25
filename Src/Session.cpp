
#include <nstd/Time.h>

#include "Session.h"
#include "ServerHandler.h"
#include "ClientHandler.h"
#include "BotEngine.h"
#include "MarketAdapter.h"
#include "User.h"
#include "Market.h"

Session::Session(ServerHandler& serverHandler, User& user, uint32_t id, const String& name, BotEngine& engine, Market& market, double balanceBase, double balanceComm) :
  serverHandler(serverHandler), user(user),
  __id(id), name(name), engine(&engine), market(&market),
  simulation(true), balanceBase(balanceBase), balanceComm(balanceComm),
  state(BotProtocol::Session::stopped), pid(0), botClient(0), nextEntityId(1) {}

Session::Session(ServerHandler& serverHandler, User& user, const Variant& variant) :
  serverHandler(serverHandler), user(user),
  state(BotProtocol::Session::stopped), pid(0), botClient(0), nextEntityId(1)
{
  const HashMap<String, Variant>& data = variant.toMap();
  __id = data.find("id")->toUInt();
  name = data.find("name")->toString();
  engine = serverHandler.findBotEngine(data.find("engine")->toString());
  market = user.findMarket(data.find("marketId")->toUInt());
  balanceBase = data.find("balanceBase")->toDouble();
  balanceComm = data.find("balanceComm")->toDouble();
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
  if(botClient)
    botClient->deselectSession();
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
  data.append("balanceBase", balanceBase);
  data.append("balanceComm", balanceComm);
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
  List<Variant>& logMessagesVar = data.append("logMessages", Variant()).toList();
  for(List<BotProtocol::SessionLogMessage>::Iterator i = logMessages.begin(), end = logMessages.end(); i != end; ++i)
  {
    BotProtocol::SessionLogMessage& logMessage = *i;
    HashMap<String, Variant>& logMessageVar = logMessagesVar.append(Variant()).toMap();
    logMessageVar.append("date", logMessage.date);
    logMessageVar.append("message", BotProtocol::getString(logMessage.message));
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
  backupOrders.swap(orders);
  backupMarkers.swap(markers);
  backupLogMessages.swap(logMessages);

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

    // todo: restore backup of orders, transactions, log messages, and markers
    backupTransactions.swap(transactions);
    backupOrders.swap(orders);
    backupMarkers.swap(markers);
    backupLogMessages.swap(logMessages);
  }
  state = BotProtocol::Session::stopped;
  return true;
}

bool_t Session::registerClient(ClientHandler& client, bool_t bot)
{
  if(bot)
  {
    if(botClient)
      return false;
    botClient = &client;
    state = simulation ? BotProtocol::Session::simulating : BotProtocol::Session::running;
  }
  else
    clients.append(&client);
  return true;
}

void_t Session::unregisterClient(ClientHandler& client)
{
  if(&client == botClient)
  {
    botClient = 0;
    stop();
  }
  else
    clients.remove(&client);
}

void_t Session::getInitialBalance(double& balanceBase, double& balanceComm) const
{
  balanceBase = this->balanceBase;
  balanceComm = this->balanceComm;
}

BotProtocol::Transaction* Session::createTransaction(double price, double amount, double fee, BotProtocol::Transaction::Type type)
{
  BotProtocol::Transaction transaction;
  transaction.entityType = BotProtocol::sessionTransaction;
  transaction.entityId = nextEntityId++;
  transaction.type = type;
  transaction.date = Time::time();
  transaction.price = price;
  transaction.amount = amount;
  transaction.fee = fee;
  return &transactions.append(transaction.entityId, transaction);
}

BotProtocol::Transaction* Session::updateTransaction(BotProtocol::Transaction& transaction)
{
  return &transactions.append(transaction.entityId, transaction);
}

bool_t Session::deleteTransaction(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Transaction>::Iterator it = transactions.find(id);
  if(it == transactions.end())
    return false;
  transactions.remove(it);
  return true;
}

BotProtocol::Order* Session::createOrder(double price, double amount, double fee, BotProtocol::Order::Type type)
{
  BotProtocol::Order order;
  order.entityType = BotProtocol::sessionOrder;
  order.entityId = nextEntityId++;
  order.type = type;
  order.date = Time::time();
  order.price = price;
  order.amount = amount;
  order.fee = fee;
  return &orders.append(order.entityId, order);
}

bool_t Session::deleteOrder(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Order>::Iterator it = orders.find(id);
  if(it == orders.end())
    return false;
  orders.remove(it);
  return true;
}

BotProtocol::Marker* Session::createMarker(BotProtocol::Marker::Type type, timestamp_t date)
{
  BotProtocol::Marker marker;
  marker.entityType = BotProtocol::sessionMarker;
  marker.entityId = nextEntityId++;
  marker.type = type;
  marker.date = date;
  return &markers.append(marker.entityId, marker);
}

BotProtocol::SessionLogMessage* Session::addLogMessage(timestamp_t date, const String& message)
{
  BotProtocol::SessionLogMessage logMessage;
  logMessage.entityType = BotProtocol::sessionLogMessage;
  logMessage.entityId = 0;
  logMessage.date = date;
  BotProtocol::setString(logMessage.message, message);
  BotProtocol::SessionLogMessage* result = &logMessages.append(logMessage);
  while(logMessages.size() > 100)
    logMessages.removeFront();
  return result;
}

void_t Session::send(ClientHandler* client)
{
  BotProtocol::Session sessionData;
  sessionData.entityType = BotProtocol::session;
  sessionData.entityId = __id;
  BotProtocol::setString(sessionData.name, name);
  sessionData.botEngineId = engine->getId();
  sessionData.marketId = market->getId();
  sessionData.state = state;
  sessionData.balanceBase = balanceBase;
  sessionData.balanceComm = balanceComm;
  if(client)
    client->sendUpdateEntity(0, &sessionData, sizeof(sessionData));
  else
    user.sendUpdateEntity(&sessionData, sizeof(sessionData));
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
