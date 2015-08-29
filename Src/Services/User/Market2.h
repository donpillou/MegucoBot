
#pragma once

#include <megucoprotocol.h>

#include "Tools/ZlimdbConnection.h"

class User2;

class Market2
{
public:
  Market2(User2& user, uint32_t tableId, const meguco_user_broker_entity& brokerEntity, const String& executable) : user(user), tableId(tableId), brokerEntity(brokerEntity), executable(executable), processId(0) {}

  User2& getUser() {return user;}
  uint32_t getTableId() const {return tableId;}
  const String& getExecutable() const {return executable;}
  meguco_user_broker_state getState() const {return (meguco_user_broker_state)brokerEntity.state;}
  void_t setState(meguco_user_broker_state state) {brokerEntity.state = state;}
  const zlimdb_entity& getEntity() const {return brokerEntity.entity;}

private:
  User2& user;
  uint32_t tableId;
  meguco_user_broker_entity brokerEntity;
  String executable;
  uint32_t processId;
};
