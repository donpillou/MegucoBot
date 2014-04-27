
#include "Session.h"
#include "ServerHandler.h"
#include "ClientHandler.h"
#include "BotEngine.h"
#include "MarketAdapter.h"
#include "Transaction.h"
#include "User.h"
#include "Order.h"

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
  for(List<Variant>::Iterator i = transactionsVar.begin(), end = transactionsVar.end(); i != end; ++i)
  {
    Transaction* transaction = new Transaction(*this, *i);
    uint32_t id = transaction->getId();
    if(transactions.find(id) != transactions.end())
    {
      delete transaction;
      continue;
    }
    transactions.append(id, transaction);
    if(id >= nextEntityId)
      nextEntityId = id + 1;
  }
  const List<Variant>& ordersVar = data.find("orders")->toList();
  for(List<Variant>::Iterator i = ordersVar.begin(), end = ordersVar.end(); i != end; ++i)
  {
    Order* order = new Order(*this, *i);
    uint32_t id = order->getId();
    if(transactions.find(id) != transactions.end())
    {
      delete order;
      continue;
    }
    orders.append(id, order);
    if(id >= nextEntityId)
      nextEntityId = id + 1;
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
  for(HashMap<uint32_t, Transaction*>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
    delete *i;
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
  for(HashMap<uint32_t, Transaction*>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
  {
    Variant& transactionVar = transactionsVar.append(Variant());;
    (*i)->toVariant(transactionVar);
  }
  List<Variant>& ordersVar = data.append("orders", Variant()).toList();
  for(HashMap<uint32_t, Order*>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
  {
    Variant& orderVar = ordersVar.append(Variant());;
    (*i)->toVariant(orderVar);
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

Transaction* Session::createTransaction(double price, double amount, double fee, BotProtocol::Transaction::Type type)
{
  uint32_t id = nextEntityId++;
  Transaction* transaction = new Transaction(*this, id, price, amount, fee, type);
  transactions.append(id, transaction);
  return transaction;
}

bool_t Session::deleteTransaction(uint32_t id)
{
  HashMap<uint32_t, Transaction*>::Iterator it = transactions.find(id);
  if(it == transactions.end())
    return false;
  delete *it;
  transactions.remove(it);
  return true;
}

Order* Session::createOrder(double price, double amount, double fee, BotProtocol::Order::Type type)
{
  uint32_t id = nextEntityId++;
  Order* order = new Order(*this, id, price, amount, fee, type);
  orders.append(id, order);
  return order;
}

bool_t Session::deleteOrder(uint32_t id)
{
  HashMap<uint32_t, Order*>::Iterator it = orders.find(id);
  if(it == orders.end())
    return false;
  delete *it;
  orders.remove(it);
  return true;
}

void_t Session::send(ClientHandler* client)
{
  BotProtocol::Session sessionData;
  sessionData.entityType = BotProtocol::session;
  sessionData.entityId = id;
  BotProtocol::setString(sessionData.name, name);
  sessionData.engineId = engine->getId();
  sessionData.marketId = marketAdapter->getId();
  sessionData.state = state;
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
