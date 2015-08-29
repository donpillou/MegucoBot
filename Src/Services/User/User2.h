
#pragma once

#include <nstd/HashSet.h>

#include <megucoprotocol.h>

#include "Tools/ZlimdbConnection.h"

class Market2;
class Session2;

class User2
{
public:
  User2() {}
  ~User2();

  Market2* createBroker(uint32_t tableId, const meguco_user_broker_entity& brokerEntity, const String& executable);
  Session2* createSession(uint32_t tableId, const meguco_user_session_entity& sessionEntity, const String& executable);
  void_t deleteMarket(Market2& market);
  void_t deleteSession(Session2& session);

private:
  HashSet<Market2*> markets;
  HashSet<Session2*> sessions;
};
