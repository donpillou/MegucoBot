
#pragma once

#include <nstd/String.h>
#include <nstd/Buffer.h>

class Hex
{
public:
  static String toString(const Buffer& buffer);
  static String toString(const byte_t* data, size_t length);
  static bool_t fromString(const String& str, Buffer& data);
};
