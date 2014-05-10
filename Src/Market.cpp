
#include <nstd/Time.h>
#include <nstd/Debug.h>

#include "Market.h"
#include "ServerHandler.h"
#include "MarketAdapter.h"
#include "ClientHandler.h"
//#include "Engine.h"
//#include "MarketAdapter.h"
//#include "Transaction.h"
#include "User.h"
//#include "Order.h"

Market::Market(ServerHandler& serverHandler, User& user, uint32_t id, MarketAdapter& marketAdapter, const String& userName, const String& key, const String& secret) :
  serverHandler(serverHandler), user(user),
  __id(id), marketAdapter(&marketAdapter), userName(userName), key(key), secret(secret),
  state(BotProtocol::Market::stopped), pid(0), adapterClient(0), nextRequestId(1)
{
  Memory::zero(&balance, sizeof(balance));
}

Market::Market(ServerHandler& serverHandler, User& user, const Variant& variant) :
  serverHandler(serverHandler), user(user),
  state(BotProtocol::Market::stopped), pid(0), adapterClient(0), nextRequestId(1)
{
  const HashMap<String, Variant>& data = variant.toMap();
  __id = data.find("id")->toUInt();
  marketAdapter = serverHandler.findMarketAdapter(data.find("market")->toString());
  userName = data.find("userName")->toString();
  key = data.find("key")->toString();
  secret = data.find("secret")->toString();
  Memory::zero(&balance, sizeof(balance));
}

Market::~Market()
{
  if(pid != 0)
    serverHandler.unregisterMarket(pid);
  process.kill();
  if(adapterClient)
    adapterClient->deselectMarket();
  for(HashMap<ClientHandler*, ClientInfo>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
  {
    ClientHandler* clientHandler = i.key();
    clientHandler->deselectMarket();
  }
}

void_t Market::toVariant(Variant& variant)
{
  HashMap<String, Variant>& data = variant.toMap();
  data.append("id", __id);
  data.append("market", marketAdapter->getName());
  data.append("userName", userName);
  data.append("key", key);
  data.append("secret", secret);
}

bool_t Market::start()
{
  if(pid != 0)
    return false;
  pid = process.start(marketAdapter->getPath());
  if(!pid)
    return false;
  serverHandler.registerMarket(pid, *this);
  state = BotProtocol::Market::starting;
  return true;
}

bool_t Market::stop()
{
  if(pid == 0)
    return false;
  if(!process.kill())
    return false;
  pid = 0;
  state = BotProtocol::Market::stopped;
  return true;
}

bool_t Market::registerClient(ClientHandler& client, bool_t adapter)
{
  if(adapter)
  {
    if(adapterClient)
      return false;
    adapterClient = &client;
    state = BotProtocol::Market::running;
  }
  else
    clients.append(&client, ClientInfo());
  return true;
}

void_t Market::unregisterClient(ClientHandler& client)
{
  if(&client == adapterClient)
  {
    adapterClient = 0;
    state = BotProtocol::Market::stopped;
  }
  else
  {
    HashMap<ClientHandler* , ClientInfo>::Iterator it = clients.find(&client);
    if(it != clients.end())
    {
      ClientInfo& clientInfo = *it;
      for(HashSet<uint32_t>::Iterator i =  clientInfo.responseIds.begin(), end = clientInfo.responseIds.end(); i != end; ++i)
        requestIds.remove(*i);
      clients.remove(it);
    }
  }
}

uint32_t Market::createRequestId(BotProtocol::EntityType entityType, uint32_t userRequestId, ClientHandler& client)
{
  HashMap<ClientHandler*, ClientInfo>::Iterator it = clients.find(&client);
  if(it == clients.end())
  {
    ASSERT(false);
    return 0;
  }
  ClientInfo& clientInfo = *it;
  uint32_t id = nextRequestId++;
  RequestId& requestId = requestIds.append(id, RequestId());
  requestId.client = &client;
  requestId.entityType = entityType;
  requestId.userRequestId = userRequestId;
  clientInfo.responseIds.append(id);
  return id;
}

bool_t Market::removeRequestId(BotProtocol::EntityType entityType, uint32_t id, uint32_t& userRequestId, ClientHandler*& client)
{
  HashMap<uint32_t, RequestId>::Iterator it = requestIds.find(id);
  if(it == requestIds.end())
    return false;
  RequestId& requestId = *it;
  if(requestId.entityType != entityType)
    return false;
  HashMap<ClientHandler*, ClientInfo>::Iterator itClient = clients.find(requestId.client);
  if(itClient == clients.end())
  {
    requestIds.remove(it);
    return false;
  }
  ClientInfo& clientInfo = *itClient;
  client = requestId.client;
  userRequestId = requestId.userRequestId;
  clientInfo.responseIds.remove(id);
  return true;
}

bool_t Market::updateTransaction(const BotProtocol::Transaction& transaction)
{
  HashMap<uint32_t, BotProtocol::Transaction>::Iterator it = transactions.find(transaction.entityId);
  if(it == transactions.end())
  { // add transaction if it does not already exist
    transactions.append(transaction.entityId, transaction);
    return true;
  }
  *it = transaction;
  return true;
}

bool_t Market::deleteTransaction(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Transaction>::Iterator it = transactions.find(id);
  if(it == transactions.end())
    return false;
  transactions.remove(it);
  return true;
}

bool_t Market::updateOrder(const BotProtocol::Order& order)
{
  HashMap<uint32_t, BotProtocol::Order>::Iterator it = orders.find(order.entityId);
  if(it == orders.end())
  {
    // add transaction if it does not already exist
    orders.append(order.entityId, order);
    return false;
  }
  *it = order;
  return true;
}

bool_t Market::deleteOrder(uint32_t id)
{
  HashMap<uint32_t, BotProtocol::Order>::Iterator it = orders.find(id);
  if(it == orders.end())
    return false;
  orders.remove(it);
  return true;
}

bool_t Market::updateBalance(const BotProtocol::MarketBalance& balance)
{
  this->balance = balance;
  return true;
}

void_t Market::send(ClientHandler* client)
{
  BotProtocol::Market market;
  market.entityType = BotProtocol::market;
  market.entityId = __id;
  market.marketAdapterId = marketAdapter->getId();
  market.state = state;
  BotProtocol::setString(market.userName, String());
  BotProtocol::setString(market.key, String());
  BotProtocol::setString(market.secret, String());

  if(client)
    client->sendEntity(&market, sizeof(market));
  else
    user.sendEntity(&market, sizeof(market));
}

void_t Market::sendEntity(const void_t* data, size_t size)
{
  for(HashMap<ClientHandler*, ClientInfo>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    i.key()->sendEntity(data, size);
}

void_t Market::removeEntity(BotProtocol::EntityType type, uint32_t id)
{
  for(HashMap<ClientHandler*, ClientInfo>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    i.key()->removeEntity(type, id);
}

