
#include "Session.h"
#include "ServerHandler.h"

Session::Session(ServerHandler& serverHandler, uint32_t id, const String& name, const String& engine) :
  serverHandler(serverHandler), id(id), name(name), engine(engine), state(BotProtocol::Session::inactive), simulation(true),
  pid(0), client(0), balanceBase(0.), balanceComm(0.) {}

Session::~Session()
{
  if(pid != 0)
    serverHandler.unregisterSession(pid);
  process.kill();
}

bool_t Session::start(double balanceBase, double balanceComm)
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
  }
  else
  {
    if(!this->client)
      return false;
    this->client = 0;
  }
  return true;
}

void_t Session::getInitialBalance(double& balanceBase, double& balanceComm) const
{
  balanceBase = this->balanceBase;
  balanceComm = this->balanceComm;
}
