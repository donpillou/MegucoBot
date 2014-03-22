
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
  byte_t key[64];
  byte_t pwhmac[64];

public:
  User(ServerHandler& serverHandler);
  ~User();

  void_t registerClient(ClientHandler& client);
  void_t unregisterClient(ClientHandler& client);
  uint32_t createSimSession(const String& name, const String& engine, double balanceBase, double balanceComm);
  bool_t deleteSimSession(uint32_t id);
  uint32_t createSession(const String& name, uint32_t simSessionId, double balanceBase, double balanceComm);
  bool_t deleteSession(uint32_t id);

private:
  ServerHandler& serverHandler;
  HashSet<ClientHandler*> clients;
  HashMap<uint32_t, Session*> simSessions;
  HashMap<uint32_t, Session*> sessions;
  uint32_t nextSessionId;
};
