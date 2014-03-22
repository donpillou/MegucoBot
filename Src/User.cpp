
#include "User.h"
#include "SimSession.h"

class Session {};

User::User() : nextSimSessionId(1), nextSessionId(1) {}

User::~User()
{
  for(HashMap<uint64_t, SimSession*>::Iterator i = simSessions.begin(), end = simSessions.end(); i != end; ++i)
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

uint64_t User::createSimSession(const String& name, const String& engine, double balanceBase, double balanceComm)
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

bool_t User::deleteSimSession(uint64_t id)
{
  HashMap<uint64_t, SimSession*>::Iterator it = simSessions.find(id);
  if(it == simSessions.end())
    return false;
  delete *it;
  simSessions.remove(it);
  return true;
}

uint64_t User::createSession(uint64_t simSessionId, double balanceBase, double balanceComm)
{
  return 0;
}

bool_t User::deleteSession(uint64_t id)
{
  HashMap<uint64_t, Session*>::Iterator it = sessions.find(id);
  if(it == sessions.end())
    return false;
  delete *it;
  sessions.remove(it);
  return true;
}

