
#pragma once

#include "Tools/ZlimdbProtocol.h"

class User2;

class Market2
{
public:
  Market2(User2& user, uint32_t tableId, const meguco_user_market_entity& marketEntity, const String& executable) : user(user), tableId(tableId), marketEntity(marketEntity), executable(executable), processId(0) {}

  User2& getUser() {return user;}
  uint32_t getTableId() const {return tableId;}
  const String& getExecutable() const {return executable;}
  meguco_user_market_state getState() const {return (meguco_user_market_state)marketEntity.state;}
  void_t setState(meguco_user_market_state state) {marketEntity.state = state;}
  const zlimdb_entity& getEntity() const {return marketEntity.entity;}

private:
  User2& user;
  uint32_t tableId;
  meguco_user_market_entity marketEntity;
  String executable;
  uint32_t processId;
};
