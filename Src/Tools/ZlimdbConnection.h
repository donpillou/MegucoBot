
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
    virtual void_t removedEntity(uint32_t tableId, uint64_t entityId) = 0;
    virtual void_t controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size) = 0;
  };

public:
  ZlimdbConnection() : zdb(0) {}
  ~ZlimdbConnection() {close();}

  bool_t connect(Callback& callback);
  void_t close();
  bool_t isOpen() const;
  int getErrno();
  const String& getErrorString() const {return error;}

  bool_t subscribe(uint32_t tableId, uint8_t flags);
  bool_t listen(uint32_t tableId);
  bool_t query(uint32_t tableId);
  bool_t getResponse(byte_t (&buffer)[ZLIMDB_MAX_MESSAGE_SIZE]);
  bool_t queryEntity(uint32_t tableId, uint64_t entityId, zlimdb_entity& entity, size_t minSize, size_t maxSize);
  bool_t createTable(const String& name, uint32_t& tableId);
  bool_t copyTable(uint32_t sourceTableId, const String& name, uint32_t& tableId, bool succeedIfExists = false);
  bool_t clearTable(uint32_t tableId);
  bool_t add(uint32_t tableId, const zlimdb_entity& entity, uint64_t& id, bool_t succeedIfExists = false);
  bool_t update(uint32_t tableId, const zlimdb_entity& entity);
  bool_t remove(uint32_t tableId, uint64_t entityId);

  bool_t startProcess(uint32_t tableId, const String& command);

  bool_t sendControlResponse(uint32_t requestId, const byte_t* data, size_t size);
  bool_t sendControlResponse(uint32_t requestId, uint16_t error);

  bool_t process();
  void_t interrupt();

public:
  static bool_t getString(const zlimdb_entity& entity, size_t offset, size_t length, String& result) // todo: remove this
  {
    if(!length || offset + length > entity.size)
      return false;
    char_t* str = (char_t*)&entity + offset;
    if(str[--length])
      return false;
    result.attach(str, length);
    return true;
  }

  static bool_t getString2(const zlimdb_entity& entity, size_t& offset, size_t length, String& result) // todo: rename to getString
  {
    if(!length || offset + length > entity.size)
      return false;
    char_t* str = (char_t*)&entity + offset;
    size_t strLen = length - 1;
    if(str[strLen])
      return false;
    result.attach(str, strLen);
    offset += length;
    return true;
  }

  static void_t setEntityHeader(zlimdb_entity& entity, uint64_t id, uint64_t time, uint16_t size)
  {
    entity.id = id;
    entity.time = time;
    entity.size = size;
  }

  static bool_t copyString(zlimdb_entity& entity, uint16_t& length, const String& str, size_t maxSize)
  {
    length = str.length() + 1;
    if((size_t)entity.size + length > maxSize)
      return false;
    Memory::copy((byte_t*)&entity + entity.size, (const char_t*)str, length);
    entity.size += length;
    return true;
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
