
#pragma once

#include <nstd/HashMap.h>

#include "Tools/ZlimdbConnection.h"
#include "Tools/ProcessManager.h"

class User2;

class ConnectionHandler : public ZlimdbConnection::Callback
{
public:
  ConnectionHandler() {}
  ~ConnectionHandler();

  const String& getErrorString() const {return error;}

  void_t addBotMarket(const String& name, const String& executable);
  void_t addBotEngine(const String& name, const String& executable);

  bool_t connect();
  bool_t process();

private:
  struct BotMarket
  {
    String name;
    String executable;
  };

  struct BotEngine
  {
    String name;
    String executable;
  };

private:
  ZlimdbConnection connection;
  String error;
  ProcessManager processManager;
  HashMap<String, BotMarket> botMarketsByName;
  HashMap<String, BotEngine> botEnginesByName;
  HashMap<uint64_t, BotMarket*> botMarkets;
  HashMap<uint64_t, BotEngine*> botEngines;
  //uint32_t botMarketsTableId;
  //uint32_t botEnginesTableId;
  //HashMap<uint64_t, String> userMarkets;
  //HashMap<uint64_t, String> userSessions;
  HashMap<String, User2*> users;

private:
  User2* findUser(const String& name) {return *users.find(name);}
  User2* createUser(const String& name);

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t removedEntity(uint32_t tableId, const zlimdb_entity& entity);
};
