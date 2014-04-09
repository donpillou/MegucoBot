
#include <nstd/File.h>

#include "Tools/Json.h"
#include "Tools/Hex.h"
#include "Tools/Math.h"
#include "Tools/Sha256.h"
#include "ServerHandler.h"
#include "ClientHandler.h"
#include "User.h"
#include "Engine.h"

ServerHandler::~ServerHandler()
{
  for(HashMap<uint64_t, ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    delete *i;
  for(HashMap<String, User*>::Iterator i = users.begin(), end = users.end(); i != end; ++i)
    delete *i;
  for(HashMap<uint32_t, Engine*>::Iterator i = engines.begin(), end = engines.end(); i != end; ++i)
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
  User* user = new User(*this);
  user->userName = userName;
  for(uint32_t* p = (uint32_t*)user->key, * end = (uint32_t*)(user->key + 32); p < end; ++p)
    *p = Math::random();
  Sha256::hmac(user->key, 32, (const byte_t*)(const char_t*)password, password.length(), user->pwhmac);
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

    User* user = new User(*this);
    user->userName = name;
    Memory::copy(user->key, (const byte_t*)key, 32);
    Memory::copy(user->pwhmac, (const byte_t*)pwhmac, 32);
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
    userVar.append("name", user->userName);
    userVar.append("key", Hex::toString(user->key, sizeof(user->key)));
    userVar.append("pwhmac", Hex::toString(user->pwhmac, sizeof(user->pwhmac)));
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
