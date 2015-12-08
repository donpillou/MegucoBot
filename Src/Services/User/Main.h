
#pragma once

#include <nstd/HashMap.h>

#include <megucoprotocol.h>

#include "Tools/ZlimdbConnection.h"

class User2;
class Market2;
class Session2;

class Main : public ZlimdbConnection::Callback
{
public:
  ~Main();

  const String& getErrorString() const {return error;}

  void_t addBrokerType(const String& name, const String& executable);
  void_t addBotType(const String& name, const String& executable);

  bool_t connect();
  void_t disconnect();
  bool_t process();

private:
  struct BrokerType
  {
    String name;
    String executable;
  };

  struct BotType
  {
    String name;
    String executable;
  };

  enum TableType
  {
    user,
    userBroker,
    userSession,
  };

  struct Process
  {
    TableType type;
    uint64_t entityId;
    uint32_t tableId;
  };

  struct TableInfo
  {
    TableType type;
    void_t* object;
  };
  /*
  class User
  {
  public:
    uint64_t maxBrokerId;
    uint64_t maxSessionId;

    User() : maxBrokerId(1), maxSessionId(1) {}
  };
  */

private:
  ZlimdbConnection connection;
  String error;
  HashMap<String, BrokerType> brokerTypesByName;
  HashMap<String, BotType> botTypesByName;
  HashMap<uint64_t, BrokerType*> brokerTypes;
  HashMap<uint64_t, BotType*> botTypes;
  HashMap<String, User2*> users;
  uint32_t processesTableId;
  //uint32_t usersTableId;
  HashMap<uint64_t, Process> processes;
  HashMap<uint32_t, Process*> processesByTable;
  HashMap<uint32_t, TableInfo> tableInfo;

private:
  User2* findUser(const String& name) {return *users.find(name);}
  User2* createUser(const String& name);

  //bool_t setUserBrokerState(Market2& market, meguco_user_broker_state state);
  //bool_t setUserSessionState(Session2& session, meguco_user_session_state state);

  void_t addedTable(uint32_t tableId, const String& tableName);
  void_t addedProcess(uint64_t entityId, const String& command);
  //void_t addedUserBroker(uint32_t tableId, TableInfo& tableInfo, const meguco_user_broker_entity& userMarket);
  //void_t addedUserSession(uint32_t tableId, TableInfo& tableInfo, const meguco_user_session_entity& userSession);
  //void_t updatedUserBroker(Market2& market, const meguco_user_broker_entity& entity);
  //void_t updatedUserSession(Session2& session, const meguco_user_session_entity& entity);
  //void_t removedTable(uint32_t tableId);
  void_t removedProcess(uint64_t entityId);
  //void_t removedUserBroker(Market2& market);
  //void_t removedUserSession(Session2& session);

  void_t controlUser(User2& user, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size);
  void_t controlUserSession(Session2& session, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size);

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity) {}
  virtual void_t removedEntity(uint32_t tableId, uint64_t entityId);
  virtual void_t controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size);
};
