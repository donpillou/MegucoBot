
#include <nstd/Time.h>

#include "Session.h"
#include "ServerHandler.h"
#include "ClientHandler.h"
#include "BotEngine.h"
#include "MarketAdapter.h"
#include "User.h"

Session::Session(ServerHandler& serverHandler, User& user, uint32_t id, const String& name, BotEngine& engine, MarketAdapter& marketAdapter, double balanceBase, double balanceComm) :
  serverHandler(serverHandler), user(user),
  id(id), name(name), engine(&engine), marketAdapter(&marketAdapter),
  simulation(true), balanceBase(balanceBase), balanceComm(balanceComm),
  state(BotProtocol::Session::stopped), pid(0), botClient(0), nextEntityId(1) {}

Session::Session(ServerHandler& serverHandler, User& user, const Variant& variant) :
  serverHandler(serverHandler), user(user),
  state(BotProtocol::Session::stopped), pid(0), botClient(0), nextEntityId(1)
{
  const HashMap<String, Variant>& data = variant.toMap();
  id = data.find("id")->toUInt();
  name = data.find("name")->toString();
  engine = serverHandler.findBotEngine(data.find("engine")->toString());
  marketAdapter = serverHandler.findMarketAdapter(data.find("market")->toString());
  balanceBase = data.find("balanceBase")->toDouble();
  balanceComm = data.find("balanceComm")->toDouble();
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
    transactions.append(id, transaction);
    if(transaction.entityId >= nextEntityId)
      nextEntityId = transaction.entityId + 1;
  }
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
    orders.append(id, order);
    if(order.entityId >= nextEntityId)
      nextEntityId = order.entityId + 1;
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
}

void_t Session::toVariant(Variant& variant)
{
  HashMap<String, Variant>& data = variant.toMap();
  data.append("id", id);
  data.append("name", name);
  data.append("engine", engine->getName());
  data.append("market", marketAdapter->getName());
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
}

bool_t Session::saveData()
{
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
  return true;
}

bool_t Session::stop()
{
  if(pid == 0)
    return false;
  if(!process.kill())
    return false;
  pid = 0;
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
    clients.append(&client);
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
    state = BotProtocol::Session::stopped;
    clients.remove(&client);
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
  return &transactions.append(id, transaction);
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
  return &orders.append(id, order);
}

bool_t Session::deleteOrder(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Order>::Iterator it = orders.find(id);
  if(it == orders.end())
    return false;
  orders.remove(it);
  return true;
}

void_t Session::send(ClientHandler* client)
{
  BotProtocol::Session sessionData;
  sessionData.entityType = BotProtocol::session;
  sessionData.entityId = id;
  BotProtocol::setString(sessionData.name, name);
  sessionData.botEngineId = engine->getId();
  sessionData.marketId = marketAdapter->getId();
  sessionData.state = state;
  sessionData.balanceBase = balanceBase;
  sessionData.balanceComm = balanceComm;
  if(client)
    client->sendEntity(&sessionData, sizeof(sessionData));
  else
    user.sendEntity(&sessionData, sizeof(sessionData));
}

void_t Session::sendEntity(const void_t* data, size_t size)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->sendEntity(data, size);
}

void_t Session::removeEntity(BotProtocol::EntityType type, uint32_t id)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->removeEntity(type, id);
}
