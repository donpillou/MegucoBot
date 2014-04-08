
#include <nstd/File.h>
#include <nstd/Variant.h>

#include "Tools/Json.h"

#include "User.h"
#include "Session.h"
#include "ClientHandler.h"

User::User(ServerHandler& serverHandler) : serverHandler(serverHandler), nextSessionId(1) {}

User::~User()
{
  for(HashMap<uint32_t, Session*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
    delete *i;
}

void_t User::registerClient(ClientHandler& client)
{
  clients.append(&client);
}

void_t User::unregisterClient(ClientHandler& client)
{
  clients.remove(&client);
}

Session* User::createSession(const String& name, const String& engine, double balanceBase, double balanceComm)
{
  uint32_t id = nextSessionId++;
  Session* session = new Session(serverHandler, id, name, engine, balanceBase, balanceComm);
  sessions.append(id, session);
  return session;
}

Session* User::findSession(uint32_t id)
{
  return *sessions.find(id);
}

bool_t User::deleteSession(uint32_t id)
{
  HashMap<uint32_t, Session*>::Iterator it = sessions.find(id);
  if(it == sessions.end())
    return false;
  delete *it;
  sessions.remove(it);
  return true;
}

void_t User::sendEntity(BotProtocol::EntityType type, uint32_t id, const void_t* data, size_t size)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
  {
    ClientHandler* clientHandler = *i;
    clientHandler->sendEntity(type, id, data, size);
  }
}

void_t User::removeEntity(BotProtocol::EntityType type, uint32_t id)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
  {
    ClientHandler* clientHandler = *i;
    clientHandler->removeEntity(type, id);
  }
}

bool_t User::loadData()
{
  File file;
  if(!file.open(userName + ".json", File::readFlag))
    return false;
  String data;
  if(!file.readAll(data))
    return false;
  Variant dataVar;
  if(!Json::parse(data, dataVar))
    return false;
  const List<Variant>& sessionsVar = dataVar.toMap().find("sessions")->toList();
  for(List<Variant>::Iterator i = sessionsVar.begin(), end = sessionsVar.end(); i != end; ++i)
  {
    Session* session = new Session(serverHandler, *i);
    uint32_t id = session->getId();
    if(sessions.find(id) != sessions.end())
    {
      delete session;
      continue;
    }
    sessions.append(id, session);
    nextSessionId = id + 1;
  }
}

bool_t User::saveData()
{
  Variant dataVar;
  HashMap<String, Variant>& data = dataVar.toMap();
  List<Variant>& sessionsVar = data.append("sessions", Variant()).toList();
  for(HashMap<uint32_t, Session*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
  {
    Variant& sessionVar = sessionsVar.append(Variant());;
    (*i)->toVariant(sessionVar);
  }
  String json;
  if(!Json::generate(data, json))
    return false;
  File file;
  if(!file.open(userName + ".json", File::writeFlag))
    return false;
  if(!file.write(json))
    return false;
  return true;
}

