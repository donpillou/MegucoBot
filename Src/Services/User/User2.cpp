
#include "User2.h"
#include "Market2.h"
#include "Session2.h"

User2::~User2()
{
  HashSet<Market2*> markets;
  markets.swap(this->markets);
  for(HashSet<Market2*>::Iterator i = markets.begin(), end = markets.end(); i != end; ++i)
    delete *i;
  HashSet<Session2*> sessions;
  sessions.swap(this->sessions);
  for(HashSet<Session2*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
    delete *i;
}

Market2* User2::createMarket(uint32_t tableId, const meguco_user_market_entity& marketEntity, const String& executable)
{
  Market2* market = new Market2(*this, tableId, marketEntity, executable);
  markets.append(market);
  return market;
}

Session2* User2::createSession(uint32_t tableId, const meguco_user_session_entity& sessionEntity, const String& executable)
{
  Session2* session = new Session2(*this, tableId, sessionEntity, executable);
  sessions.append(session);
  return session;
}

void_t User2::deleteMarket(Market2& market)
{
  markets.remove(&market);
  delete &market;
}

void_t User2::deleteSession(Session2& session)
{
  sessions.remove(&session);
  delete &session;
}
