
#pragma once

#include <nstd/HashMap.h>

#include "Tools/Server.h"

class ClientHandler;
class User;
class Session;
class Engine;
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

  void_t registerSession(uint32_t pid, Session& session) {sessions.append(pid, &session);}
  void_t unregisterSession(uint32_t pid) {sessions.remove(pid);}
  Session* findSessionByPid(uint32_t pid) {return *sessions.find(pid);}

  void_t addMarket(const String& name, const String& currencyBase, const String& currencyComm);
  const HashMap<uint32_t, Market*>& getMarkets() const {return markets;}
  Market* findMarket(const String& name) const {return *marketsByName.find(name);}
  Market* findMarket(uint32_t id) const {return *markets.find(id);}

  bool_t loadData();
  bool_t saveData();

private:
  uint16_t port;
  uint32_t nextEntityId;
  HashMap<uint64_t, ClientHandler*> clients;
  HashMap<String, User*> users;
  HashMap<uint32_t, Session*> sessions;
  HashMap<uint32_t, Engine*> engines;
  HashMap<String, Engine*> enginesByName;
  HashMap<uint32_t, Market*> markets;
  HashMap<String, Market*> marketsByName;

  virtual void_t acceptedClient(Server::Client& client, uint32_t addr, uint16_t port);
  virtual void_t closedClient(Server::Client& client);
};
