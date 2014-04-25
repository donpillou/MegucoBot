
#include "MarketAdapter.h"

MarketAdapter::MarketAdapter(uint32_t id, const String& name, const String& path, const String& currencyBase, const String& currencyComm) :
  id(id), name(name), path(path), currencyBase(currencyBase), currencyComm(currencyComm) {}