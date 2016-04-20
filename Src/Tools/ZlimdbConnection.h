
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

  bool_t subscribe(uint32_t tableId, uint8_t flags);
  bool_t subscribe(uint32_t tableId, zlimdb_query_type type, uint64_t param, uint8_t flags);
  bool_t unsubscribe(uint32_t tableId);
  bool_t listen(uint32_t tableId);
  bool_t sync(uint32_t tableId, int64_t& serverTime, int64_t& tableTime);
  bool_t query(uint32_t tableId);
  bool_t getResponse(byte_t (&buffer)[ZLIMDB_MAX_MESSAGE_SIZE]);
  bool_t queryEntity(uint32_t tableId, uint64_t entityId, zlimdb_entity& entity, size_t minSize, size_t maxSize);
  bool_t createTable(const String& name, uint32_t& tableId);
  bool_t findTable(const String& name, uint32_t& tableId);
  bool_t copyTable(uint32_t sourceTableId, const String& name, uint32_t& tableId);
  bool_t moveTable(const String& sourceName, uint32_t destTableId, bool succeedIfNotExist = false);
  bool_t clearTable(uint32_t tableId);
  bool_t add(uint32_t tableId, const zlimdb_entity& entity, uint64_t& id, bool_t succeedIfExists = false);
  bool_t update(uint32_t tableId, const zlimdb_entity& entity);
  bool_t remove(uint32_t tableId, uint64_t entityId);
  bool_t control(uint32_t tableId, uint64_t entityId, uint32_t controlCode, const void_t* data, uint32_t size, byte_t(&buffer)[ZLIMDB_MAX_MESSAGE_SIZE]);

  bool_t startProcess(uint32_t tableId, const String& command);
  bool_t stopProcess(uint32_t tableId, uint64_t entityId);

  bool_t sendControlResponse(uint32_t requestId, const byte_t* data, size_t size);
  bool_t sendControlResponse(uint32_t requestId, uint16_t error);

  bool_t process();
  void_t interrupt();

public:
  static int_t getErrno();
  static void_t setErrno(int_t error);
  static String getErrorString();

  static bool_t getString(const zlimdb_entity& entity, size_t offset, size_t length, String& result)
  {
    if(!length || offset + length > entity.size)
      return zlimdb_seterrno(zlimdb_local_error_invalid_message_data), false;
    char_t* str = (char_t*)&entity + offset;
    if(str[--length])
      return zlimdb_seterrno(zlimdb_local_error_invalid_message_data), false;
    result.attach(str, length);
    return true;
  }

  static  bool_t getString(const zlimdb_entity& entity, size_t length, String& result, size_t& offset)
  {
    if(!length || offset + length > entity.size)
      return zlimdb_seterrno(zlimdb_local_error_invalid_message_data), false;
    char_t* str = (char_t*)&entity + offset;
    size_t strLen = length - 1;
    if(str[strLen])
      return zlimdb_seterrno(zlimdb_local_error_invalid_message_data), false;
    result.attach(str, strLen);
    offset += length;
    return true;
  }

  static  bool_t getString(const void* data, size_t size, size_t offset, size_t length, String& result)
  {
    if(!length || offset + length > size)
      return zlimdb_seterrno(zlimdb_local_error_invalid_message_data), false;
    const char_t* str = (char_t*)data + offset;
    if(str[--length])
      return zlimdb_seterrno(zlimdb_local_error_invalid_message_data), false;
    result.attach(str, length);
    return true;
  }

  static void_t setEntityHeader(zlimdb_entity& entity, uint64_t id, uint64_t time, uint16_t size)
  {
    entity.id = id;
    entity.time = time;
    entity.size = size;
  }

  static bool_t copyString(const String& str, zlimdb_entity& entity, uint16_t& length, size_t maxSize)
  {
    length = (uint16_t)(str.length() + 1);
    if((size_t)entity.size + length > maxSize)
      return zlimdb_seterrno(zlimdb_local_error_invalid_parameter), false;
    Memory::copy((byte_t*)&entity + entity.size, (const char_t*)str, length);
    entity.size += length;
    return true;
  }

private:
  zlimdb* zdb;
  Callback* callback;

private:
  void zlimdbCallback(const zlimdb_header& message);

private:
  static void zlimdbCallback(void* userData, const zlimdb_header* message) {((ZlimdbConnection*)userData)->zlimdbCallback(*message);}
};
