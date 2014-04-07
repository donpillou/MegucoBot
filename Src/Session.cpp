
#include "Session.h"
#include "ServerHandler.h"

Session::Session(ServerHandler& serverHandler, uint32_t id, const String& name, const String& engine, double balanceBase, double balanceComm) :
  serverHandler(serverHandler), id(id), name(name), engine(engine), balanceBase(balanceBase), balanceComm(balanceComm),
  state(BotProtocol::Session::inactive), simulation(true), pid(0), client(0) {}

Session::~Session()
{
  if(pid != 0)
    serverHandler.unregisterSession(pid);
  process.kill();
}

bool_t Session::startSimulation()
{
  if(pid != 0)
    return false;
  pid = process.start(engine);
  if(!pid)
    return false;
  serverHandler.registerSession(pid, *this);
  state = BotProtocol::Session::simulating;
  return true;
}

bool_t Session::stop()
{
  if(pid == 0)
    return false;
  if(!process.kill())
    return false;
  pid = 0;
  state = BotProtocol::Session::inactive;
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

void_t Session::toVariant(Variant& variant)
{
  HashMap<String, Variant>& data = variant.toMap();
  data.append("id", id);
  data.append("name", name);
  data.append("engine", engine);
  data.append("balanceBase", balanceBase);
  data.append("balanceComm", balanceComm);
}
