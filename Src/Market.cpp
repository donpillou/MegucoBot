
#include "Market.h"
#include "ServerHandler.h"
#include "MarketAdapter.h"
#include "ClientHandler.h"
//#include "Engine.h"
//#include "MarketAdapter.h"
//#include "Transaction.h"
#include "User.h"
//#include "Order.h"

Market::Market(ServerHandler& serverHandler, User& user, uint32_t id, MarketAdapter& marketAdapter, const String& username, const String& key, const String& secret) :
  serverHandler(serverHandler), user(user),
  id(id), marketAdapter(&marketAdapter), username(username), key(key), secret(secret),
  state(BotProtocol::Market::stopped), pid(0), adapterClient(0), nextEntityId(1) {}

Market::Market(ServerHandler& serverHandler, User& user, const Variant& variant) :
  serverHandler(serverHandler), user(user),
  state(BotProtocol::Market::stopped), pid(0), adapterClient(0), nextEntityId(1)
{
  const HashMap<String, Variant>& data = variant.toMap();
  id = data.find("id")->toUInt();
  marketAdapter = serverHandler.findMarketAdapter(data.find("market")->toString());
  username = data.find("username")->toString();
  key = data.find("key")->toString();
  secret = data.find("secret")->toString();
}

Market::~Market()
{
  if(pid != 0)
    serverHandler.unregisterMarket(pid);
  process.kill();
  if(adapterClient)
    adapterClient->deselectMarket();
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->deselectMarket();
}

void_t Market::toVariant(Variant& variant)
{
  HashMap<String, Variant>& data = variant.toMap();
  data.append("id", id);
  data.append("market", marketAdapter->getName());
  data.append("username", username);
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
  send();
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
  send();
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
    send();
  }
  else
    clients.append(&client);
  return true;
}

void_t Market::unregisterClient(ClientHandler& client)
{
  if(&client == adapterClient)
  {
    adapterClient = 0;
    state = BotProtocol::Market::stopped;
    send();
  }
  else
    clients.remove(&client);
}

void_t Market::send(ClientHandler* client)
{
  BotProtocol::Market marketData;
  marketData.marketAdapterId = marketAdapter->getId();
  marketData.state = state;

  if(client)
    client->sendEntity(BotProtocol::market, id, &marketData, sizeof(marketData));
  else
    user.sendEntity(BotProtocol::market, id, &marketData, sizeof(marketData));
}

void_t Market::sendEntity(BotProtocol::EntityType type, uint32_t id, const void_t* data, size_t size)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->sendEntity(type, id, data, size);
}

void_t Market::removeEntity(BotProtocol::EntityType type, uint32_t id)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->removeEntity(type, id);
}
