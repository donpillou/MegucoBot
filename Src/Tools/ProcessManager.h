
#pragma once

#include <nstd/String.h>
#include <nstd/Thread.h>
#include <nstd/Mutex.h>
#include <nstd/List.h>

class ProcessManager
{
public:
  class Callback
  {
  public:
    virtual void_t terminatedProcess(uint32_t pid) = 0;
  };

public:
  ProcessManager() : nextId(1) {}

  bool_t start(Callback& callback);
  void_t stop();

  bool_t startProcess(const String& commandLine, uint32_t& id);
  bool_t killProcess(uint32_t id);

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
    uint32_t id;
  };

private:
  Thread thread;
  Callback* callback;
  uint32_t nextId;

  Mutex mutex;
  List<Action> actions;

private:
  static uint_t proc(void_t* param) {return ((ProcessManager*)param)->proc();}
  uint_t proc();
};
