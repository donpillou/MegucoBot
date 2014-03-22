
#include "User.h"
#include "SimSession.h"
#include "Session.h"

User::User() : nextSimSessionId(1), nextSessionId(1) {}

User::~User()
{
  for(HashMap<uint32_t, SimSession*>::Iterator i = simSessions.begin(), end = simSessions.end(); i != end; ++i)
    delete *i;
}

void_t User::addClient(ClientHandler& client)
{
  clients.append(&client);
}

void_t User::removeClient(ClientHandler& client)
{
  clients.remove(&client);
}

uint32_t User::createSimSession(const String& name, const String& engine, double balanceBase, double balanceComm)
{
  uint32_t id = nextSimSessionId++;
  SimSession* simSession = new SimSession(id, name);
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
  HashMap<uint32_t, SimSession*>::Iterator it = simSessions.find(id);
  if(it == simSessions.end())
    return false;
  delete *it;
  simSessions.remove(it);
  return true;
}

uint32_t User::createSession(const String& name, uint32_t simSessionId, double balanceBase, double balanceComm)
{
  HashMap<uint32_t, SimSession*>::Iterator it = simSessions.find(simSessionId);
  if(it == simSessions.end())
    return 0;
  SimSession* simSession = *it;


  uint32_t id = nextSessionId++;
  Session* session = new Session(id, name);
  if(!session->start(simSession->getEngineName(), balanceBase, balanceComm))
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

