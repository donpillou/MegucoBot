
#pragma once

#include <nstd/String.h>
#include <nstd/Buffer.h>

#include <zlimdbclient.h>

class ZlimdbConnection
{
public:
  class Callback
  {
  public:
    virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity) = 0;
    virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity) = 0;
    virtual void_t removedEntity(uint32_t tableId, uint64_t entityId) = 0;
  };

public:
  ZlimdbConnection() : zdb(0) {}
  ~ZlimdbConnection() {close();}

  bool_t connect(Callback& callback);
  void_t close();
  bool_t isOpen() const;
  int getErrno();
  const String& getErrorString() const {return error;}

  bool_t subscribe(uint32_t tableId);
  bool_t query(uint32_t tableId);
  bool_t getResponse(Buffer& buffer);
  bool_t createTable(const String& name, uint32_t& tableId);
  bool_t add(uint32_t tableId, const zlimdb_entity& entity, uint64_t& id);
  bool_t update(uint32_t tableId, const zlimdb_entity& entity);
  bool_t remove(uint32_t tableId, uint64_t entityId);

  bool_t process();
  void_t interrupt();

private:
  String error;
  zlimdb* zdb;
  Callback* callback;

private:
  static String getZlimdbError();

  void zlimdbCallback(const zlimdb_header& message);

private:
  static void zlimdbCallback(void* user_data, const zlimdb_header* message) {((ZlimdbConnection*)user_data)->zlimdbCallback(*message);}
};
