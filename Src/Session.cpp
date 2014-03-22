
#include "Session.h"
#include "ServerHandler.h"

Session::Session(ServerHandler& serverHandler, uint32_t id, const String& name, bool_t simulation) : serverHandler(serverHandler), id(id), name(name), simulation(simulation),
  pid(0), state(newState), client(0), balanceBase(0.), balanceComm(0.) {}

Session::~Session()
{
  if(pid != 0)
    serverHandler.unregisterSession(pid);
  process.kill();
}

bool_t Session::start(const String& engine, double balanceBase, double balanceComm)
{
  if(process.isRunning())
    return false;
  pid = process.start(engine);
  if(!pid)
    return false;
  serverHandler.registerSession(pid, *this);
  this->engine = engine;
  this->balanceBase = balanceBase;
  this->balanceComm = balanceComm;
  return true;
}

bool_t Session::setClient(ClientHandler* client)
{
  if(client)
  {
    if(this->client)
      return false;
    this->client = client;
    state = connectedState;
  }
  else
  {
    if(!this->client)
      return false;
    this->client = 0;
    state = disconnectedState;
  }
  return true;
}

void_t Session::getInitialBalance(double& balanceBase, double& balanceComm) const
{
  balanceBase = this->balanceBase;
  balanceComm = this->balanceComm;
}
