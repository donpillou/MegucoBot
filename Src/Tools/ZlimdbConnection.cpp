
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

bool_t ZlimdbConnection::connect()
{
  close();

  // connect to server
  zdb = zlimdb_create(0, 0);
  if(!zdb)
  {
    error = getZlimdbError();
    return false;
  }
  if(zlimdb_connect(zdb, 0, 0, "root", "root") != 0)
  {
    error = getZlimdbError();
    zlimdb_free(zdb);
    zdb = 0;
    return false;
  }

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
