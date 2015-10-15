
#pragma once

#include "Tools/ZlimdbConnection.h"

class Session2
{
public:
  Session2(User2& user, uint32_t tableId, const meguco_user_session_entity& sessionEntity, const String& executable) : user(user), tableId(tableId), executable(executable), processId(0)
  {
    this->sessionEntity.assign((const byte_t*)&sessionEntity, sessionEntity.entity.size);
  }

  User2& getUser() {return user;}
  uint32_t getTableId() const {return tableId;}
  const String& getExecutable() const {return executable;}
  meguco_user_session_state getState() const {return (meguco_user_session_state)((const meguco_user_session_entity*)(const byte_t*)sessionEntity)->state;}
  void_t setState(meguco_user_session_state state) {((meguco_user_session_entity*)(byte_t*)sessionEntity)->state = state;}
  const zlimdb_entity& getEntity() const {return ((const meguco_user_session_entity*)(const byte_t*)sessionEntity)->entity;}

private:
  User2& user;
  uint32_t tableId;
  Buffer sessionEntity;
  String executable;
  uint32_t processId;
};
