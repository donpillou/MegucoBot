
#pragma once

#include <nstd/String.h>
#include <nstd/HashMap.h>
#include <nstd/HashSet.h>

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
  uint32_t createSession(const String& name, const String& engine, double balanceBase, double balanceComm);
  bool_t deleteSession(uint32_t id);

  void_t sendClients(const byte_t* data, size_t size);
  
  const HashMap<uint32_t, Session*>& getSessions() const {return sessions;}

private:
  ServerHandler& serverHandler;
  HashSet<ClientHandler*> clients;
  HashMap<uint32_t, Session*> sessions;
  uint32_t nextSessionId;
};
