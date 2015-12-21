
#include <nstd/Log.h>
#include <nstd/File.h>
#include <nstd/Thread.h>
#include <nstd/Directory.h>

#include "Main.h"
#include "User.h"
#include "Broker.h"
#include "Session.h"

int_t main(int_t argc, char_t* argv[])
{
  String binaryDir = File::dirname(File::dirname(String(argv[0], String::length(argv[0]))));

  Log::setFormat("%P> %m");

  // initialize connection handler
  Main main;

  // find broker types
  {
    Directory dir;
    if(dir.open(binaryDir + "/Brokers", String(), false))
    {
      String path;
      bool_t isDir;
      while(dir.read(path, isDir))
        if(File::isExecutable(binaryDir + "/Brokers/" + path))
        {
          const char_t* prevUpper = 0, * lastUpper = 0, * end;
          for(const char_t* p = path; *p; ++p)
            if(String::isUpper(*p))
            {
              prevUpper = lastUpper;
              lastUpper = p;
            }
            else if(*p == '.')
            {
              end = p;
              break;
            }
          if(prevUpper && lastUpper && end)
          {
            String name = path.substr(0, prevUpper - (const char_t*)path);
            String comm = path.substr(name.length(), lastUpper - prevUpper).toUpperCase();
            String base = path.substr(name.length() + comm.length(), end - lastUpper).toUpperCase();
            main.addBrokerType(name + "/" + comm + "/" + base, String("Brokers/") + path);
          }
        }
    }
  }

  // find bot types
  {
    Directory dir;
    if(dir.open(binaryDir + "/Bots", String(), false))
    {
      String path;
      bool_t isDir;
      while(dir.read(path, isDir))
        if(File::isExecutable(binaryDir + "/Bots/" + path))
          main.addBotType(File::basename(path, "exe"), String("Bots/") + path);
    }
  }

  // main loop
  for(;; Thread::sleep(10 * 1000))
  {
    // connect to zlimdb server
    if(!main.connect())
    {
        Log::errorf("Could not connect to zlimdb server: %s", (const char_t*)main.getErrorString());
        continue;
    }
    Log::infof("Connected to zlimdb server.");

    // run connection handler loop
    main.process();

    Log::errorf("Lost connection to zlimdb server: %s", (const char_t*)main.getErrorString());
  }
  return 0;
}

Main::~Main()
{
  disconnect();
}

void_t Main::addBrokerType(const String& name, const String& executable)
{
  BrokerType brokerType = {name, executable};
  brokerTypesByName.append(name, brokerType);
}

void_t Main::addBotType(const String& name, const String& executable)
{
  BotType botType = {name, executable};
  botTypesByName.append(name, botType);
}

void_t Main::disconnect()
{
  connection.close();
  for(HashMap<String, User *>::Iterator i = users.begin(), end = users.end(); i != end; ++i)
    delete *i;
  users.clear();
  processes.clear();
  processesByCommand.clear();
  tableInfo.clear();
  users.clear();
}

bool_t Main::connect()
{
  disconnect();

  if(!connection.connect(*this))
    return error = connection.getErrorString(), false;

  byte_t buffer[ZLIMDB_MAX_MESSAGE_SIZE];

  // update broker types table
  {
    HashMap<String, uint64_t> knownBotMarkets;
    uint32_t botMarketsTableId;
    if(!connection.createTable("brokers", botMarketsTableId))
      return error = connection.getErrorString(), false;
    if(!connection.query(botMarketsTableId))
      return error = connection.getErrorString(), false;
    String marketName;
    while(connection.getResponse(buffer))
    {
      for(const meguco_broker_type_entity* brokerType = (const meguco_broker_type_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_broker_type_entity));
          brokerType;
          brokerType = (const meguco_broker_type_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_broker_type_entity), &brokerType->entity))
      {
        if(!ZlimdbConnection::getString(brokerType->entity, sizeof(*brokerType), brokerType->name_size, marketName))
          continue;
        knownBotMarkets.append(marketName, brokerType->entity.id);
      }
    }
    if(connection.getErrno() != 0)
      return error = connection.getErrorString(), false;
    for(HashMap<String, BrokerType>::Iterator i = brokerTypesByName.begin(), end = brokerTypesByName.end(); i != end; ++i)
    {
      const String& marketName = i.key();
      HashMap<String, uint64_t>::Iterator it = knownBotMarkets.find(marketName);
      if(it == knownBotMarkets.end())
      {
        meguco_broker_type_entity* brokerType = (meguco_broker_type_entity*)buffer;
        ZlimdbConnection::setEntityHeader(brokerType->entity, 0, 0, sizeof(meguco_broker_type_entity));
        if(!ZlimdbConnection::copyString(marketName, brokerType->entity, brokerType->name_size, ZLIMDB_MAX_ENTITY_SIZE))
          continue;
        uint64_t id;
        if(!connection.add(botMarketsTableId, brokerType->entity, id))
          return error = connection.getErrorString(), false;
        brokerTypes.append(id, &*i);
      }
      else
      {
        knownBotMarkets.remove(it);
        brokerTypes.append(*it, &*i);
      }
    }
    for(HashMap<String, uint64_t>::Iterator i = knownBotMarkets.begin(), end = knownBotMarkets.end(); i != end; ++i)
      connection.remove(botMarketsTableId, *i);
  }

  // update bot types table
  {
    HashMap<String, uint64_t> knownBotEngines;
    uint32_t botEnginesTableId;
    if(!connection.createTable("bots", botEnginesTableId))
      return error = connection.getErrorString(), false;
    if(!connection.query(botEnginesTableId))
      return error = connection.getErrorString(), false;
    String engineName;
    while(connection.getResponse(buffer))
    {
      for(const meguco_bot_type_entity* botType = (const meguco_bot_type_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_bot_type_entity));
          botType;
          botType = (const meguco_bot_type_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_bot_type_entity), &botType->entity))
      {
        if(!ZlimdbConnection::getString(botType->entity, sizeof(*botType), botType->name_size, engineName))
          continue;
        knownBotEngines.append(engineName, botType->entity.id);
      }
    }
    if(connection.getErrno() != 0)
      return error = connection.getErrorString(), false;
    for(HashMap<String, BotType>::Iterator i = botTypesByName.begin(), end = botTypesByName.end(); i != end; ++i)
    {
      const String& botEngineName = i.key();
      HashMap<String, uint64_t>::Iterator it = knownBotEngines.find(botEngineName);
      if(it == knownBotEngines.end())
      {
        meguco_bot_type_entity* botType = (meguco_bot_type_entity*)buffer;
        ZlimdbConnection::setEntityHeader(botType->entity, 0, 0, sizeof(meguco_bot_type_entity));
        if(!ZlimdbConnection::copyString(botEngineName, botType->entity, botType->name_size, ZLIMDB_MAX_ENTITY_SIZE))
          continue;
        uint64_t id;
        if(!connection.add(botEnginesTableId, botType->entity, id))
          return error = connection.getErrorString(), false;
        botTypes.append(id, &*i);
      }
      else
      {
        knownBotEngines.remove(it);
        botTypes.append(*it, &*i);
      }
    }
    for(HashMap<String, uint64_t>::Iterator i = knownBotEngines.begin(), end = knownBotEngines.end(); i != end; ++i)
      connection.remove(botEnginesTableId, *i);
  }

  // get processes
  if(!connection.createTable("processes", processesTableId))
      return error = connection.getErrorString(), false;
  if(!connection.subscribe(processesTableId, 0))
    return error = connection.getErrorString(), false;
  {
    while(connection.getResponse(buffer))
    {
      String command;
      for(const meguco_process_entity* process = (const meguco_process_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_process_entity));
          process;
          process = (const meguco_process_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_process_entity), &process->entity))
      {
        if(!ZlimdbConnection::getString(process->entity, sizeof(*process), process->cmd_size, command))
          continue;
        addedProcess(process->entity.id, command);
      }
    }
    if(connection.getErrno() != 0)
      return error = connection.getErrorString(), false;
  }

  // get table list
  if(!connection.subscribe(zlimdb_table_tables, 0))
    return error = connection.getErrorString(), false;
  {
    String tableName;
    while(connection.getResponse(buffer))
    {
      for(const zlimdb_table_entity* entity = (const zlimdb_table_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(zlimdb_table_entity));
          entity;
          entity = (const zlimdb_table_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(zlimdb_table_entity), &entity->entity))
      {
        if(!ZlimdbConnection::getString(entity->entity, sizeof(*entity), entity->name_size, tableName))
          continue;
        addedTable((uint32_t)entity->entity.id, tableName);
      }
    }
    if(connection.getErrno() != 0)
      return error = connection.getErrorString(), false;
  }
  return true;
}

bool_t Main::process()
{
  for(;;)
    if(!connection.process())
      return error = connection.getErrorString(), false;
}

User * Main::createUser(const String& name)
{
  User * user = new User(name);
  users.append(name, user);
  return user;
}

void_t Main::addedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == zlimdb_table_tables)
  {
    if(entity.size < sizeof(zlimdb_table_entity))
      return;
    const zlimdb_table_entity* tableEntity = (const zlimdb_table_entity*)&entity;
    String tableName;
    if(!ZlimdbConnection::getString(tableEntity->entity, sizeof(*tableEntity), tableEntity->name_size, tableName))
      return;
    addedTable((uint32_t)tableEntity->entity.id, tableName);
  }
  else if(tableId == processesTableId)
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

void_t Main::controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size)
{
  HashMap<uint32_t, TableInfo>::Iterator it = tableInfo.find(tableId);
  if(it == tableInfo.end())
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
  const TableInfo& tableInfo = *it;
  switch(tableInfo.type)
  {
  case user:
    return controlUser(*(User *)tableInfo.object, requestId, controlCode, data, size);
  case userSession:
    return controlUserSession(*(Session*)tableInfo.object, requestId, entityId, controlCode, data, size);
  default:
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
  }
}

void_t Main::addedTable(uint32_t tableId, const String& tableName)
{
  if(tableName.startsWith("users/"))
  {
    const char_t* start = (const char_t*)tableName + 6;
    const char_t* end = String::find(start, '/');
    if(!end)
      return;
    String userName = tableName.substr(6, end - start);
    User * user = findUser(userName);
    if(!user)
      user = createUser(userName);
    const char_t* typeNameStart = end + 1;

    if(String::startsWith(typeNameStart, "brokers/"))
    {
      uint64_t brokerId = String::toUInt64(typeNameStart + 8);
      Broker* broker = user->findBroker(brokerId);
      if(!broker)
        broker = user->createBroker(brokerId);

      if(tableName.endsWith("/balance"))
        broker->setBalanceTableId(tableId);
      else if(tableName.endsWith("/orders"))
        broker->setOrdersTableId(tableId);
      else if(tableName.endsWith("/transactions"))
        broker->setTransactionsTableId(tableId);
      else if(tableName.endsWith("/log"))
        broker->setLogTableId(tableId);
      else if(tableName.endsWith("/broker"))
      {
        broker->setBrokerTableId(tableId);

        // subscribe to broker
        if(!connection.subscribe(tableId, 0))
          return;
        TableInfo tableInfoData = {Main::userBroker};
        TableInfo& tableInfo = this->tableInfo.append(tableId, tableInfoData);
        tableInfo.object = broker;
        byte_t buffer[ZLIMDB_MAX_MESSAGE_SIZE];
        while(connection.getResponse(buffer))
        {
          for(const meguco_user_broker_entity* entity = (const meguco_user_broker_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_entity));
              entity;
              entity = (const meguco_user_broker_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_entity), &entity->entity))
          {
            if(entity->entity.id != 1)
              continue;
            BrokerType* brokerType = *brokerTypes.find(entity->broker_type_id);
            if(!brokerType)
              continue;

            broker->setEntity(*entity);

            // update user broker state
            String command = brokerType->executable + " " + user->getName() + " " + String::fromUInt64(brokerId);
            broker->setCommand(command);
            HashMap<String, Process*>::Iterator it = processesByCommand.find(command);
            meguco_user_broker_state state = meguco_user_broker_stopped;
            if(it != processesByCommand.end() && (*it)->type == Main::userBroker)
              state = meguco_user_broker_running;
            if (state != broker->getState())
            {
              broker->setState(state);
              connection.update(tableId, broker->getEntity());
            }

            // start broker process
            if(state != meguco_user_broker_running)
            {
              // start process
              if(!connection.startProcess(processesTableId, broker->getCommand()))
                return;

              // set state to starting
              broker->setState(meguco_user_broker_starting);
              if(!connection.update(tableId, broker->getEntity()))
                return;
            }
          }
        }
        if(connection.getErrno() != 0)
          return;
      }
    }

    else if(String::startsWith(typeNameStart, "sessions/"))
    {
      uint64_t sessionId = String::toUInt64(typeNameStart + 8);
      Session* session = user->findSession(sessionId);
      if(!session)
        session = user->createSession(sessionId);

      if(tableName.endsWith("/orders"))
        session->setOrdersTableId(tableId);
      else if(tableName.endsWith("/transactions"))
        session->setTransactionsTableId(tableId);
      else if(tableName.endsWith("/assets"))
        session->setAssetsTableId(tableId);
      else if(tableName.endsWith("/log"))
        session->setLogTableId(tableId);
      else if(tableName.endsWith("/properties"))
        session->setPropertiesTableId(tableId);
      else if(tableName.endsWith("/broker"))
      {
        session->setSessionTableId(tableId);

        // subscribe to session
        if(!connection.subscribe(tableId, zlimdb_subscribe_flag_responder))
          return;
        TableInfo tableInfoData = {Main::userSession};
        TableInfo& tableInfo = this->tableInfo.append(tableId, tableInfoData);
        tableInfo.object = session;
        byte_t buffer[ZLIMDB_MAX_MESSAGE_SIZE];
        while(connection.getResponse(buffer))
        {
          for(const meguco_user_session_entity* entity = (const meguco_user_session_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_user_session_entity));
              entity;
              entity = (const meguco_user_session_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_user_session_entity), &entity->entity))
          {
            if(entity->entity.id != 1)
              continue;
            BotType* botType = *botTypes.find(entity->bot_type_id);
            if(!botType)
              continue;

            session->setEntity(*entity);

            // update user session state
            String command = botType->executable + " " + user->getName() + " " + String::fromUInt64(sessionId);
            session->setCommand(command);
            HashMap<String, Process*>::Iterator it = processesByCommand.find(command);
            meguco_user_session_state state = meguco_user_session_stopped;
            if(it != processesByCommand.end() && (*it)->type == Main::userSession)
              state = meguco_user_session_running;
            if (state != session->getState())
            {
              session->setState(state);
              connection.update(tableId, session->getEntity());
            }
          }
        }
        if(connection.getErrno() != 0)
          return;
      }
    }

    else if(tableName.endsWith("/user"))
    {
      if(!connection.listen(tableId))
        return;
      TableInfo tableInfoData = {Main::user};
      TableInfo& tableInfo = this->tableInfo.append(tableId, tableInfoData);
      tableInfo.object = user;
    }
  }
}

void_t Main::addedProcess(uint64_t entityId, const String& command)
{
  if(command.startsWith("Brokers/"))
  {
    size_t pos = 0;
    getArg(command, pos, ' ');
    String userName = getArg(command, pos, ' ');
    uint64_t brokerId = getArg(command, pos, ' ').toUInt64();

    Process processData = {userBroker, command, entityId};
    Process& process = processes.append(entityId, processData);
    processesByCommand.append(command, &process);
    User * user = findUser(userName);
    if(user)
    {
      Broker* broker = user->findBroker(brokerId);
      if(broker && broker->hasEntity())
      {
        process.object = broker;
        broker->setState(meguco_user_broker_running);
        if(!connection.update(broker->getBrokerTableId(), broker->getEntity()))
          return;
      }
    }
  }
  if(command.startsWith("Bots/"))
  {
    size_t pos = 0;
    getArg(command, pos, ' ');
    String userName = getArg(command, pos, ' ');
    uint64_t sessionId = getArg(command, pos, ' ').toUInt64();

    Process processData = {userSession, command, entityId};
    Process& process = processes.append(entityId, processData);
    processesByCommand.append(command, &process);
    User * user = findUser(userName);
    if(user)
    {
      Session* session = user->findSession(sessionId);
      if(session && session->hasEntity())
      {
        process.object = session;
        session->setState(meguco_user_session_running);
        if(!connection.update(session->getSessionTableId(), session->getEntity()))
          return;
      }
    }
  }
}

void_t Main::removedProcess(uint64_t entityId)
{
  HashMap<uint64_t, Process>::Iterator it = processes.find(entityId);
  if(it != processes.end())
  {
    Process& process = *it;
    switch(process.type)
    {
    case userBroker:
      {
        Broker* broker = (Broker*)process.object;
        if(broker && broker->getState() != meguco_user_broker_stopped)
        {
          broker->setState(meguco_user_broker_stopped);
          connection.update(broker->getBrokerTableId(), broker->getEntity());
        }
      }
      break;
    case userSession:
      {
        Session* session = (Session*)process.object;
        if(session && session->getState() != meguco_user_session_stopped)
        {
          session->setState(meguco_user_session_stopped);
          connection.update(session->getSessionTableId(), session->getEntity());
        }
      }
      break;
    default:
      break;
    }
    processesByCommand.remove(process.command);
    processes.remove(it);
  }
}

void_t Main::controlUser(User & user, uint32_t requestId, uint32_t controlCode, const byte_t* data, size_t size)
{
  switch(controlCode)
  {
  case meguco_user_control_create_broker:
    if(size < sizeof(meguco_user_broker_entity))
      return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
    {
      // get arguments
      const meguco_user_broker_entity* createArgs = (const meguco_user_broker_entity*)data;
      size_t offset = sizeof(meguco_user_broker_entity);
      String name, key, secret;
      if(!ZlimdbConnection::getString(createArgs->entity, createArgs->user_name_size, name, offset) ||
        !ZlimdbConnection::getString(createArgs->entity, createArgs->key_size, key, offset) ||
        !ZlimdbConnection::getString(createArgs->entity, createArgs->secret_size, secret, offset))
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);

      // prepare broker entity
      byte_t buffer[ZLIMDB_MAX_ENTITY_SIZE];
      meguco_user_broker_entity* brokerEntity = (meguco_user_broker_entity*)buffer;
      ZlimdbConnection::setEntityHeader(brokerEntity->entity, 0, 0, sizeof(meguco_user_broker_entity));
      if(!ZlimdbConnection::copyString(name, brokerEntity->entity, brokerEntity->user_name_size, ZLIMDB_MAX_ENTITY_SIZE) ||
        !ZlimdbConnection::copyString(key, brokerEntity->entity, brokerEntity->key_size, ZLIMDB_MAX_ENTITY_SIZE) ||
        !ZlimdbConnection::copyString(secret, brokerEntity->entity, brokerEntity->secret_size, ZLIMDB_MAX_ENTITY_SIZE))
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
      brokerEntity->broker_type_id = createArgs->broker_type_id;
      brokerEntity->state = meguco_user_broker_stopped;

      // get new brokerId;
      uint64_t brokerId = user.getNewBrokerId();

      // create broker table
      uint32_t brokerTableId;
      String tablePrefix = String("users/") + user.getName() + "/brokers/" + String::fromUInt64(brokerId);
      if(!connection.createTable(tablePrefix + "/broker", brokerTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // add broker entity
      uint64_t id;
      if(!connection.add(brokerTableId, brokerEntity->entity, id))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // create other tables
      uint32_t newTableId;
      if(!connection.createTable(tablePrefix + "/balance", newTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(!connection.createTable(tablePrefix + "/orders", newTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(!connection.createTable(tablePrefix + "/transactions", newTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(!connection.createTable(tablePrefix + "/log", newTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // send answer
      return (void_t)connection.sendControlResponse(requestId, (const byte_t*)&brokerId, sizeof(brokerId));
    }
    break;
  case meguco_user_control_remove_broker:
    if(size < sizeof(uint64_t))
      return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
    {
      uint64_t brokerId = *(uint64_t*)data;

      // find user
      Broker* broker = user.findBroker(brokerId);
      if(!broker)
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_entity_not_found);

      // kill process
      HashMap<String, Process*>::Iterator it = processesByCommand.find(broker->getCommand());
      if(it != processesByCommand.end())
      {
        Process* process = *it;
        if(!connection.remove(processesTableId, process->entityId))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      }

      // remove tables
      if(broker->getBrokerTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, broker->getBrokerTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(broker->getBalanceTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, broker->getBalanceTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(broker->getOrdersTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, broker->getOrdersTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(broker->getTransactionsTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, broker->getTransactionsTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(broker->getLogTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, broker->getLogTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // send answer
      return (void_t)connection.sendControlResponse(requestId, 0, 0);
    }
    break;
  case meguco_user_control_create_session:
    if(size < sizeof(meguco_user_session_entity))
      return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
    {
      // get arguments
      const meguco_user_session_entity* createArgs = (const meguco_user_session_entity*)data;
      String name;
      if(!ZlimdbConnection::getString(createArgs->entity, sizeof(meguco_user_session_entity), createArgs->name_size, name))
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);

      // prepare session entity
      byte_t buffer[ZLIMDB_MAX_ENTITY_SIZE];
      meguco_user_session_entity* sessionEntity = (meguco_user_session_entity*)buffer;
      ZlimdbConnection::setEntityHeader(sessionEntity->entity, 0, 0, sizeof(meguco_user_broker_entity));
      if(!ZlimdbConnection::copyString(name, sessionEntity->entity, sessionEntity->name_size, ZLIMDB_MAX_ENTITY_SIZE))
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
      sessionEntity->bot_type_id = createArgs->bot_type_id;
      sessionEntity->state = meguco_user_session_stopped;

      // get new brokerId;
      uint64_t sessionId = user.getNewSessionId();

      // create broker table
      uint32_t sessionTableId;
      String tablePrefix = String("users/") + user.getName() + "/sessions/" + String::fromUInt64(sessionId);
      if(!connection.createTable(tablePrefix + "/session", sessionTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // add broker entity
      uint64_t id;
      if(!connection.add(sessionTableId, sessionEntity->entity, id))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // create other tables
      uint32_t newTableId;
      if(!connection.createTable(tablePrefix + "/orders", newTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(!connection.createTable(tablePrefix + "/transactions", newTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(!connection.createTable(tablePrefix + "/assets", newTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(!connection.createTable(tablePrefix + "/log", newTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(!connection.createTable(tablePrefix + "/properties", newTableId))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // send answer
      return (void_t)connection.sendControlResponse(requestId, (const byte_t*)&sessionId, sizeof(sessionId));

    }
    break;
  case meguco_user_control_remove_session:
    if(size < sizeof(uint64_t))
      return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
    {
      uint64_t sessionId = *(uint64_t*)data;

      // find user
      Session* session = user.findSession(sessionId);
      if(!session)
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_entity_not_found);

      // kill process
      HashMap<String, Process*>::Iterator it = processesByCommand.find(session->getCommand());
      if(it != processesByCommand.end())
      {
        Process* process = *it;
        if(!connection.remove(processesTableId, process->entityId))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      }

      // remove tables
      if(session->getSessionTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, session->getSessionTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(session->getOrdersTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, session->getOrdersTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(session->getTransactionsTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, session->getTransactionsTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(session->getAssetsTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, session->getAssetsTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(session->getLogTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, session->getLogTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      if(session->getPropertiesTableId() != 0)
        if(!connection.remove(zlimdb_table_tables, session->getPropertiesTableId()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // send answer
      return (void_t)connection.sendControlResponse(requestId, 0, 0);
    }
    break;
  default:
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
  }
}

void_t Main::controlUserSession(Session& session, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size)
{
  if(!session.hasEntity())
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);

  switch(controlCode)
  {
  case meguco_user_session_control_start_live:
  case meguco_user_session_control_start_simulation:
    if(session.getState() == meguco_user_session_stopped)
    {
      // update mode
      meguco_user_session_mode mode = controlCode == meguco_user_session_control_start_live ? meguco_user_session_live : meguco_user_session_simulation;
      if(session.getMode() != mode)
      {
        session.setMode(mode);
        if(!connection.update(session.getSessionTableId(), session.getEntity()))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      }

      // start process
      if(!connection.startProcess(processesTableId, session.getCommand()))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // set state to starting
      session.setState(meguco_user_session_starting);
      if(!connection.update(session.getSessionTableId(), session.getEntity()))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
    }

    // send answer
    return (void_t)connection.sendControlResponse(requestId, 0, 0);

  case meguco_user_session_control_stop:
    if(session.getState() != meguco_user_session_stopped)
    {
      HashMap<String, Process*>::Iterator it = processesByCommand.find(session.getCommand());
      if(it != processesByCommand.end())
      {
        Process* process = *it;
        if(!connection.stopProcess(processesTableId, process->entityId))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      }

      // set state to stopping
      session.setState(meguco_user_session_stopping);
      if(!connection.update(session.getSessionTableId(), session.getEntity()))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
    }

    // send answer
    return (void_t)connection.sendControlResponse(requestId, 0, 0);

  default:
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
  }
}
