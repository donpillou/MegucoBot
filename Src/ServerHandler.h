
#pragma once

#include <nstd/HashMap.h>

#include "Tools/Server.h"

class ClientHandler;
class User;
class Session;
class Engine;
class MarketAdapter;
class Market;

class ServerHandler : public Server::Listener
{
public:
  ServerHandler(uint16_t port) : port(port), nextEntityId(1) {}
  ~ServerHandler();

  void_t addEngine(const String& name, const String& path);
  const HashMap<uint32_t, Engine*>& getEngines() const {return engines;}
  Engine* findEngine(const String& name) const {return *enginesByName.find(name);}
  Engine* findEngine(uint32_t id) const {return *engines.find(id);}

  bool_t addUser(const String& userName, const String& password);
  User* findUser(const String& userName) {return *users.find(userName);}

  void_t registerSession(uint32_t pid, Session& session) {sessionsByPid.append(pid, &session);}
  void_t unregisterSession(uint32_t pid) {sessionsByPid.remove(pid);}
  Session* findSessionByPid(uint32_t pid) {return *sessionsByPid.find(pid);}

  void_t registerMarket(uint32_t pid, Market& market) {marketsByPid.append(pid, &market);}
  void_t unregisterMarket(uint32_t pid) {marketsByPid.remove(pid);}
  Market* findMarketByPid(uint32_t pid) {return *marketsByPid.find(pid);}

  void_t addMarketAdapter(const String& name, const String& path, const String& currencyBase, const String& currencyComm);
  const HashMap<uint32_t, MarketAdapter*>& getMarketAdapters() const {return marketAdapters;}
  MarketAdapter* findMarketAdapter(const String& name) const {return *marketAdaptersByName.find(name);}
  MarketAdapter* findMarketAdapter(uint32_t id) const {return *marketAdapters.find(id);}

  bool_t loadData();
  bool_t saveData();

private:
  uint16_t port;
  uint32_t nextEntityId;
  HashMap<uint64_t, ClientHandler*> clients;
  HashMap<String, User*> users;
  HashMap<uint32_t, Session*> sessionsByPid;
  HashMap<uint32_t, Market*> marketsByPid;
  HashMap<uint32_t, Engine*> engines;
  HashMap<String, Engine*> enginesByName;
  HashMap<uint32_t, MarketAdapter*> marketAdapters;
  HashMap<String, MarketAdapter*> marketAdaptersByName;

  virtual void_t acceptedClient(Server::Client& client, uint32_t addr, uint16_t port);
  virtual void_t closedClient(Server::Client& client);
};
