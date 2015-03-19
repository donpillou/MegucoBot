
#include "Tools/ZlimdbProtocol.h"

#include "ConnectionHandler.h"
#include "User2.h"
#include "Market2.h"

ConnectionHandler::~ConnectionHandler()
{
  for(HashMap<String, User2*>::Iterator i = users.begin(), end = users.end(); i != end; ++i)
    delete *i;
  //for(HashMap<String, BotMarket*>::Iterator i = botMarketsByName.begin(), end = botMarketsByName.end(); i != end; ++i)
  //  delete *i;
  //for(HashMap<String, BotEngine*>::Iterator i = botEnginesByName.begin(), end = botEnginesByName.end(); i != end; ++i)
  //  delete *i;
}

void_t ConnectionHandler::addBotMarket(const String& name, const String& executable)
{
  BotMarket botMarket = {name, executable};
  botMarketsByName.append(name, botMarket);
}

void_t ConnectionHandler::addBotEngine(const String& name, const String& executable)
{
  BotEngine botEngine = {name, executable};
  botEnginesByName.append(name, botEngine);
}

bool_t ConnectionHandler::connect()
{
  connection.close();

  if(!connection.connect(*this))
    return error = connection.getErrorString(), false;

  Buffer buffer(ZLIMDB_MAX_MESSAGE_SIZE);

  // get table list
  uint32_t botMarketsTableId, botEnginesTableId;
  HashMap<uint32_t, String> userMarkets;
  HashMap<uint32_t, String> userSessions;
  if(connection.subscribe(zlimdb_table_tables))
    return error = connection.getErrorString(), false;
  {
    String tableName;
    while(connection.getResponse(buffer))
    {
      void* data = (byte_t*)buffer;
      uint32_t size = buffer.size();
      for(const zlimdb_table_entity* botMarket; botMarket = (const zlimdb_table_entity*)zlimdb_get_entity(sizeof(zlimdb_table_entity), &data, &size);)
      {
        if(!ZlimdbProtocol::getString(botMarket->entity, sizeof(*botMarket), botMarket->name_size, tableName))
          continue;
        if(tableName == "botMarkets")
          botMarketsTableId = (uint32_t)botMarket->entity.id;
        if(tableName == "botEngines")
          botEnginesTableId = (uint32_t)botMarket->entity.id;
        if(tableName.startsWith("users/") && tableName.endsWith("/market"))
        {
          String userName = tableName.substr(6, tableName.length() - (6 + 7));
          if(userName.find('/'))
            continue;
          userMarkets.append((uint32_t)botMarket->entity.id, userName);
        }
        if(tableName.startsWith("users/") && tableName.endsWith("/session"))
        {
          String userName = tableName.substr(6, tableName.length() - (6 + 8));
          if(userName.find('/'))
            continue;
          userSessions.append((uint32_t)botMarket->entity.id, userName);
        }
      }
    }
  }

  // update botmarkets table
  HashMap<String, uint64_t> knownBotMarkets;
  if(botMarketsTableId == 0)
    if(connection.createTable("botMarkets", botMarketsTableId))
      return error = connection.getErrorString(), false;
  else
  {
    if(!connection.query(botMarketsTableId))
      return error = connection.getErrorString(), false;
    String marketName;
    while(connection.getResponse(buffer))
    {
      void* data = (byte_t*)buffer;
      uint32_t size = buffer.size();
      for(const meguco_bot_market_entity* botMarket; botMarket = (const meguco_bot_market_entity*)zlimdb_get_entity(sizeof(meguco_bot_market_entity), &data, &size);)
      {
        if(!ZlimdbProtocol::getString(botMarket->entity, sizeof(*botMarket), botMarket->name_size, marketName))
          continue;
        knownBotMarkets.append(marketName, botMarket->entity.id);
      }
    }
  }
  buffer.resize(ZLIMDB_MAX_MESSAGE_SIZE);
  for(HashMap<String, BotMarket>::Iterator i = botMarketsByName.begin(), end = botMarketsByName.end(); i != end; ++i)
  {
    const String& marketName = i.key();
    HashMap<String, uint64_t>::Iterator it = knownBotMarkets.find(marketName);
    if(it == knownBotMarkets.end())
    {
      meguco_bot_market_entity* botMarket = (meguco_bot_market_entity*)(byte_t*)buffer;
      ZlimdbProtocol::setEntityHeader(botMarket->entity, 0, 0, sizeof(*botMarket) + marketName.length());
      ZlimdbProtocol::setString(botMarket->entity, botMarket->name_size, sizeof(*botMarket), marketName);
      uint64_t id;
      if(!connection.add(botMarketsTableId, botMarket->entity, id))
        return error = connection.getErrorString(), false;
      botMarkets.append(id, &*i);
    }
    else
    {
      knownBotMarkets.remove(it);
      botMarkets.append(*it, &*i);
    }
  }
  for(HashMap<String, uint64_t>::Iterator i = knownBotMarkets.begin(), end = knownBotMarkets.end(); i != end; ++i)
    connection.remove(botMarketsTableId, *i);
  knownBotMarkets.swap(HashMap<String, uint64_t>());

  // update bot engines list
  HashMap<String, uint64_t> knownBotEngines;
  if(botEnginesTableId == 0)
    if(connection.createTable("botEngines", botEnginesTableId))
      return error = connection.getErrorString(), false;
  else
  {
    if(!connection.query(botEnginesTableId))
      return error = connection.getErrorString(), false;
    String engineName;
    while(connection.getResponse(buffer))
    {
      void* data = (byte_t*)buffer;
      uint32_t size = buffer.size();
      for(const meguco_bot_engine_entity* botEngine; botEngine = (const meguco_bot_engine_entity*)zlimdb_get_entity(sizeof(meguco_bot_engine_entity), &data, &size);)
      {
        if(!ZlimdbProtocol::getString(botEngine->entity, sizeof(*botEngine), botEngine->name_size, engineName))
          continue;
        knownBotEngines.append(engineName, botEngine->entity.id);
      }
    }
  }
  buffer.resize(ZLIMDB_MAX_MESSAGE_SIZE);
  for(HashMap<String, BotEngine>::Iterator i = botEnginesByName.begin(), end = botEnginesByName.end(); i != end; ++i)
  {
    const String& botEngineName = i.key();
    HashMap<String, uint64_t>::Iterator it = knownBotEngines.find(botEngineName);
    if(it == knownBotEngines.end())
    {
      meguco_bot_engine_entity* botEngine = (meguco_bot_engine_entity*)(byte_t*)buffer;
      ZlimdbProtocol::setEntityHeader(botEngine->entity, 0, 0, sizeof(*botEngine) + botEngineName.length());
      ZlimdbProtocol::setString(botEngine->entity, botEngine->name_size, sizeof(*botEngine), botEngineName);
      uint64_t id;
      if(!connection.add(botEnginesTableId, botEngine->entity, id))
        return error = connection.getErrorString(), false;
      botEngines.append(id, &*i);
    }
    else
    {
      knownBotEngines.remove(it);
      botEngines.append(*it, &*i);
      // todo: add entione to marketById table
    }
  }
  for(HashMap<String, uint64_t>::Iterator i = knownBotEngines.begin(), end = knownBotEngines.end(); i != end; ++i)
    connection.remove(botEnginesTableId, *i);
  knownBotEngines.swap(HashMap<String, uint64_t>());

  // subscribe to user markets
  List<Market2*> markets;
  for(HashMap<uint32_t, String>::Iterator i = userMarkets.begin(), end = userMarkets.end(); i != end; ++i)
  {
    if(!connection.subscribe(i.key()))
    {
      if(connection.getErrno() == zlimdb_error_table_not_found)
        continue;
      return error = connection.getErrorString(), false;
    }
    while(connection.getResponse(buffer))
    {
      void* data = (byte_t*)buffer;
      uint32_t size = buffer.size();
      for(const meguco_user_market_entity* userMarket; userMarket = (const meguco_user_market_entity*)zlimdb_get_entity(sizeof(meguco_user_market_entity), &data, &size);)
      {
        if(userMarket->entity.id != 1)
          continue;
        BotMarket* botMarket = *botMarkets.find(userMarket->bot_market_id);
        User2* user = findUser(*i);
        if(!user)
          user = createUser(*i);
        Market2* market = user->createMarket(processManager, *userMarket, botMarket ? botMarket->executable : String());
        markets.append(market);
      }
    }
  }

  // start markets processes
  for(List<Market2*>::Iterator i = markets.begin(), end = markets.end(); i != end; ++i)
  {
    Market2* market = *i;
    market->startProcess();
    // todo: update entity
  }

/*
  // subscribe to user sessions
  ??
    ich muss irgendwie wenn ich ein add via subscription bekomme darauf subscriben können
    */

  return true;
}

bool_t ConnectionHandler::process()
{
  for(;;)
  {
    if(!connection.process())
      return false;
    // todo: check for terminated processes
  }
}

User2* ConnectionHandler::createUser(const String& name)
{
  User2* user = new User2();
  users.append(name, user);
  return user;
}

void_t ConnectionHandler::addedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == zlimdb_table_tables)
  {
    if(entity.size >= sizeof(zlimdb_table_entity))
    {
      const zlimdb_table_entity* tableEntity = (const zlimdb_table_entity*)&entity;
      String tableName;
      if(!ZlimdbProtocol::getString(tableEntity->entity, sizeof(*tableEntity), tableEntity->name_size, tableName))
        return;
      /*
      if(tableName.startsWith("users/") && tableName.endsWith("/market"))
        userMarkets.append(tableEntity->entity.id, tableName);
      if(tableName.startsWith("users/") && tableName.endsWith("/session"))
        userSessions.append(tableEntity->entity.id, tableName);
        ??
        */
    }
  }
}

void_t ConnectionHandler::updatedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
}

void_t ConnectionHandler::removedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
}
