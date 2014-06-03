
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
  state(BotProtocol::Market::stopped), pid(0), handlerClient(0), entityClient(0)
{
  Memory::zero(&balance, sizeof(balance));
}

Market::Market(ServerHandler& serverHandler, User& user, const Variant& variant) :
  serverHandler(serverHandler), user(user),
  state(BotProtocol::Market::stopped), pid(0), handlerClient(0), entityClient(0)
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
  if(handlerClient)
    handlerClient->deselectMarket();
  if(entityClient)
    entityClient->deselectMarket();
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->deselectMarket();
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

bool_t Market::registerClient(ClientHandler& client, ClientType type)
{
  switch(type)
  {
  case handlerType:
    if(handlerClient)
      return false;
    handlerClient = &client;
    state = BotProtocol::Market::running;
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

void_t Market::unregisterClient(ClientHandler& client)
{
  if(&client == handlerClient)
  {
    handlerClient = 0;
    state = BotProtocol::Market::stopped;
  }
  else if(&client == entityClient)
    entityClient = 0;
  else
    clients.remove(&client);
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
    // add order if it does not already exist
    orders.append(order.entityId, order);
    return true;
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

void_t Market::getEntity(BotProtocol::Market& market) const
{
  market.entityType = BotProtocol::market;
  market.entityId = __id;
  market.marketAdapterId = marketAdapter->getId();
  market.state = state;
  BotProtocol::setString(market.userName, String());
  BotProtocol::setString(market.key, String());
  BotProtocol::setString(market.secret, String());
}

void_t Market::sendUpdateEntity(const void_t* data, size_t size)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->sendUpdateEntity(0, data, size);
}

void_t Market::sendRemoveEntity(BotProtocol::EntityType type, uint32_t id)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->sendRemoveEntity(0, type, id);
}
