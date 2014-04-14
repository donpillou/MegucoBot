
#include <nstd/File.h>

#include "Tools/Json.h"
#include "Tools/Hex.h"
#include "Tools/Math.h"
#include "Tools/Sha256.h"
#include "ServerHandler.h"
#include "ClientHandler.h"
#include "User.h"
#include "Engine.h"
#include "Market.h"

ServerHandler::~ServerHandler()
{
  for(HashMap<uint64_t, ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    delete *i;
  for(HashMap<String, User*>::Iterator i = users.begin(), end = users.end(); i != end; ++i)
    delete *i;
  for(HashMap<uint32_t, Engine*>::Iterator i = engines.begin(), end = engines.end(); i != end; ++i)
    delete *i;
  for(HashMap<uint32_t, Market*>::Iterator i = markets.begin(), end = markets.end(); i != end; ++i)
    delete *i;
}

void_t ServerHandler::acceptedClient(Server::Client& client, uint32_t addr, uint16_t port)
{
  uint64_t clientId = nextEntityId++;
  ClientHandler* clientHandler = new ClientHandler(clientId, addr, *this, client);
  client.setListener(clientHandler);
  clients.append(clientId, clientHandler);
}

void_t ServerHandler::closedClient(Server::Client& client)
{
  ClientHandler* clientHandler = (ClientHandler*)client.getListener();
  clients.remove(clientHandler->getId());
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

User* ServerHandler::findUser(const String& userName)
{
  return *users.find(userName);
}

void_t ServerHandler::addEngine(const String& path)
{
  uint32_t id = nextEntityId++;
  Engine* engine = new Engine(id, path);
  engines.append(id, engine);
}

Session* ServerHandler::findSessionByPid(uint32_t pid)
{
  return *sessions.find(pid);
}

void_t ServerHandler::registerSession(uint32_t pid, Session& session)
{
  sessions.append(pid, &session);
}

void_t ServerHandler::unregisterSession(uint32_t pid)
{
  sessions.remove(pid);
}

void_t ServerHandler::addMarket(const String& name, const String& currencyBase, const String& currencyComm)
{
  uint32_t id = nextEntityId++;
  Market* market = new Market(id, name, currencyBase, currencyComm);
  markets.append(id, market);
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
