
#pragma once

#include <nstd/String.h>
#include <nstd/HashMap.h>
#include <nstd/HashSet.h>

#include "BotProtocol.h"

class ServerHandler;
class ClientHandler;
class Session;

class User
{
public:
  String userName;
  byte_t key[32];
  byte_t pwhmac[32];

public:
  User(ServerHandler& serverHandler);
  ~User();

  void_t registerClient(ClientHandler& client);
  void_t unregisterClient(ClientHandler& client);
  Session* createSession(const String& name, const String& engine, double balanceBase, double balanceComm);
  Session* findSession(uint32_t id);
  bool_t deleteSession(uint32_t id);

  void_t sendEntity(BotProtocol::EntityType type, uint32_t id, const void_t* data, size_t size);
  void_t removeEntity(BotProtocol::EntityType type, uint32_t id);
  
  const HashMap<uint32_t, Session*>& getSessions() const {return sessions;}
  
  bool_t saveData();

private:
  ServerHandler& serverHandler;
  HashSet<ClientHandler*> clients;
  HashMap<uint32_t, Session*> sessions;
  uint32_t nextSessionId;
};
