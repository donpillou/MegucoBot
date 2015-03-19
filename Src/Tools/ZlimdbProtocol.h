
#pragma once

#include <megucoprotocol.h>

#include <nstd/String.h>

class ZlimdbProtocol
{
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
};
