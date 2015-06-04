
#include <nstd/Console.h>
#include <nstd/File.h>
#include <nstd/Directory.h>
#include <nstd/Thread.h>

#include "Main.h"

int_t main(int_t argc, char_t* argv[])
{
  String binaryDir = File::dirname(String(argv[0], String::length(argv[0])));

  // initialize connection handler
  Main main;

  // find market engines
  {
    Directory dir;
    if(dir.open(binaryDir + "/Markets", String(), false))
    {
      String path;
      bool_t isDir;
      while(dir.read(path, isDir))
        if(File::isExecutable(path))
          main.addMarket(path);
    }
  }

  // main loop
  for(;; Thread::sleep(10 * 1000))
  {
    // connect to zlimdb server
    if(!main.connect())
    {
        Console::errorf("error: Could not connect to zlimdb server: %s\n", (const char_t*)main.getErrorString());
        continue;
    }
    Console::printf("Connected to zlimdb server.\n");

    // run connection handler loop
    main.process();

    Console::errorf("error: Lost connection to zlimdb server: %s\n", (const char_t*)main.getErrorString());
  }
  return 0;
}

Main::~Main()
{
  disconnect();
}

void_t Main::disconnect()
{
  connection.close();
  processes.clear();
}

void_t Main::addMarket(const String& executable)
{
  Market& market = markets.append(executable, Market());
  market.running = false;
}

bool_t Main::connect()
{
  disconnect();

  if(!connection.connect(*this))
    return error = connection.getErrorString(), false;

  Buffer buffer(ZLIMDB_MAX_MESSAGE_SIZE);

  // get processes
  if(!connection.createTable("processes", processesTableId))
      return error = connection.getErrorString(), false;
  if(connection.subscribe(processesTableId))
    return error = connection.getErrorString(), false;
  {
    String tableName;
    HashMap<uint64_t, String> processes;
    while(connection.getResponse(buffer))
    {
      void* data = (byte_t*)buffer;
      uint32_t size = buffer.size();
      String command;
      for(const meguco_process_entity* process; process = (const meguco_process_entity*)zlimdb_get_entity(sizeof(meguco_process_entity), &data, &size);)
      {
        if(!ZlimdbConnection::getString(process->entity, sizeof(*process), process->cmd_size, command))
          continue;
        processes.append(process->entity.id, command);
      }
    }
    if(connection.getErrno() != 0)
      return error = connection.getErrorString(), false;
    for(HashMap<uint64_t, String>::Iterator i = processes.begin(), end = processes.end(); i != end; ++i)
      addedProcess(i.key(), *i);
  }

  // start markets
  for(HashMap<String, Market>::Iterator i = markets.begin(), end = markets.end(); i != end; ++i)
  {
  }

  return true;
}

bool_t Main::process()
{
  for(;;)
    if(!connection.process())
      return false;
}

void_t Main::addedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == processesTableId)
  {
    if(entity.size < sizeof(meguco_process_entity))
      return;
    const meguco_process_entity* process = (const meguco_process_entity*)&entity;
    String command;
    if(!ZlimdbConnection::getString(process->entity, sizeof(*process), process->cmd_size, command))
      return;
    addedProcess(process->entity.id, command);
  }
}

void_t Main::removedEntity(uint32_t tableId, uint64_t entityId)
{
  if(tableId == processesTableId)
    removedProcess(entityId);
}

void_t Main::addedProcess(uint64_t entityId, const String& command)
{
  if(command.startsWith("Markets/"))
  {
    HashMap<String, Market>::Iterator it = markets.find(command);
    if(it == markets.end())
      return;
    Market& market = *it;
    if(market.running)
    {
      // todo: kill this process?
      return;
    }
    processes.append(entityId, &market);
    market.running = true;
  }
}

void_t Main::removedProcess(uint64_t entityId)
{
  HashMap<uint64_t, Market*>::Iterator it = processes.find(entityId);
  if(it != processes.end())
  {
    Market* market = *it;
    market->running = false;
    processes.remove(it);
    // todo: restart process?
  }
}
