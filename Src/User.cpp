
#include "User.h"
#include "Session.h"

User::User(ServerHandler& serverHandler) : serverHandler(serverHandler), nextSessionId(1) {}

User::~User()
{
  for(HashMap<uint32_t, Session*>::Iterator i = simSessions.begin(), end = simSessions.end(); i != end; ++i)
    delete *i;
  for(HashMap<uint32_t, Session*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
    delete *i;
}

void_t User::registerClient(ClientHandler& client)
{
  clients.append(&client);
}

void_t User::unregisterClient(ClientHandler& client)
{
  clients.remove(&client);
}

uint32_t User::createSimSession(const String& name, const String& engine, double balanceBase, double balanceComm)
{
  uint32_t id = nextSessionId++;
  Session* simSession = new Session(serverHandler, id, name, true);
  if(!simSession->start(engine, balanceBase, balanceComm))
  {
    delete simSession;
    return 0;
  }
  simSessions.append(id, simSession);
  return id;
}

bool_t User::deleteSimSession(uint32_t id)
{
  HashMap<uint32_t, Session*>::Iterator it = simSessions.find(id);
  if(it == simSessions.end())
    return false;
  delete *it;
  simSessions.remove(it);
  return true;
}

uint32_t User::createSession(const String& name, uint32_t simSessionId, double balanceBase, double balanceComm)
{
  HashMap<uint32_t, Session*>::Iterator it = simSessions.find(simSessionId);
  if(it == simSessions.end())
    return 0;
  Session* simSession = *it;


  uint32_t id = nextSessionId++;
  Session* session = new Session(serverHandler, id, name, false);
  if(!session->start(simSession->getEngine(), balanceBase, balanceComm))
  {
    delete session;
    return 0;
  }
  sessions.append(id, session);
  return id;
}

bool_t User::deleteSession(uint32_t id)
{
  HashMap<uint32_t, Session*>::Iterator it = sessions.find(id);
  if(it == sessions.end())
    return false;
  delete *it;
  sessions.remove(it);
  return true;
}

