
#include <nstd/Log.h>
#include <nstd/Console.h>
#include <nstd/File.h>
#include <nstd/Process.h>
#include <nstd/Error.h>

#include <megucoprotocol.h>

#include "Main.h"

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
  Log::setFormat("%P> %m");

  // daemonize process
#ifndef _WIN32
  if(!logFile.isEmpty())
  {
    Log::infof("Starting as daemon...");
    if(!Process::daemonize(logFile))
    {
      Log::errorf("Could not daemonize process: %s", (const char_t*)Error::getErrorString());
      return -1;
    }
  }
#endif

  // initialize process manager
  Main server(binaryDir);
  if(!server.init())
  {
    Log::errorf("Could not initialize process: %s", (const char_t*)server.getErrorString());
    return -1;
  }

  // main loop
  for(;; Thread::sleep(10 * 1000))
  {
    // connect to zlimdb server
    if(!server.connect())
    {
        Log::errorf("Could not connect to zlimdb server: %s", (const char_t*)server.getErrorString());
        continue;
    }
    Log::infof("Connected to zlimdb server.");

    // run connection handler loop
    server.process();

    Log::errorf("Lost connection to zlimdb server: %s", (const char_t*)server.getErrorString());
  }
  return 0;
}

bool_t Main::init()
{
  if(!processManager.start(*this))
    return error = Error::getErrorString(), false;
  return true;
}

bool_t Main::connect()
{
  connection.close();

  if(!connection.connect(*this))
    return error = connection.getErrorString(), false;

  HashMap<String, bool_t> autostartProcesses;
  autostartProcesses.append("Services/Market.exe", false);
  autostartProcesses.append("Services/User.exe", false);

  Buffer buffer(ZLIMDB_MAX_MESSAGE_SIZE);
  if(!connection.createTable("processes", processesTableId))
    return error = connection.getErrorString(), false;
  if(!connection.subscribe(processesTableId))
    return error = connection.getErrorString(), false;
  HashMap<uint64_t, Process> processes;
  String command;
  while(connection.getResponse(buffer))
  {
    void* data = (byte_t*)buffer;
    uint32_t size = buffer.size();
    for(const meguco_process_entity* process; process = (const meguco_process_entity*)zlimdb_get_entity(sizeof(meguco_process_entity), &data, &size);)
    {
      if(!ZlimdbConnection::getString(process->entity, sizeof(*process), process->cmd_size, command))
        continue;
      Process& processInfo = processes.append(process->entity.id, Process());
      processInfo.entityId = process->entity.id;
      processInfo.command = command;
      HashMap<String, bool_t>::Iterator it = autostartProcesses.find(command);
      if(it != autostartProcesses.end())
        *it = true;
      addedProcess(process->entity.id, command);
    }
  }
  if(connection.getErrno() != 0)
    return error = connection.getErrorString(), false;

  // add known processes not in processes table to processes table
  List<HashMap<uint64_t, Process>::Iterator> processesToAdd;
  for(HashMap<uint64_t, Process>::Iterator i = this->processes.begin(), end = this->processes.end(); i != end; ++i)
  {
    Process& process = *i;
    if(processes.find(process.entityId) == processes.end())
      processesToAdd.append(i);
  }
  for(List<HashMap<uint64_t, Process>::Iterator>::Iterator i = processesToAdd.end(), end = processesToAdd.end(); i != end; ++i)
  {
    Process& process = *(*i);
    buffer.resize(sizeof(meguco_process_entity) + process.command.length());
    meguco_process_entity* processEntity = (meguco_process_entity*)(byte_t*)buffer;
    ZlimdbConnection::setEntityHeader(processEntity->entity, 0, 0, buffer.size());
    ZlimdbConnection::setString(processEntity->entity, processEntity->cmd_size, sizeof(meguco_process_entity), process.command);
    uint64_t entityId;
    if(!connection.add(processesTableId, processEntity->entity, entityId))
      return error = connection.getErrorString(), false;
    Process newProcess;
    newProcess.command = process.command;
    newProcess.entityId = entityId;
    this->processes.remove(*i);
    this->processes.append(entityId, newProcess);
  }

  // start autostart process if they are not already running
  for(HashMap<String, bool_t>::Iterator i = autostartProcesses.begin(), end = autostartProcesses.end(); i != end; ++i)
  {
    bool_t alreadyRunning = *i;
    if(!alreadyRunning)
    {
      String cmd = i.key();
      buffer.resize(sizeof(meguco_process_entity) + cmd.length());
      meguco_process_entity* processEntity = (meguco_process_entity*)(byte_t*)buffer;
      ZlimdbConnection::setEntityHeader(processEntity->entity, 0, 0, buffer.size());
      ZlimdbConnection::setString(processEntity->entity, processEntity->cmd_size, sizeof(meguco_process_entity), cmd);
      uint64_t entityId;
      if(!connection.add(processesTableId, processEntity->entity, entityId))
        return error = connection.getErrorString(), false;
    }
  }

  return true;
}

bool_t Main::process()
{
  for(;;)
  {
    if(!connection.process())
      return error = connection.getErrorString(), false;
    List<uint64_t> terminatedProcesses;
    mutex.lock();
    terminatedProcesses.swap(this->terminatedProcesses);
    mutex.unlock();
    for(List<uint64_t>::Iterator i = terminatedProcesses.begin(), end = terminatedProcesses.end(); i != end; ++i)
      removeProcess(*i);
  }
}

void_t Main::removeProcess(uint64_t entityId)
{
  HashMap<uint64_t, Process>::Iterator it = processes.find(entityId);
  if(it == processes.end())
    return;
  const Process& process = *it;
  connection.remove(processesTableId, process.entityId);
  processes.remove(it);
}

bool_t Main::killProcess(uint64_t entityId)
{
  processManager.killProcess(entityId);
  return true;
}

void_t Main::terminatedProcess(uint64_t entityId)
{
  mutex.lock();
  terminatedProcesses.append(entityId);
  mutex.unlock();
  connection.interrupt();
}

void_t Main::addedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == processesTableId)
  {
    if(entity.size < sizeof(meguco_process_entity))
      return;
    const meguco_process_entity& processEntity = (const meguco_process_entity&)entity;
    String cmd;
    if(!ZlimdbConnection::getString(processEntity.entity, sizeof(meguco_process_entity), processEntity.cmd_size, cmd))
      return;
    String command = cmd;
    command += "";
    addedProcess(processEntity.entity.id, cmd);
  }
}

void_t Main::addedProcess(uint64_t entityId, const String& cmd)
{
  if(processes.find(entityId) != processes.end())
    return;
  processManager.startProcess(entityId, binaryDir + "/" + cmd);
  Process& process = processes.append(entityId, Process());
  process.command = cmd;
  process.entityId = entityId;
}

void_t Main::removedEntity(uint32_t tableId, uint64_t entityId)
{
  if(tableId == processesTableId)
    removedProcess(entityId);
}

void_t Main::removedProcess(uint64_t entityId)
{
  HashMap<uint64_t, Process>::Iterator it = processes.find(entityId);
  if(it == processes.end())
    return;
  const Process& process = *it;
  if(!killProcess(process.entityId))
    return;
  processes.remove(it);
}
