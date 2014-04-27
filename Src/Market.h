

#pragma once

#include <nstd/String.h>
#include <nstd/Process.h>
#include <nstd/Variant.h>
#include <nstd/HashSet.h>

#include "BotProtocol.h"

class ServerHandler;
class MarketAdapter;
class ClientHandler;
class User;

class Market
{
public:
  Market(ServerHandler& serverHandler, User& user, uint32_t id, MarketAdapter& marketAdapter, const String& username, const String& key, const String& secret);
  Market(ServerHandler& serverHandler, User& user, const Variant& variant);
  ~Market();
  void_t toVariant(Variant& variant);

  bool_t start();
  bool_t stop();

  bool_t registerClient(ClientHandler& client, bool_t adapter);
  void_t unregisterClient(ClientHandler& client);

  uint32_t getId() const {return id;}
  MarketAdapter* getMarketAdapter() const {return marketAdapter;}
  BotProtocol::Market::State getState() const {return state;}

  void_t send(ClientHandler* client = 0);
  void_t sendEntity(const void_t* data, size_t size);
  void_t removeEntity(BotProtocol::EntityType type, uint32_t id);

private:
  ServerHandler& serverHandler;
  User& user;
  uint32_t id;
  MarketAdapter* marketAdapter;
  String username;
  String key;
  String secret;
  BotProtocol::Market::State state;
  Process process;
  uint32_t pid;
  ClientHandler* adapterClient;
  HashSet<ClientHandler*> clients;
  uint32_t nextEntityId;
};
