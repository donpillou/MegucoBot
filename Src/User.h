
#pragma once

#include <nstd/String.h>
#include <nstd/HashMap.h>
#include <nstd/HashSet.h>

class ClientHandler;
class SimSession;
class Session;

class User
{
public:
  String userName;
  byte_t key[64];
  byte_t pwhmac[64];

public:
  User();
  ~User();

  void_t addClient(ClientHandler& client);
  void_t removeClient(ClientHandler& client);
  uint64_t createSimSession(const String& name, const String& engine, double balanceBase, double balanceComm);
  bool_t deleteSimSession(uint64_t id);
  uint64_t createSession(uint64_t simSessionId, double balanceBase, double balanceComm);
  bool_t deleteSession(uint64_t id);

private:
  HashSet<ClientHandler*> clients;
  HashMap<uint64_t, SimSession*> simSessions;
  uint64_t nextSimSessionId;
  HashMap<uint64_t, Session*> sessions;
  uint64_t nextSessionId;
};
