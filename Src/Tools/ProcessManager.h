
#pragma once

#include <nstd/String.h>
#include <nstd/Thread.h>
#include <nstd/Mutex.h>
#include <nstd/List.h>
#include <nstd/Array.h>

class Process;

class ProcessManager
{
public:
  class Callback
  {
  public:
    virtual void_t terminatedProcess(uint64_t entityId) = 0;
  };

public:
  const String& getErrorString() const {return error;}

  bool_t start(Callback& callback);
  void_t stop();

  void_t startProcess(uint64_t id, const String& commandLine);
  void_t setProcessId(uint64_t id, uint64_t newId);
  void_t killProcess(uint64_t id);

private:
  struct Action
  {
    enum Type
    {
      startType,
      killType,
      quitType,
    } type;
    String commandLine;
    uint64_t id;
  };

private:
  String error;
  Thread thread;
  Callback* callback;

  Mutex mutex;
  List<Action> actions;
  Array<Process*> processes;
  HashMap<Process*, uint64_t> idsByProcessMap;
  HashMap<uint64_t, Process*> processesByIdMap;

private:
  static uint_t proc(void_t* param) {return ((ProcessManager*)param)->proc();}
  uint_t proc();
};
