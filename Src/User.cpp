
#include <nstd/File.h>
#include <nstd/Variant.h>

#include "Tools/Json.h"

#include "User.h"
#include "Session.h"
#include "Market.h"
#include "ClientHandler.h"

User::User(ServerHandler& serverHandler, const String& userName, const byte_t (&key)[32], const byte_t (&pwhmac)[32]) :
  serverHandler(serverHandler), userName(userName), nextEntityId(1)
{
  Memory::copy(this->key, key, sizeof(this->key));
  Memory::copy(this->pwhmac, pwhmac, sizeof(this->pwhmac));
}

User::~User()
{
  for(HashMap<uint32_t, Session*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
    delete *i;
  for(HashMap<uint32_t, Market*>::Iterator i = markets.begin(), end = markets.end(); i != end; ++i)
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

Session* User::createSession(const String& name, Engine& engine, MarketAdapter& marketAdapater, double balanceBase, double balanceComm)
{
  uint32_t id = nextEntityId++;
  Session* session = new Session(serverHandler, *this, id, name, engine, marketAdapater, balanceBase, balanceComm);
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

Market* User::createMarket(MarketAdapter& marketAdapter, const String& username, const String& key, const String& secret)
{
  uint32_t id = nextEntityId++;
  Market* market = new Market(serverHandler, id, marketAdapter, username, key, secret);
  if(!market->start())
  {
    delete market;
    return 0;
  }
  markets.append(id, market);
  return market;
}

bool_t User::deleteMarket(uint32_t id)
{
  HashMap<uint32_t, Market*>::Iterator it = markets.find(id);
  if(it == markets.end())
    return false;
  delete *it;
  markets.remove(it);
  return true;
}

void_t User::sendEntity(BotProtocol::EntityType type, uint32_t id, const void_t* data, size_t size)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->sendEntity(type, id, data, size);
}

void_t User::removeEntity(BotProtocol::EntityType type, uint32_t id)
{
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->removeEntity(type, id);
}

bool_t User::loadData()
{
  File file;
  if(!file.open(String("user-") + userName + ".json", File::readFlag))
    return false;
  String data;
  if(!file.readAll(data))
    return false;
  Variant dataVar;
  if(!Json::parse(data, dataVar))
    return false;
{
    const List<Variant>& marketsVar = dataVar.toMap().find("markets")->toList();
    for(List<Variant>::Iterator i = marketsVar.begin(), end = marketsVar.end(); i != end; ++i)
    {
      Market* market = new Market(serverHandler, *i);
      uint32_t id = market->getId();
      if(markets.find(id) != markets.end() || !market->getMarketAdapter() ||
         !market->start())
      {
        delete market;
        continue;
      }
      markets.append(id, market);
      if(id >= nextEntityId)
        nextEntityId = id + 1;
    }
  }
  {
    const List<Variant>& sessionsVar = dataVar.toMap().find("sessions")->toList();
    for(List<Variant>::Iterator i = sessionsVar.begin(), end = sessionsVar.end(); i != end; ++i)
    {
      Session* session = new Session(serverHandler, *this, *i);
      uint32_t id = session->getId();
      if(sessions.find(id) != sessions.end() || !session->getEngine() || !session->getMarketAdapter())
      {
        delete session;
        continue;
      }
      sessions.append(id, session);
      if(id >= nextEntityId)
        nextEntityId = id + 1;
    }
  }
  return true;
}

bool_t User::saveData()
{
  Variant dataVar;
  HashMap<String, Variant>& data = dataVar.toMap();
  {
    List<Variant>& marketsVar = data.append("markets", Variant()).toList();
    for(HashMap<uint32_t, Market*>::Iterator i = markets.begin(), end = markets.end(); i != end; ++i)
    {
      Variant& marketVar = marketsVar.append(Variant());;
      (*i)->toVariant(marketVar);
    }
  }
  {
    List<Variant>& sessionsVar = data.append("sessions", Variant()).toList();
    for(HashMap<uint32_t, Session*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
    {
      Variant& sessionVar = sessionsVar.append(Variant());;
      (*i)->toVariant(sessionVar);
    }
  }
  String json;
  if(!Json::generate(dataVar, json))
    return false;
  File file;
  if(!file.open(String("user-") + userName + ".json", File::writeFlag))
    return false;
  if(!file.write(json))
    return false;
  return true;
}

