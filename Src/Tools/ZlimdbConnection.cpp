
#include <nstd/Error.h>
#include <nstd/Debug.h>

#include <zlimdbclient.h>
#include <megucoprotocol.h>

#include "ZlimdbConnection.h"

static class ZlimdbFramework
{
public:
  ZlimdbFramework()
  {
    VERIFY(zlimdb_init() == 0);
  }
  ~ZlimdbFramework()
  {
    VERIFY(zlimdb_cleanup() == 0);
  }
} zlimdbFramework;

bool_t ZlimdbConnection::connect(Callback& callback)
{
  close();

  // connect to server
  zdb = zlimdb_create(zlimdbCallback, this);
  if(!zdb)
    return error = getZlimdbError(), false;
  if(zlimdb_connect(zdb, 0, 0, "root", "root") != 0)
  {
    error = getZlimdbError();
    zlimdb_free(zdb);
    zdb = 0;
    return false;
  }
  this->callback = &callback;
  return true;
}

void_t ZlimdbConnection::close()
{
  if(zdb)
  {
    zlimdb_free(zdb);
    zdb = 0;
  }
}

bool_t ZlimdbConnection::isOpen() const
{
  return zlimdb_is_connected(zdb) == 0;
}

int ZlimdbConnection::getErrno()
{
  return zlimdb_errno();
}

bool_t ZlimdbConnection::subscribe(uint32_t tableId)
{
  if(zlimdb_subscribe(zdb, tableId, zlimdb_query_type_all, 0) != 0)
    return error = getZlimdbError(), false;
  return true;
}

bool_t ZlimdbConnection::query(uint32_t tableId)
{
  if(zlimdb_query(zdb, tableId, zlimdb_query_type_all, 0) != 0)
    return error = getZlimdbError(), false;
  return true;
}

bool_t ZlimdbConnection::query(uint32_t tableId, uint64_t entityId, byte_t (&buffer)[ZLIMDB_MAX_MESSAGE_SIZE], size_t& size)
{
  uint32_t size32 = ZLIMDB_MAX_MESSAGE_SIZE;
  if(zlimdb_query_entity(zdb, tableId, entityId, buffer, &size) != 0)
    return error = getZlimdbError(), false;
  size = size32;
  return true;
}

bool_t ZlimdbConnection::getResponse(byte_t (&buffer)[ZLIMDB_MAX_MESSAGE_SIZE], size_t& size)
{
  uint32_t size32 = ZLIMDB_MAX_MESSAGE_SIZE;
  if(zlimdb_get_response(zdb, buffer, &size) != 0)
    return error = getZlimdbError(), false;
  size = size32;
  return true;
}

bool_t ZlimdbConnection::createTable(const String& name, uint32_t& tableId)
{
  if(zlimdb_add_table(zdb, name, &tableId) != 0)
    return error = getZlimdbError(), false;
  return true;
}

bool_t ZlimdbConnection::copyTable(uint32_t sourceTableId, const String& name, uint32_t& tableId, bool succeedIfExists)
{
  if(zlimdb_copy_table(zdb, sourceTableId, name, &tableId) != 0)
  {
    if (succeedIfExists && zlimdb_errno() == zlimdb_error_table_already_exists)
      return true;
    return error = getZlimdbError(), false;
  }
  return true;
}

bool_t ZlimdbConnection::clearTable(uint32_t tableId)
{
  if(zlimdb_clear(zdb, tableId) != 0)
    return error = getZlimdbError(), false;
  return true;
}

bool_t ZlimdbConnection::add(uint32_t tableId, const zlimdb_entity& entity, uint64_t& id, bool_t succeedIfExists)
{
  if(zlimdb_add(zdb, tableId, &entity, &id) != 0)
  {
    if (succeedIfExists && zlimdb_errno() == zlimdb_error_entity_id)
      return true;
    return error = getZlimdbError(), false;
  }
  return true;
}

bool_t ZlimdbConnection::update(uint32_t tableId, const zlimdb_entity& entity)
{
  if(zlimdb_update(zdb, tableId, &entity) != 0)
    return error = getZlimdbError(), false;
  return true;
}

bool_t ZlimdbConnection::remove(uint32_t tableId, uint64_t entityId)
{
  if(zlimdb_remove(zdb, tableId, entityId) != 0)
    return error = getZlimdbError(), false;
  return true;
}

bool_t ZlimdbConnection::process()
{
  for(;;)
    if(zlimdb_exec(zdb, 60 * 1000))
    {
      switch(zlimdb_errno())
      {
      case  zlimdb_local_error_interrupted:
        return true;
      case zlimdb_local_error_timeout:
        continue;
      }
      return error = getZlimdbError(), false;
    }
}

void_t ZlimdbConnection::interrupt()
{
  zlimdb_interrupt(zdb);
}

String ZlimdbConnection::getZlimdbError()
{
  int err = zlimdb_errno();
  if(err == zlimdb_local_error_system)
    return Error::getErrorString();
  else
  {
    const char* errstr = zlimdb_strerror(err);
    return String(errstr, String::length(errstr));
  }
}

void ZlimdbConnection::zlimdbCallback(const zlimdb_header& message)
{
  switch(message.message_type)
  {
  case zlimdb_message_add_request:
    if(message.size >= sizeof(zlimdb_add_request) + sizeof(zlimdb_entity))
    {
      const zlimdb_add_request* addRequest = (const zlimdb_add_request*)&message;
      const zlimdb_entity* entity = (const zlimdb_entity*)(addRequest + 1);
      if(sizeof(zlimdb_add_request) + entity->size <= message.size)
        callback->addedEntity(addRequest->table_id, *entity);
    }
    break;
  case zlimdb_message_update_request:
    if(message.size >= sizeof(zlimdb_update_request) + sizeof(zlimdb_entity))
    {
      const zlimdb_update_request* updateRequest = (const zlimdb_update_request*)&message;
      const zlimdb_entity* entity = (const zlimdb_entity*)(updateRequest + 1);
      if(sizeof(zlimdb_update_request) + entity->size <= message.size)
        callback->updatedEntity(updateRequest->table_id, *(const zlimdb_entity*)(updateRequest + 1));
    }
    break;
  case zlimdb_message_remove_request:
    if(message.size >= sizeof(zlimdb_remove_request))
    {
      const zlimdb_remove_request* removeRequest = (const zlimdb_remove_request*)&message;
      callback->removedEntity(removeRequest->table_id, removeRequest->id);
    }
    break;
  case zlimdb_message_control_request:
    if(message.size >= sizeof(zlimdb_control_request))
    {
      const zlimdb_control_request* controlRequest = (const zlimdb_control_request*)&message;
      callback->controlEntity(controlRequest->table_id, controlRequest->id, controlRequest->control_code, (byte_t*)(controlRequest + 1), message.size - sizeof(zlimdb_control_request));
    }
  default:
    break;
  }
}
