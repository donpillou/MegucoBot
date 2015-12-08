
#pragma once

#include <nstd/HashMap.h>

#include <megucoprotocol.h>

#include "Tools/ZlimdbConnection.h"

class Market2;
class Session2;

class User2
{
public:
  User2(const String& name) : name(name), maxBrokerId(0), maxSessionId(0) {}
  ~User2();

  const String& getName() const {return name;}
  uint64_t getNewBrokerId() {return ++maxBrokerId;}
  uint64_t getNewSessionId() {return ++maxSessionId;}

  Market2* findBroker(uint64_t brokerId) {return *brokers.find(brokerId);}
  Market2* createBroker(uint64_t brokerId);
  void_t deleteBroker(Market2& market);

  Session2* findSession(uint64_t sessionId) {return *sessions.find(sessionId);}
  Session2* createSession(uint64_t sessionId);
  void_t deleteSession(Session2& session);

private:
  const String name;
  HashMap<uint64_t, Market2*> brokers;
  HashMap<uint64_t, Session2*> sessions;
  uint64_t maxBrokerId;
  uint64_t maxSessionId;
};
