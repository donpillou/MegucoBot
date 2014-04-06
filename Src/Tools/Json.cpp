
#include <nstd/Buffer.h>

#include "Json.h"

class Token
{
public:
  char token;
  Variant value;
};

static bool_t appendAsUtf8(String& str, uint_t ch)
{
  if (ch < 0x80)
  {
    str.append((char_t)ch);
    return true;
  }
  if (ch < 0x800)
  {
    str.append((ch>>6) | 0xC0);
    str.append((ch & 0x3F) | 0x80);
    return true;
  }
  if (ch < 0x10000)
  {
    str.append((ch>>12) | 0xE0);
    str.append(((ch>>6) & 0x3F) | 0x80);
    str.append((ch & 0x3F) | 0x80);
    return true;
  }
  if (ch < 0x110000)
  {
    str.append((ch>>18) | 0xF0);
    str.append(((ch>>12) & 0x3F) | 0x80);
    str.append(((ch>>6) & 0x3F) | 0x80);
    str.append((ch & 0x3F) | 0x80);
    return true;
  }
  return false;
}

static bool nextToken(const tchar_t*& data, Token& token)
{
  while(String::isSpace(*data))
    ++data;
  token.token = *data;
  switch(token.token)
  {
  case '\0':
    return true;
  case '{':
  case '}':
  case '[':
  case ']':
  case ',':
  case ':':
    ++data;
    return true;
  case '"':
    {
      ++data;
      String value;
      for(;;)
        switch(*data)
        {
        case 0:
          return false;
        case '\\':
          {
            ++data;
            switch(*data)
            {
            case '"':
            case '\\':
            case '/':
              value.append(*data);
              ++data;
              break;
            case 'b':
              value.append('\b');
              ++data;
              break;
            case 'f':
              value.append('\f');
              ++data;
              break;
            case 'n':
              value.append('\n');
              ++data;
              break;
            case 'r':
              value.append('\r');
              ++data;
              break;
            case 't':
              value.append('\t');
              ++data;
              break;
            case 'u':
              {
                ++data;
                String k(4);
                for(int i = 0; i < 4; ++i)
                  if(*data)
                  {
                    k.append(*data);
                    ++data;
                  }
                  else
                    break; // todo: return false?
                int_t i;
                if(k.scanf("%x", &i) == 1)
                  appendAsUtf8(value, i);
                // todo: else return false?
                // todo: support UTF-16 surrogate pairs encoded as 12-character sequence
                break;
              }
              break;
            default:
              value.append('\\');
              value.append(*data);
              ++data;
              break;
            }
          }
          break;
        case '"':
          ++data;
          token.value = value;
          return true;
        default:
          value.append(*data);
          ++data;
          break;
        }
    }
    return false;
  case 't':
    if(String::compare(data, "true", 4) == 0)
    {
      data += 4;
      token.value = true;
      return true;
    }
    return false;
  case 'f':
    if(String::compare(data, "false", 5) == 0)
    {
      data += 5;
      token.value = false;
      return true;
    }
    return false;
  case 'n':
    if(String::compare(data, "null", 4) == 0)
    {
      data += 4;
      token.value.clear(); // creates a null variant
      return true;
    }
    return false;
  default:
    token.token = '#';
    if(*data == '-' || String::isDigit(*data))
    {
      String n;
      bool isDouble = false;
      for(;;)
        switch(*data)
        {
        case 'E':
        case 'e':
        case '-':
        case '+':
          n.append(*data);
          ++data;
          break;
        case '.':
          isDouble = true;
          n.append(*data);
          ++data;
          break;
        default:
          if(String::isDigit(*data))
          {
            n.append(*data);
            ++data;
            break;
          }
          goto scanNumber;
        }
    scanNumber:
      if(isDouble)
      {
        token.value = n.toDouble();
        return true;
      }
      else
      {
        int64_t result = n.toInt64();
        int_t resultInt = (int_t)result;
        if((int64_t)resultInt == result)
          token.value = resultInt;
        else
          token.value = result;
        return true;
      }
    }
    return false;
  }
}

static bool_t parseObject(const tchar_t*& data, Token& token, Variant& result);
static bool_t parseValue(const tchar_t*& data, Token& token, Variant& result);
static bool_t parseArray(const tchar_t*& data, Token& token, Variant& result);

static bool_t parseObject(const tchar_t*& data, Token& token, Variant& result)
{
  if(token.token != '{')
    return false;
  if(!nextToken(data, token))
    return false;
  HashMap<String, Variant>& object = result.toMap();
  String key;
  while(token.token != '}')
  {
    if(token.token != '"')
      return false;
    key = token.value.toString();
    if(!nextToken(data, token))
      return false;
    if(token.token != ':')
      return false;
    if(!nextToken(data, token))
      return false;
    if(!parseValue(data, token, object.append(key, Variant())))
      return false;
    if(token.token == '}')
      break;
    if(token.token != ',')
      return false;
    if(!nextToken(data, token))
      return false;
  } 
  if(!nextToken(data, token)) // skip }
    return false;
  return true;
}

static bool_t parseArray(const tchar_t*& data, Token& token, Variant& result)
{
  if(token.token != '[')
    return false;
  if(!nextToken(data, token))
    return false;
  List<Variant>& list = result.toList();
  while(token.token != ']')
  {
    if(!parseValue(data, token, list.append(Variant())))
      return false;
    if(token.token == ']')
      break;
    if(token.token != ',')
      return false;
    if(!nextToken(data, token))
      return false;
  }
  if(!nextToken(data, token)) // skip ]
    return false;
  return true;
}

static bool_t parseValue(const tchar_t*& data, Token& token, Variant& result)
{
  switch(token.token)
  {
  case '"':
  case '#':
  case 't':
  case 'f':
  case 'n':
    {
      result.swap(token.value);
      if(!nextToken(data, token))
        return false;
      return true;
    }
  case '[':
    return parseArray(data, token, result);
  case '{':
    return parseObject(data, token, result);
  }
  return false;
}

bool_t Json::parse(const tchar_t* data, Variant& result)
{
  Token token;
  if(!nextToken(data, token))
    return false;
  if(!parseValue(data, token, result))
    return false;
  return true;
}

bool_t Json::parse(const String& data, Variant& result)
{
  return parse((const tchar_t*)data, result);
}

bool_t Json::parse(const Buffer& data, Variant& result)
{
  return parse((const tchar_t*)(const byte_t*)data, result);
}

bool_t Json::generate(const Variant& data, String& result)
{
  switch(data.getType())
  {
  case Variant::nullType:
    result += "null";
    return true;
  case Variant::boolType:
  case Variant::doubleType:
  case Variant::intType:
  case Variant::uintType:
  case Variant::int64Type:
  case Variant::uint64Type:
    result += data.toString();
    return true;
  case Variant::stringType:
    {
      const String str = data.toString();
      size_t strLen = str.length();
      result.reserve(result.length() + 2 + strLen * 2);
      result += '"';
      for(const tchar_t* start = str, * p = start;;)
      {
        const tchar_t* e = String::findOneOf(p, "\"\\");
        if(!e)
        {
          result.append(p, strLen - (p - start));
          break;
        }
        if(e > p)
          result.append(p, e - p);
        switch(*e)
        {
        case '"':
          result += "\\\"";
          break;
        case '\\':
          result += "\\\\";
          break;
        }
        p = e + 1;
      }
      result += '"';
      return true;
    }
  case Variant::mapType:
    {
      result += '{';
      const HashMap<String, Variant>& map = data.toMap();
      for(HashMap<String, Variant>::Iterator i = map.begin(), end = map.end();;)
      {
        result += i.key();
        result += ':';
        if(!generate(*i, result))
          return false;
        if(++i == end)
          break;
        result += ',';
      }
      result += '}';
      return true;
    }
  case Variant::listType:
    {
      result += '[';;
      const List<Variant>& list = data.toList();
      for(List<Variant>::Iterator i = list.begin(), end = list.end();;)
      {
        if(!generate(*i, result))
          return false;
        if(++i == end)
          break;
        result += ',';
      }
      result += ']';
      return true;
    }
  }
  return false;
}
