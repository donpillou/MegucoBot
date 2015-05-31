
#pragma once

#include "Tools/ZlimdbConnection.h"

class Session2
{
public:
  Session2(User2& user, uint32_t tableId, const meguco_user_session_entity& marketEntity, const String& executable) : user(user), tableId(tableId), sessionEntity(sessionEntity), executable(executable), processId(0) {}

  User2& getUser() {return user;}
  uint32_t getTableId() const {return tableId;}
  const String& getExecutable() const {return executable;}
  meguco_user_session_state getState() const {return (meguco_user_session_state)sessionEntity.state;}
  void_t setState(meguco_user_session_state state) {sessionEntity.state = state;}
  const zlimdb_entity& getEntity() const {return sessionEntity.entity;}

private:
  User2& user;
  uint32_t tableId;
  meguco_user_session_entity sessionEntity;
  String executable;
  uint32_t processId;
};
