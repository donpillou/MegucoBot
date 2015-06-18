
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
  Main server(binaryDir);
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
        continue;
    }
    Console::printf("Connected to zlimdb server.\n");

    // run connection handler loop
    server.process();

    Console::errorf("error: Lost connection to zlimdb server: %s\n", (const char_t*)server.getErrorString());
  }
  return 0;
}

bool_t Main::init()
{
  if(!processManager.start(*this))
    return error = Error::getErrorString(), false;

  // start market service
  {
    uint32_t processId;
    String command = "Services/Market.exe";
    if(!processManager.startProcess(binaryDir + "/" + command, processId))
      return error = processManager.getErrorString(), false;
    Process& process = processesById.append(processId, Process());
    process.processId = processId;
    process.command = command;
    process.entityId = 0;
  }

  // start user serivce
  {
    uint32_t processId;
    String command = "Services/User.exe";
    if(!processManager.startProcess(binaryDir + "/" + command, processId))
      return error = processManager.getErrorString(), false;
    Process& process = processesById.append(processId, Process());
    process.processId = processId;
    process.command = command;
    process.entityId = 0;
  }

  return true;
}

bool_t Main::connect()
{
  connection.close();
  this->processes.clear();

  if(!connection.connect(*this))
    return error = connection.getErrorString(), false;

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
      processInfo.processId = process->process_id;
    }
  }
  if(connection.getErrno() != 0)
    return error = connection.getErrorString(), false;

  // start unknown processes from processes table
  for(HashMap<uint64_t, Process>::Iterator i = processes.begin(), end = processes.end(), next; i != end; i = next)
  {
    next = i;
    ++next;
    Process& process = *i;
    HashMap<uint32_t, Process>::Iterator it = this->processesById.find(process.processId);
    if(it == this->processesById.end())
    {
      if(!startProcess(process.entityId, process.command))
        return false;
    }
    else
    {
      Process& knownProcess = *it;
      if(process.entityId != knownProcess.entityId)
      {
        knownProcess.entityId = 0;
        if(!connection.remove(processesTableId, process.entityId))
          return error = connection.getErrorString(), false;
        processes.remove(i);
      }
    }
  }

  // add known processes not in processes table to processes table
  for(HashMap<uint32_t, Process>::Iterator i = this->processesById.begin(), end = this->processesById.end(); i != end; ++i)
  {
    Process& process = *i;
    if(processes.find(process.entityId) == processes.end())
    {
      buffer.resize(sizeof(meguco_process_entity) + process.command.length());
      meguco_process_entity* processEntity = (meguco_process_entity*)(byte_t*)buffer;
      ZlimdbConnection::setEntityHeader(processEntity->entity, 0, 0, buffer.size());
      processEntity->process_id = process.processId;
      ZlimdbConnection::setString(processEntity->entity, processEntity->cmd_size, sizeof(meguco_process_entity), process.command);
      uint64_t entityId;
      if(!connection.add(processesTableId, processEntity->entity, entityId))
        return error = connection.getErrorString(), false;
      process.entityId = entityId;
      this->processes.append(entityId, &process);
    }
    else
      this->processes.append(process.entityId, &process);
  }
  return true;
}

bool_t Main::process()
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

void_t Main::removeProcess(uint32_t processId)
{
  HashMap<uint32_t, Process>::Iterator it = processesById.find(processId);
  if(it == processesById.end())
    return;
  const Process& process = *it;
  connection.remove(processesTableId, process.entityId);
  processes.remove(process.entityId);
  processesById.remove(it);
}

bool_t Main::startProcess(uint64_t& entityId, const String& command)
{
  uint32_t processId;
  if(!processManager.startProcess(binaryDir + "/" + command, processId))
    return error = processManager.getErrorString(), false;
  Process& process = processesById.append(processId, Process());
  process.processId = processId;
  process.command = command;
  process.entityId = entityId;
  if(!entityId)
  {
    Buffer buffer;
    buffer.resize(sizeof(meguco_process_entity) + command.length());
    meguco_process_entity* processEntity = (meguco_process_entity*)(byte_t*)buffer;
    ZlimdbConnection::setEntityHeader(processEntity->entity, entityId, 0, buffer.size());
    processEntity->process_id = processId;
    ZlimdbConnection::setString(processEntity->entity, processEntity->cmd_size, sizeof(meguco_process_entity), command);
    if(!connection.add(processesTableId, processEntity->entity, entityId))
      return error = connection.getErrorString(), false;
  }
  process.entityId = entityId;
  processes.append(process.entityId, &process);
  return true;
}

bool_t Main::killProcess(uint32_t processId)
{
  if(!processManager.killProcess(processId))
    return error = processManager.getErrorString(), false;
  return true;
}

void_t Main::terminatedProcess(uint32_t processId)
{
  mutex.lock();
  terminatedProcesses.append(processId);
  mutex.unlock();
  connection.interrupt();
}

void_t Main::addedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == processesTableId && entity.size >= sizeof(meguco_process_entity))
  {
    meguco_process_entity& process = (meguco_process_entity&)entity;
    if(processesById.find(process.process_id) != processesById.end())
      return;
    String command;
    if(!ZlimdbConnection::getString(process.entity, sizeof(meguco_process_entity), process.cmd_size, command))
      return;
    if(!startProcess(process.entity.id, command))
      connection.remove(processesTableId, process.entity.id);
  }
}

void_t Main::removedEntity(uint32_t tableId, uint64_t entityId)
{
  if(tableId == processesTableId)
  {
    HashMap<uint64_t, Process*>::Iterator it = processes.find(entityId);
    if(it == processes.end())
      return;
    const Process* process = *it;
    if(!killProcess(process->processId))
      return;
    processes.remove(it);
    processesById.remove(process->processId);
  }
}
