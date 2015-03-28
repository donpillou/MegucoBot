
#pragma once

#include <nstd/HashMap.h>

#include "Tools/ZlimdbConnection.h"
#include "Tools/ProcessManager.h"

class User2;
class Market2;

class ConnectionHandler : public ZlimdbConnection::Callback, public ProcessManager::Callback
{
public:
  ConnectionHandler() {}
  ~ConnectionHandler();

  bool_t init();

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
  HashMap<String, User2*> users;
  Mutex mutex;
  List<uint32_t> terminatedProcesses;

private:
  User2* findUser(const String& name) {return *users.find(name);}
  User2* createUser(const String& name);
  void_t registerMarketProcess(uint32_t pid, Market2& market);
  void_t unregisterMarketProcess(uint32_t pid);

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t removedEntity(uint32_t tableId, const zlimdb_entity& entity);

private: // ProcessManager::Callback
  virtual void_t terminatedProcess(uint32_t pid);
};
