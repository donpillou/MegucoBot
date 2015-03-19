
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
  zdb = zlimdb_create(0, 0);
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

bool_t ZlimdbConnection::getResponse(Buffer& buffer)
{
  buffer.resize(ZLIMDB_MAX_MESSAGE_SIZE);
  uint32_t size = ZLIMDB_MAX_MESSAGE_SIZE;
  if(zlimdb_get_response(zdb, buffer, &size) != 0)
    return error = getZlimdbError(), false;
  buffer.resize(size);
  return true;
}

bool_t ZlimdbConnection::createTable(const String& name, uint32_t& tableId)
{
  if(zlimdb_add_table(zdb, name, &tableId) != 0)
    return error = getZlimdbError(), false;
  return true;
}

bool_t ZlimdbConnection::add(uint32_t tableId, const zlimdb_entity& entity, uint64_t& id)
{
  if(zlimdb_add(zdb, tableId, &entity, &id) != 0)
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
      return false;
    }
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
