
#pragma once

#include <nstd/HashMap.h>

#include "Tools/Server.h"

class ClientHandler;
class User;
class Session;

class ServerHandler : public Server::Listener
{
public:
  ServerHandler(uint16_t port) : port(port), nextClientId(1) {}
  ~ServerHandler();

  void_t addEngine(const String& engine) {engines.append(engine);}
  const List<String>& getEngines() const {return engines;}
  bool_t addUser(const String& userName, const String& password);
  User* findUser(const String& userName);
  Session* findSession(uint32_t pid);
  void_t registerSession(uint32_t pid, Session& session);
  void_t unregisterSession(uint32_t pid);

private:
  uint16_t port;
  uint64_t nextClientId;
  HashMap<uint64_t, ClientHandler*> clients;
  HashMap<String, User*> users;
  HashMap<uint32_t, Session*> sessions;
  List<String> engines;

  virtual void_t acceptedClient(Server::Client& client, uint32_t addr, uint16_t port);
  virtual void_t closedClient(Server::Client& client);
};
