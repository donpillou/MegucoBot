
#include <nstd/Console.h>
#include <nstd/File.h>
#include <nstd/Process.h>
#include <nstd/Error.h>
#include <nstd/HashMap.h>

#include "Tools/ProcessManager.h"
#include "Tools/ZlimdbConnection.h"
#include "Tools/ZlimdbProtocol.h"

static class MegucoServer : public ZlimdbConnection::Callback, public ProcessManager::Callback
{
public:
  bool_t init()
  {
    if(!processManager.start(*this))
      return error = Error::getErrorString(), false;
    return true;
  }

  const String& getErrorString() const {return error;}

  bool_t connect()
  {
    connection.close();

    if(!connection.connect(*this))
      return error = connection.getErrorString(), false;

    Buffer buffer(ZLIMDB_MAX_MESSAGE_SIZE);

    if(!connection.createTable("processes", processesTableId))
      return error = connection.getErrorString(), false;
    if(!connection.subscribe(processesTableId))
      return error = connection.getErrorString(), false;

    HashMap<uint32_t, Process> processes;
    String command;
    while(connection.getResponse(buffer))
    {
      void* data = (byte_t*)buffer;
      uint32_t size = buffer.size();
      for(const meguco_process_entity* process; process = (const meguco_process_entity*)zlimdb_get_entity(sizeof(meguco_process_entity), &data, &size);)
      {
        if(!ZlimdbProtocol::getString(process->entity, sizeof(*process), process->cmd_size, command))
          continue;
        Process& processInfo = processes.append(process->process_id, Process());
        processInfo.entityId = process->entity.id;
        processInfo.command = command;
        processInfo.processId = process->process_id;
      }
    }

    // start unknown processes from processes table
    for(HashMap<uint32_t, Process>::Iterator i = processes.begin(), end = processes.end(), next; i != end; i = next)
    {
      next = i;
      ++next;
      Process& process = *i;
      HashMap<uint32_t, Process>::Iterator it = this->processes.find(process.processId);
      if(it == this->processes.end())
      {
        if(!startProcess(process.entityId, process.command))
          return false;
      }
      else
      {
        Process& knownProcess = *it;
        if(process.entityId != knownProcess.entityId)
        {
          if(!connection.remove(processesTableId, process.entityId))
            return error = connection.getErrorString(), false;
          processes.remove(i);
        }
      }
    }

    // add known processes not in processes table to processes table
    for(HashMap<uint32_t, Process>::Iterator i = this->processes.begin(), end = this->processes.end(); i != end; ++i)
    {
      Process& process = *i;
      if(processes.find(process.processId) == processes.end())
      {
        buffer.resize(sizeof(meguco_process_entity) + process.command.length());
        meguco_process_entity* processEntity = (meguco_process_entity*)(byte_t*)buffer;
        ZlimdbProtocol::setEntityHeader(processEntity->entity, 0, 0, buffer.size());
        processEntity->process_id = process.processId;
        ZlimdbProtocol::setString(processEntity->entity, processEntity->cmd_size, sizeof(meguco_process_entity), process.command);
        uint64_t entityId;
        if(!connection.add(processesTableId, processEntity->entity, entityId))
          return error = connection.getErrorString(), false;
        process.entityId = entityId;
      }
    }
    return true;
  }

  bool_t process()
  {
    for(;;)
    {
      if(!connection.process())
        return false;
      List<uint32_t> terminatedProcesses;
      mutex.lock();
      terminatedProcesses.swap(this->terminatedProcesses);
      mutex.unlock();
      for(List<uint32_t>::Iterator i = terminatedProcesses.begin(), end = terminatedProcesses.end(); i != end; ++i)
        removeProcess(*i);
    }
  }

private:
  struct Process
  {
    uint64_t entityId;
    String command;
    uint32_t processId;
  };

private:
  String error;
  ZlimdbConnection connection;
  uint32_t processesTableId;
  ProcessManager processManager;
  HashMap<uint32_t, Process> processes;
  Mutex mutex;
  List<uint32_t> terminatedProcesses;

private:
  void_t removeProcess(uint32_t processId)
  {
    HashMap<uint32_t, Process>::Iterator it = processes.find(processId);
    if(it == processes.end())
      return;
    Process& process = *it;
    connection.remove(processesTableId, process.entityId);
    processes.remove(it);
  }

  bool_t startProcess(uint64_t& entityId, const String& command)
  {
    uint32_t processId;
    if(!processManager.startProcess(command, processId))
      return error = processManager.getErrorString(), false;
    Process& process = processes.append(processId, Process());
    process.processId = processId;
    process.command = command;
    process.entityId = entityId;
    if(!entityId)
    {
      Buffer buffer;
      buffer.resize(sizeof(meguco_process_entity) + command.length());
      meguco_process_entity* processEntity = (meguco_process_entity*)(byte_t*)buffer;
      ZlimdbProtocol::setEntityHeader(processEntity->entity, entityId, 0, buffer.size());
      processEntity->process_id = processId;
      ZlimdbProtocol::setString(processEntity->entity, processEntity->cmd_size, sizeof(meguco_process_entity), command);
      if(!connection.add(processesTableId, processEntity->entity, entityId))
        return error = connection.getErrorString(), false;
      process.entityId = entityId;
    }
    return true;
  }

  bool_t killProcess(uint32_t processId)
  {
    if(!processManager.killProcess(processId))
      return error = processManager.getErrorString(), false;
    return true;
  }

private: // ProcessManager::Callback
  void_t terminatedProcess(uint32_t processId)
  {
    mutex.lock();
    terminatedProcesses.append(processId);
    mutex.unlock();
    connection.interrupt();
  }

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity)
  {
    if(tableId == processesTableId && entity.size >= sizeof(meguco_process_entity))
    {
      meguco_process_entity& process = (meguco_process_entity&)entity;
      if(processes.find(process.process_id) != processes.end())
        return;
      String command;
      if(!ZlimdbProtocol::getString(process.entity, sizeof(meguco_process_entity), process.cmd_size, command))
        return;
      if(!startProcess(process.entity.id, command))
        connection.remove(processesTableId, process.entity.id);
    }
  }

  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity) {}

  virtual void_t removedEntity(uint32_t tableId, const zlimdb_entity& entity)
  {
    if(tableId == processesTableId && entity.size >= sizeof(meguco_process_entity))
    {
      meguco_process_entity& process = (meguco_process_entity&)entity;
      HashMap<uint32_t, Process>::Iterator it = processes.find(process.process_id);
      if(it == processes.end())
        return;
      if(!killProcess(process.process_id))
        return;
      processes.remove(it);
    }
  }
} server;

int_t main(int_t argc, char_t* argv[])
{
  String logFile;
  String binaryDir = File::dirname(String(argv[0], String::length(argv[0])));

  // parse parameters
  {
    Process::Option options[] = {
        {'b', "daemon", Process::argumentFlag | Process::optionalFlag},
        {'h', "help", Process::optionFlag},
    };
    Process::Arguments arguments(argc, argv, options);
    int_t character;
    String argument;
    while(arguments.read(character, argument))
      switch(character)
      {
      case 'b':
        logFile = argument.isEmpty() ? String("MegucoServer.log") : argument;
        break;
      case '?':
        Console::errorf("Unknown option: %s.\n", (const char_t*)argument);
        return -1;
      case ':':
        Console::errorf("Option %s required an argument.\n", (const char_t*)argument);
        return -1;
      default:
        Console::errorf("Usage: %s [-b]\n\
  -b, --daemon[=<file>]   Detach from calling shell and write output to <file>.\n", argv[0]);
        return -1;
      }
  }

  // daemonize process
#ifndef _WIN32
  if(!logFile.isEmpty())
  {
    Console::printf("Starting as daemon...\n");
    if(!Process::daemonize(logFile))
    {
      Console::errorf("error: Could not daemonize process: %s\n", (const char_t*)Error::getErrorString());
      return -1;
    }
  }
#endif

  // initialize process manager
  MegucoServer server;
  if(!server.init())
  {
    Console::errorf("error: Could not initialize process: %s\n", (const char_t*)server.getErrorString());
    return -1;
  }

  // main loop
  for(;; Thread::sleep(10 * 1000))
  {
    // connect to zlimdb server
    if(!server.connect())
    {
        Console::errorf("error: Could not connect to zlimdb server: %s\n", (const char_t*)server.getErrorString());
        return -1;
    }
    Console::printf("Connected to zlimdb server.\n");

    // run connection handler loop
    server.process();

    Console::errorf("error: Lost connection to zlimdb server: %s\n", (const char_t*)server.getErrorString());
  }
  return 0;
}

