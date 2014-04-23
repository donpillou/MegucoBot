
#pragma once

#include <nstd/String.h>
#include <nstd/HashMap.h>
#include <nstd/HashSet.h>

#include "BotProtocol.h"

class ServerHandler;
class ClientHandler;
class Session;
class Engine;
class MarketAdapter;

class User
{
public:
  User(ServerHandler& serverHandler, const String& userName, const byte_t (&key)[32], const byte_t (&pwhmac)[32]);
  ~User();

  void_t registerClient(ClientHandler& client);
  void_t unregisterClient(ClientHandler& client);
  Session* createSession(const String& name, Engine& engine, MarketAdapter& marketAdapater, double balanceBase, double balanceComm);
  Session* findSession(uint32_t id);
  bool_t deleteSession(uint32_t id);

  void_t sendEntity(BotProtocol::EntityType type, uint32_t id, const void_t* data, size_t size);
  void_t removeEntity(BotProtocol::EntityType type, uint32_t id);
  
  const String& getUserName() const {return userName;}
  const byte_t* getKey() const {return key;}
  const byte_t* getPwHmac() const {return pwhmac;}
  const HashMap<uint32_t, Session*>& getSessions() const {return sessions;}
  
  bool_t loadData();
  bool_t saveData();

private:
  ServerHandler& serverHandler;
  String userName;
  byte_t key[32];
  byte_t pwhmac[32];
  HashSet<ClientHandler*> clients;
  HashMap<uint32_t, Session*> sessions;
  uint32_t nextEntityId;
};
