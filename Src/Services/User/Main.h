
#pragma once

#include <nstd/HashMap.h>

#include <megucoprotocol.h>

#include "Tools/ZlimdbConnection.h"

class User;
class Session;

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
    String command;
    uint64_t entityId;
    void_t* object;
  };

  struct TableInfo
  {
    TableType type;
    void_t* object;
  };

private:
  ZlimdbConnection connection;
  String error;
  HashMap<String, BrokerType> brokerTypesByName;
  HashMap<String, BotType> botTypesByName;
  HashMap<uint64_t, BrokerType*> brokerTypes;
  HashMap<uint64_t, BotType*> botTypes;
  HashMap<String, User *> users;
  uint32_t processesTableId;
  HashMap<uint64_t, Process> processes;
  HashMap<String, Process*> processesByCommand;
  HashMap<uint32_t, TableInfo> tableInfo;

private:
  User * findUser(const String& name) {return *users.find(name);}
  User * createUser(const String& name);

  void_t addedTable(uint32_t tableId, const String& tableName);
  void_t addedProcess(uint64_t entityId, const String& command);
  void_t removedProcess(uint64_t entityId);

  void_t controlUser(User & user, uint32_t requestId, uint32_t controlCode, const byte_t* data, size_t size);
  void_t controlUserSession(Session& session, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size);

  static String getArg(const String& str, size_t& pos, char_t separator)
  {
    const char_t* end = str.find(separator, pos);
    if(end)
    {
      size_t len = end - ((const char_t*)str + pos);
      String result = str.substr(pos, len);
      pos += len + 1;
      return result;
    }
    String result = str.substr(pos);
    pos = str.length();
    return result;
  }

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity) {}
  virtual void_t removedEntity(uint32_t tableId, uint64_t entityId);
  virtual void_t controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size);
};
