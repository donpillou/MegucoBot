
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
    return false;
  if(zlimdb_connect(zdb, 0, 0, "root", "root") != 0)
  {
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

bool_t ZlimdbConnection::isOpen() const {return zlimdb_is_connected(zdb) == 0;}
int_t ZlimdbConnection::getErrno() {return zlimdb_errno();}
void_t ZlimdbConnection::setErrno(int_t error) {zlimdb_seterrno(error);}

bool_t ZlimdbConnection::subscribe(uint32_t tableId, uint8_t flags)
{
  if(zlimdb_subscribe(zdb, tableId, zlimdb_query_type_all, 0, flags) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::subscribe(uint32_t tableId, zlimdb_query_type type, uint64_t param, uint8_t flags)
{
  if(zlimdb_subscribe(zdb, tableId, type, param, flags) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::listen(uint32_t tableId)
{
  if(zlimdb_subscribe(zdb, tableId, zlimdb_query_type_since_next, 0, zlimdb_subscribe_flag_responder) != 0)
    return false;
  byte_t buffer[ZLIMDB_MAX_MESSAGE_SIZE];
  while(zlimdb_get_response(zdb, (zlimdb_header*)buffer, ZLIMDB_MAX_MESSAGE_SIZE) == 0);
  if(zlimdb_errno() != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::sync(uint32_t tableId, int64_t& serverTime, int64_t& tableTime)
{
  if(zlimdb_sync(zdb, tableId, &serverTime, &tableTime) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::query(uint32_t tableId)
{
  if(zlimdb_query(zdb, tableId, zlimdb_query_type_all, 0) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::getResponse(byte_t (&buffer)[ZLIMDB_MAX_MESSAGE_SIZE])
{
  if(zlimdb_get_response(zdb, (zlimdb_header*)buffer, ZLIMDB_MAX_MESSAGE_SIZE) != 0)
    return false;
  return true;
}
/*
bool_t ZlimdbConnection::queryEntity(uint32_t tableId, uint64_t entityId, byte_t (&buffer)[ZLIMDB_MAX_MESSAGE_SIZE])
{
  if(zlimdb_query(zdb, tableId, zlimdb_query_type_by_id, entityId) != 0)
    return error = getZlimdbError(), false;
  if(zlimdb_get_response(zdb, (zlimdb_header*)buffer, ZLIMDB_MAX_MESSAGE_SIZE) != 0)
    return error = getZlimdbError(), false;
  byte_t buffer2[ZLIMDB_MAX_MESSAGE_SIZE];
  while(zlimdb_get_response(zdb, (zlimdb_header*)buffer2, ZLIMDB_MAX_MESSAGE_SIZE) == 0);
  if(zlimdb_errno() != 0)
    return error = getZlimdbError(), false;
  return true;
}
*/
bool_t ZlimdbConnection::queryEntity(uint32_t tableId, uint64_t entityId, zlimdb_entity& entity, size_t minSize, size_t maxSize)
{
  if(zlimdb_query_entity(zdb, tableId, entityId, &entity, minSize, maxSize) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::createTable(const String& name, uint32_t& tableId)
{
  if(zlimdb_add_table(zdb, name, &tableId) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::findTable(const String& name, uint32_t& tableId)
{
  if(zlimdb_find_table(zdb, name, &tableId) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::copyTable(uint32_t sourceTableId, const String& name, uint32_t& tableId, bool succeedIfExists)
{
  if(zlimdb_copy_table(zdb, sourceTableId, name, &tableId) != 0)
  {
    if (succeedIfExists && zlimdb_errno() == zlimdb_error_table_already_exists)
      return true;
    return false;
  }
  return true;
}

bool_t ZlimdbConnection::moveTable(const String& sourceName, const String& destName, uint32_t destTableId, uint32_t& newDestTableId, bool succeedIfNotExist)
{
  uint32_t sourceTableId;
  if(zlimdb_find_table(zdb, sourceName, &sourceTableId) != 0)
  {
    if(succeedIfNotExist && zlimdb_errno() == zlimdb_error_table_not_found)
    {
      newDestTableId = destTableId;
      return true;
    }
    return false;
  }
  if(destTableId != 0 && zlimdb_remove_table(zdb, destTableId) != 0)
    return false;
  if(zlimdb_copy_table(zdb, sourceTableId, destName, &newDestTableId) != 0)
    return false;
  if(zlimdb_remove_table(zdb, sourceTableId) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::clearTable(uint32_t tableId)
{
  if(zlimdb_clear(zdb, tableId) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::add(uint32_t tableId, const zlimdb_entity& entity, uint64_t& id, bool_t succeedIfExists)
{
  if(zlimdb_add(zdb, tableId, &entity, &id) != 0)
  {
    if (succeedIfExists && zlimdb_errno() == zlimdb_error_entity_id)
      return true;
    return false;
  }
  return true;
}

bool_t ZlimdbConnection::update(uint32_t tableId, const zlimdb_entity& entity)
{
  if(zlimdb_update(zdb, tableId, &entity) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::remove(uint32_t tableId, uint64_t entityId)
{
  if(zlimdb_remove(zdb, tableId, entityId) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::control(uint32_t tableId, uint64_t entityId, uint32_t controlCode, const void_t* data, uint32_t size, byte_t(&buffer)[ZLIMDB_MAX_MESSAGE_SIZE])
{
  if(zlimdb_control(zdb, tableId, entityId, controlCode, data, size, (zlimdb_header*)buffer, ZLIMDB_MAX_MESSAGE_SIZE) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::startProcess(uint32_t tableId, const String& command)
{
  char buffer[ZLIMDB_MAX_MESSAGE_SIZE];
  meguco_process_entity* process = (meguco_process_entity*)buffer;
  setEntityHeader(process->entity, 0, 0, sizeof(meguco_process_entity));
  if(!copyString(command, process->entity, process->cmd_size, ZLIMDB_MAX_ENTITY_SIZE))
  {
    zlimdb_seterrno(zlimdb_local_error_invalid_parameter);
    return false;
  }
  if(zlimdb_control(zdb, tableId, 0, meguco_process_control_start, process, process->entity.size, (zlimdb_header*)buffer, ZLIMDB_MAX_MESSAGE_SIZE) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::stopProcess(uint32_t tableId, uint64_t entityId)
{
  char buffer[ZLIMDB_MAX_MESSAGE_SIZE];
  if(zlimdb_control(zdb, tableId, entityId, meguco_process_control_stop, 0, 0, (zlimdb_header*)buffer, ZLIMDB_MAX_MESSAGE_SIZE) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::sendControlResponse(uint32_t requestId, const byte_t* data, size_t size)
{
  if(zlimdb_control_respond(zdb, requestId, data, size) != 0)
    return false;
  return true;
}

bool_t ZlimdbConnection::sendControlResponse(uint32_t requestId, uint16_t error)
{
  if(zlimdb_control_respond_error(zdb, requestId, error) != 0)
    return false;
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
      return false;
    }
}

void_t ZlimdbConnection::interrupt()
{
  zlimdb_interrupt(zdb);
}

String ZlimdbConnection::getErrorString()
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
      callback->controlEntity(controlRequest->table_id, controlRequest->header.request_id, controlRequest->id, controlRequest->control_code, (byte_t*)(controlRequest + 1), message.size - sizeof(zlimdb_control_request));
    }
  default:
    break;
  }
}
