
#include <nstd/Log.h>
#include <nstd/Console.h>
#include <nstd/File.h>
#include <nstd/Process.h>
#include <nstd/Error.h>
#include <nstd/HashSet.h>

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

  HashSet<String> autostartProcesses;
#ifdef _WIN32
  autostartProcesses.append("Services/Market.exe");
  autostartProcesses.append("Services/User.exe");
#else
  autostartProcesses.append("Services/Market");
  autostartProcesses.append("Services/User");
#endif

  byte_t buffer[ZLIMDB_MAX_MESSAGE_SIZE];
  if(!connection.createTable("processes", processesTableId))
    return error = connection.getErrorString(), false;
  if(!connection.subscribe(processesTableId, zlimdb_subscribe_flag_responder))
    return error = connection.getErrorString(), false;
  HashMap<uint64_t, Process> processesInTable;
  String command;
  while(connection.getResponse(buffer))
  {
    void* data = (byte_t*)buffer;
    for(const meguco_process_entity* process = (const meguco_process_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_process_entity));
        process;
        process = (const meguco_process_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_process_entity), &process->entity))
    {
      if(!ZlimdbConnection::getString(process->entity, sizeof(*process), process->cmd_size, command))
        continue;
      Process& processInfo = processesInTable.append(process->entity.id, Process());
      processInfo.entityId = process->entity.id;
      processInfo.command = command;
    }
  }
  if(connection.getErrno() != 0)
    return error = connection.getErrorString(), false;

  // update processes in processes table
  for(HashMap<uint64_t, Process>::Iterator i = localProcesses.begin(), end = localProcesses.end(), next; i != end; i = next)
  {
    next = i; ++next;
    uint64_t processEntityId = i.key();
    const Process& process = *i;
    HashMap<uint64_t, Process>::Iterator it = processesInTable.find(processEntityId);
    if(it == processesInTable.end())
    { // add to table
      meguco_process_entity* processEntity = (meguco_process_entity*)buffer;
      ZlimdbConnection::setEntityHeader(processEntity->entity, 0, 0, sizeof(meguco_broker_type_entity));
      if(!ZlimdbConnection::copyString(process.command, processEntity->entity, processEntity->cmd_size, ZLIMDB_MAX_ENTITY_SIZE))
        continue;
      uint64_t id;
      if(!connection.add(processesTableId, processEntity->entity, id))
        return error = connection.getErrorString(), false;
      if(id != processEntityId) // update local process id
      {
        processManager.setProcessId(processEntityId, id);
        localProcesses.append(id, process);
        localProcesses.remove(i);
      }
    }
    else
    {
      if(it->command != i->command) // update command in table
      {
        meguco_process_entity* processEntity = (meguco_process_entity*)buffer;
        ZlimdbConnection::setEntityHeader(processEntity->entity, 0, 0, sizeof(meguco_broker_type_entity));
        if(!ZlimdbConnection::copyString(process.command, processEntity->entity, processEntity->cmd_size, ZLIMDB_MAX_ENTITY_SIZE))
          continue;
        if(!connection.update(processesTableId, processEntity->entity))
          return error = connection.getErrorString(), false;
      }
      processesInTable.remove(it);
    }
    autostartProcesses.remove(process.command);
  }

  // remove unknown process from processes table
  for(HashMap<uint64_t, Process>::Iterator i = processesInTable.begin(), end = processesInTable.end(); i != end; ++i)
    if(!connection.remove(processesTableId, i.key()))
      return error = connection.getErrorString(), false;

  // start not running autostart processes
  for(HashSet<String>::Iterator i = autostartProcesses.begin(), end = autostartProcesses.end(); i != end; ++i)
  {
    const String& cmd = *i;
    meguco_process_entity* processEntity = (meguco_process_entity*)buffer;
    ZlimdbConnection::setEntityHeader(processEntity->entity, 0, 0, sizeof(meguco_process_entity));
    if(!ZlimdbConnection::copyString(cmd, processEntity->entity, processEntity->cmd_size, ZLIMDB_MAX_ENTITY_SIZE))
      continue;
    uint64_t id;
    if(!connection.add(processesTableId, processEntity->entity, id))
      return error = connection.getErrorString(), false;
    Process& process = localProcesses.append(id, Process());
    process.entityId = id;
    process.command = cmd;
    processManager.startProcess(id, binaryDir + "/" + cmd);
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
  HashMap<uint64_t, Process>::Iterator it = localProcesses.find(entityId);
  if(it == localProcesses.end())
    return;
  const Process& process = *it;
  connection.remove(processesTableId, process.entityId);
  localProcesses.remove(it);
}

void_t Main::terminatedProcess(uint64_t entityId)
{
  mutex.lock();
  terminatedProcesses.append(entityId);
  mutex.unlock();
  connection.interrupt();
}

void_t Main::controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size)
{
  if(tableId == processesTableId)
    return controlProcess(requestId, entityId, controlCode, data, size);
  return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
}

void_t Main::controlProcess(uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size)
{
  switch(controlCode)
  {
  case meguco_process_control_start:
    {
      if(size < sizeof(meguco_process_entity) || ((meguco_process_entity*)data)->entity.size < size)
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_message_data);
      meguco_process_entity* args = (meguco_process_entity*)data;
      String cmd;
      if(!ZlimdbConnection::getString(args->entity, sizeof(meguco_process_entity), args->cmd_size, cmd))
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_message_data);
      char_t buffer[ZLIMDB_MAX_ENTITY_SIZE];
      meguco_process_entity* processEntity = (meguco_process_entity*)buffer;
      ZlimdbConnection::setEntityHeader(processEntity->entity, 0, 0, sizeof(meguco_process_entity));
      if(!ZlimdbConnection::copyString(cmd, processEntity->entity, processEntity->cmd_size, ZLIMDB_MAX_ENTITY_SIZE))
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_message_data);
      uint64_t id;
      if(!connection.add(processesTableId, processEntity->entity, id))
        return (void_t)connection.sendControlResponse(requestId, connection.getErrno());
      Process& process = localProcesses.append(id, Process());
      process.entityId = id;
      process.command = cmd;
      processManager.startProcess(id, binaryDir + "/" + cmd);
      return (void_t)connection.sendControlResponse(requestId, (const byte_t*)&id, sizeof(id));
    }
    break;
  case meguco_process_control_stop:
    processManager.killProcess(entityId);
    return (void_t)connection.sendControlResponse(requestId, 0, 0);
  default:
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
  }
}
