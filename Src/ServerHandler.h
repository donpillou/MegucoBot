
#pragma once

#include <nstd/HashMap.h>

#include "Tools/Server.h"

class ClientHandler;
class User;
class Session;
class BotEngine;
class MarketAdapter;
class Market;

class ServerHandler : public Server::Listener
{
public:
  ServerHandler(uint16_t port);
  ~ServerHandler();

  void_t addBotEngine(const String& name, const String& path);
  const HashMap<uint32_t, BotEngine*>& getBotEngines() const {return botEngines;}
  BotEngine* findBotEngine(const String& name) const {return *botEnginesByName.find(name);}
  BotEngine* findBotEngine(uint32_t id) const {return *botEngines.find(id);}

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

  uint32_t createRequestId(uint32_t requesterRequestId, ClientHandler& requester, ClientHandler& requestee);
  bool_t findAndRemoveRequestId(uint32_t requestId, uint32_t& requesterRequestId, ClientHandler*& requester);
  void_t removeRequestId(uint32_t requesteeRequestId);

private:
  class ClientData
  {
  public:
    HashSet<uint32_t> requestIds;
  };

  class RequestId
  {
  public:
    ClientHandler* requester;
    ClientHandler* requestee;
    uint32_t requesterRequestId;
  };

private:
  uint16_t port;
  uint32_t nextEntityId;
  HashMap<ClientHandler*, ClientData> clients;
  HashMap<String, User*> users;
  HashMap<uint32_t, Session*> sessionsByPid;
  HashMap<uint32_t, Market*> marketsByPid;
  HashMap<uint32_t, BotEngine*> botEngines;
  HashMap<String, BotEngine*> botEnginesByName;
  HashMap<uint32_t, MarketAdapter*> marketAdapters;
  HashMap<String, MarketAdapter*> marketAdaptersByName;
  HashMap<uint32_t, RequestId> requestIds;
  uint32_t nextRequestId;

private: // Server::Listener
  virtual void_t acceptedClient(Server::Client& client, uint32_t addr, uint16_t port);
  virtual void_t closedClient(Server::Client& client);
};
