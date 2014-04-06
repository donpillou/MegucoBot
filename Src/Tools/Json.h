
#pragma once

#include <nstd/Variant.h>

class Buffer;

class Json
{
public:
  static bool_t parse(const tchar_t* data, Variant& result);
  static bool_t parse(const String& data, Variant& result);
  static bool_t parse(const Buffer& data, Variant& result);

  static bool_t generate(const Variant& data, String& result);
};
