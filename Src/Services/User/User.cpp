
#include "User.h"
#include "Broker.h"
#include "Session.h"

User::~User()
{
  HashMap<uint64_t, Broker*> brokers;
  brokers.swap(this->brokers);
  for(HashMap<uint64_t, Broker*>::Iterator i = brokers.begin(), end = brokers.end(); i != end; ++i)
    delete *i;
  HashMap<uint64_t, Session*> sessions;
  sessions.swap(this->sessions);
  for(HashMap<uint64_t, Session*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
    delete *i;
}

Broker*User::createBroker(uint64_t brokerId)
{
  if(brokerId > maxBrokerId)
    maxBrokerId = brokerId;
  Broker* broker = new Broker(*this, brokerId);
  brokers.append(brokerId, broker);
  return broker;
}

Session*User::createSession(uint64_t sessionId)
{
  if(sessionId > maxSessionId)
    maxSessionId = sessionId;
  Session* session = new Session(*this, sessionId);
  sessions.append(sessionId, session);
  return session;
}

void_t User::deleteBroker(Broker& market)
{
  brokers.remove(market.getBrokerId());
  delete &market;
}

void_t User::deleteSession(Session& session)
{
  sessions.remove(session.getSessionId());
  delete &session;
}
