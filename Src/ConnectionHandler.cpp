
#include "Tools/ZlimdbProtocol.h"

#include "ConnectionHandler.h"

void_t ConnectionHandler::addMarketAdapter(const String& name, const String& path)
{
}

void_t ConnectionHandler::addBotEngine(const String& name, const String& executable)
{
}

bool_t ConnectionHandler::connect()
{
  if(!connection.connect())
    return error = connection.getErrorString(), false;

  // todo: update botmarkets table

  // todo: update bot engines list

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

void_t ConnectionHandler::addedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  switch(tableId)
  {
  case zlimdb_table_tables:
    {
    /*
      String tableName;
      ZlimdbProtocol::getString(tableName);
      if(tableName == "botmarkets")
        botMarketsTableId = tableId;
      if(tableName == "botengines")
        botEnginesTableId = tableId;
      if(isUserMarketTable(tableName))
        launchUserMarketProcess();
      if(isUserSessionTable(tableName))
        subscribeToUserSessionProcess();
        */
    }
    break;
  default:
    break;
  }
}

void_t ConnectionHandler::updatedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
}

void_t ConnectionHandler::removedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
}
