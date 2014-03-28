

#pragma once

#include <nstd/String.h>
#include <nstd/Process.h>

class ServerHandler;
class ClientHandler;

class Session
{
public:
  enum State
  {
    newState,
    connectedState,
    disconnectedState,
  };

public:
  Session(ServerHandler& serverHandler, uint32_t id, const String& name, bool_t simulation);
  ~Session();

  bool_t start(const String& engine, double balanceBase, double balanceComm);

  bool_t setClient(ClientHandler* client);

  bool_t isSimulation() const {return simulation;}
  const String& getName() const {return name;}
  const String& getEngine() const {return engine;}
  void_t getInitialBalance(double& balanceBase, double& balanceComm) const;

private:
  ServerHandler& serverHandler;
  uint32_t id;
  String name;
  bool_t simulation;
  Process process;
  uint32_t pid;
  State state;
  ClientHandler* client;
  String engine;
  double balanceBase;
  double balanceComm;
};
