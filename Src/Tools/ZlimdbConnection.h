
#pragma once

#include <nstd/String.h>

#include <zlimdbclient.h>

class ZlimdbConnection
{
public:
  class Callback
  {
  public:
    virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity) = 0;
    virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity) = 0;
    virtual void_t removedEntity(uint32_t tableId, const zlimdb_entity& entity) = 0;
  };

public:
  ZlimdbConnection() : zdb(0) {}
  ~ZlimdbConnection() {close();}

  bool_t connect();
  void_t close();
  bool_t isOpen() const;
  const String& getErrorString() const {return error;}

  bool_t process();

private:
  String error;
  zlimdb* zdb;

private:
  static String getZlimdbError();
};
