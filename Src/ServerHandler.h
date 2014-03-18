
#pragma once

#include <nstd/HashMap.h>

#include "Tools/Server.h"

class ClientHandler;
class SinkClient;

class ServerHandler : public Server::Listener
{
public:
  ServerHandler(uint16_t port) : port(port), nextClientId(1) {}
  ~ServerHandler();

private:
  uint16_t port;
  uint64_t nextClientId;
  HashMap<uint64_t, ClientHandler*> clients;

  virtual void_t acceptedClient(Server::Client& client, uint32_t addr, uint16_t port);
  virtual void_t closedClient(Server::Client& client);
};
