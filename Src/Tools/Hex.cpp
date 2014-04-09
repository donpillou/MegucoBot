
#include "Hex.h"

String Hex::toString(const Buffer& buffer)
{
  return toString((const byte_t*)buffer, buffer.size());
}

String Hex::toString(const byte_t* data, size_t length)
{
  String result;
  result.resize(length * 2);
  tchar_t* dest = result;
  static const tchar_t* hex = _T("0123456789ABCDEF");
  for(const byte_t* src =  data, * end = data + length; src < end; ++src)
  {
    dest[0] = hex[*src >> 4];
    dest[1] = hex[*src & 0xf];
    dest += 2;
  }
  return result;
}

bool_t Hex::fromString(const String& str, Buffer& data)
{
  size_t size = str.length();
  if(size % 2)
    return false;
  size /= 2;
  data.resize(size);
  tchar_t cu, cl;
  const tchar_t* src = str;
  for(byte_t* dest = data, * end = dest + size; dest < end; ++dest)
  {
    cu = String::toLowerCase(src[0]);
    cl = String::toLowerCase(src[1]);
    if(cu >= _T('0') && cu <= _T('9'))
      cu -= _T('0');
    else if(cu >= _T('a') && cu <= _T('f'))
      cu -= _T('a') + 10;
    else
      return false;
    if(cl >= _T('0') && cl <= _T('9'))
      cl -= _T('0');
    else if(cl >= 'a' && cl <= 'f')
      cl -= _T('a') + 10;
    else
      return false;
    *dest = cu << 4 | cl;
    src += 2;
  }
  return true;
}
