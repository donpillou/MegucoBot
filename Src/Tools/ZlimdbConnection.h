
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
    virtual void_t controlEntity(uint32_t tableId, uint64_t entityId, uint32_t controlCode, const Buffer& buffer) = 0;
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
  bool_t query(uint32_t tableId, uint64_t entityId, Buffer& buffer);
  bool_t getResponse(Buffer& buffer);
  bool_t createTable(const String& name, uint32_t& tableId);
  bool_t copyTable(uint32_t sourceTableId, const String& name, uint32_t& tableId, bool succeedIfExists = false);
  bool_t clearTable(uint32_t tableId);
  bool_t add(uint32_t tableId, const zlimdb_entity& entity, uint64_t& id, bool_t succeedIfExists = false);
  bool_t update(uint32_t tableId, const zlimdb_entity& entity);
  bool_t remove(uint32_t tableId, uint64_t entityId);

  bool_t process();
  void_t interrupt();

public:
  static bool_t getString(const zlimdb_entity& entity, size_t offset, size_t length, String& result)
  {
    if(offset + length > entity.size)
      return false;
    result.attach((const char_t*)&entity + offset, length);
    return true;
  }

  //static bool_t getString(const void* data, size_t size, const zlimdb_entity& entity, size_t offset, size_t length, String& result)
  //{
  //  size_t end = offset + length;
  //  if(end > entity.size || (const byte_t*)&entity + end > (const byte_t*)data + size)
  //    return false;
  //  result.attach((const char_t*)&entity + offset, length);
  //  return true;
  //}

  static void_t setEntityHeader(zlimdb_entity& entity, uint64_t id, uint64_t time, uint16_t size)
  {
    entity.id = id;
    entity.time = time;
    entity.size = size;
  }

  static void_t setString(zlimdb_entity& entity, uint16_t& length, size_t offset, const String& str)
  {
    length = str.length();
    Memory::copy((byte_t*)&entity + offset, (const char_t*)str, str.length());
  }

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