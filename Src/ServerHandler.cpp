
#include <nstd/File.h>
#include <nstd/Debug.h>
#include <nstd/Math.h>

#include "Tools/Json.h"
#include "Tools/Hex.h"
#include "Tools/Sha256.h"
#include "ServerHandler.h"
#include "ClientHandler.h"
#include "User.h"
#include "BotEngine.h"
#include "MarketAdapter.h"

ServerHandler::ServerHandler(uint16_t port) : port(port), nextEntityId(1), nextRequestId(1) {}

ServerHandler::~ServerHandler()
{
  for(HashMap<ClientHandler*, ClientData>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    delete i.key();
  for(HashMap<String, User*>::Iterator i = users.begin(), end = users.end(); i != end; ++i)
    delete *i;
  for(HashMap<uint32_t, BotEngine*>::Iterator i = botEngines.begin(), end = botEngines.end(); i != end; ++i)
    delete *i;
  for(HashMap<uint32_t, MarketAdapter*>::Iterator i = marketAdapters.begin(), end = marketAdapters.end(); i != end; ++i)
    delete *i;
}

void_t ServerHandler::acceptedClient(Server::Client& client, uint32_t addr, uint16_t port)
{
  ClientHandler* clientHandler = new ClientHandler(addr, *this, client);
  client.setListener(clientHandler);
  clients.append(clientHandler, ClientData());
}

void_t ServerHandler::closedClient(Server::Client& client)
{
  // find clientHandler and clientData
  ClientHandler* clientHandler = (ClientHandler*)client.getListener();
  HashMap<ClientHandler*, ClientData>::Iterator it = clients.find(clientHandler);
  ASSERT(it != clients.end());
  ClientData& clientData = *it;

  // remove ids of open requests
  while(!clientData.requestIds.isEmpty())
    removeRequestId(clientData.requestIds.front());

  // unregister and delete client handler
  clients.remove(it);
  delete clientHandler;
}

bool_t ServerHandler::addUser(const String& userName, const String& password)
{
  if(findUser(userName))
    return false;
  byte_t key[32];
  byte_t pwhmac[32];
  for(uint32_t* p = (uint32_t*)key, * end = (uint32_t*)(key + 32); p < end; ++p)
    *p = Math::random();
  Sha256::hmac(key, 32, (const byte_t*)(const char_t*)password, password.length(), pwhmac);
  User* user = new User(*this, userName, key, pwhmac);
  users.append(userName, user);
  saveData();
  return true;
}

void_t ServerHandler::addBotEngine(const String& name, const String& path)
{
  uint32_t id = nextEntityId++;
  BotEngine* engine = new BotEngine(id, path);
  botEngines.append(id, engine);
  botEnginesByName.append(name, engine);
}

void_t ServerHandler::addMarketAdapter(const String& name, const String& path, const String& currencyBase, const String& currencyComm)
{
  uint32_t id = nextEntityId++;
  MarketAdapter* marketAdapter = new MarketAdapter(id, name, path, currencyBase, currencyComm);
  marketAdapters.append(id, marketAdapter);
  marketAdaptersByName.append(name, marketAdapter);
}

bool_t ServerHandler::loadData()
{
  File file;
  if(!file.open("users.json", File::readFlag))
    return false;
  String data;
  if(!file.readAll(data))
    return false;
  Variant dataVar;
  if(!Json::parse(data, dataVar))
    return false;
  const List<Variant>& usersVar = dataVar.toList();
  Buffer key, pwhmac;
  for(List<Variant>::Iterator i = usersVar.begin(), end = usersVar.end(); i != end; ++i)
  {
    const HashMap<String, Variant>& userVar = i->toMap();

    const String& name = userVar.find("name")->toString();
    if(users.find(name) != users.end())
      continue;
    if(!Hex::fromString(userVar.find("key")->toString(), key) || key.size() != 32)
      continue;
    if(!Hex::fromString(userVar.find("pwhmac")->toString(), pwhmac) || pwhmac.size() != 32)
      continue;

    User* user = new User(*this, name, (const byte_t (&)[32])*(const byte_t*)key, (const byte_t (&)[32])*(const byte_t*)pwhmac);
    users.append(name, user);
    user->loadData();
  }
  return true;
}

bool_t ServerHandler::saveData()
{
  Variant dataVar;
  List<Variant>& usersVar = dataVar.toList();
  for(HashMap<String, User*>::Iterator i = users.begin(), end = users.end(); i != end; ++i)
  {
    const User* user = *i;
    HashMap<String, Variant>& userVar = usersVar.append(Variant()).toMap();
    userVar.append("name", user->getUserName());
    userVar.append("key", Hex::toString(user->getKey(), 32));
    userVar.append("pwhmac", Hex::toString(user->getPwHmac(), 32));
  }
  String json;
  if(!Json::generate(dataVar, json))
    return false;
  File file;
  if(!file.open("users.json", File::writeFlag))
    return false;
  if(!file.write(json))
    return false;
  return true;
}

uint32_t ServerHandler::createRequestId(uint32_t requesterRequestId, ClientHandler& requester, ClientHandler& requestee)
{
  HashMap<ClientHandler*, ClientData>::Iterator itRequester = clients.find(&requester);
  HashMap<ClientHandler*, ClientData>::Iterator itRequestee = clients.find(&requestee);
  ASSERT(itRequester != clients.end());
  ASSERT(itRequestee != clients.end());
  ClientData& requsterData = *itRequester;
  ClientData& requsteeData = *itRequestee;
  uint32_t id = nextRequestId++;
  RequestId& requestId = requestIds.append(id, RequestId());
  requestId.requester = &requester;
  requestId.requestee = &requestee;
  requestId.requesterRequestId = requesterRequestId;
  requsterData.requestIds.append(id);
  requsteeData.requestIds.append(id);
  return id;
}

bool_t ServerHandler::findAndRemoveRequestId(uint32_t id, uint32_t& requesterRequestId, ClientHandler*& requester)
{
  HashMap<uint32_t, RequestId>::Iterator it = requestIds.find(id);
  if(it == requestIds.end())
    return false;
  RequestId& requestId = *it;
  HashMap<ClientHandler*, ClientData>::Iterator itRequester = clients.find(requestId.requester);
  HashMap<ClientHandler*, ClientData>::Iterator itRequestee = clients.find(requestId.requestee);
  ASSERT(itRequester != clients.end());
  ASSERT(itRequestee != clients.end());
  requester = requestId.requester;
  requesterRequestId = requestId.requesterRequestId;
  ClientData& requsterData = *itRequester;
  ClientData& requsteeData = *itRequestee;
  requsterData.requestIds.remove(id);
  requsteeData.requestIds.remove(id);
  requestIds.remove(it);
  return true;
}

void_t ServerHandler::removeRequestId(uint32_t id)
{
  HashMap<uint32_t, RequestId>::Iterator it = requestIds.find(id);
  if(it == requestIds.end())
    return;
  RequestId& requestId = *it;
  HashMap<ClientHandler*, ClientData>::Iterator itRequester = clients.find(requestId.requester);
  HashMap<ClientHandler*, ClientData>::Iterator itRequestee = clients.find(requestId.requestee);
  ASSERT(itRequester != clients.end());
  ASSERT(itRequestee != clients.end());
  ClientData& requsterData = *itRequester;
  ClientData& requsteeData = *itRequestee;
  requsterData.requestIds.remove(id);
  requsteeData.requestIds.remove(id);
  requestIds.remove(it);
}
