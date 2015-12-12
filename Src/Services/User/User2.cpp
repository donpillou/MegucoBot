
#include "User2.h"
#include "Broker.h"
#include "Session2.h"

User2::~User2()
{
  HashMap<uint64_t, Broker*> brokers;
  brokers.swap(this->brokers);
  for(HashMap<uint64_t, Broker*>::Iterator i = brokers.begin(), end = brokers.end(); i != end; ++i)
    delete *i;
  HashMap<uint64_t, Session2*> sessions;
  sessions.swap(this->sessions);
  for(HashMap<uint64_t, Session2*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
    delete *i;
}

Broker* User2::createBroker(uint64_t brokerId)
{
  if(brokerId > maxBrokerId)
    maxBrokerId = brokerId;
  Broker* broker = new Broker(*this, brokerId);
  brokers.append(brokerId, broker);
  return broker;
}

Session2* User2::createSession(uint64_t sessionId)
{
  if(sessionId > maxSessionId)
    maxSessionId = sessionId;
  Session2* session = new Session2(*this, sessionId);
  sessions.append(sessionId, session);
  return session;
}

void_t User2::deleteBroker(Broker& market)
{
  brokers.remove(market.getBrokerId());
  delete &market;
}

void_t User2::deleteSession(Session2& session)
{
  sessions.remove(session.getSessionId());
  delete &session;
}
