
#include "MarketAdapter.h"
#include "BotProtocol.h"
#include "ClientHandler.h"

MarketAdapter::MarketAdapter(uint32_t id, const String& name, const String& path, const String& currencyBase, const String& currencyComm) :
  id(id), name(name), path(path), currencyBase(currencyBase), currencyComm(currencyComm) {}

void_t MarketAdapter::send(ClientHandler& client)
{
  BotProtocol::MarketAdapter marketAdapterData;
  marketAdapterData.entityType = BotProtocol::marketAdapter;
  marketAdapterData.entityId = id;
  BotProtocol::setString(marketAdapterData.name, name);
  BotProtocol::setString(marketAdapterData.currencyBase, currencyBase);
  BotProtocol::setString(marketAdapterData.currencyComm, currencyComm);
  client.sendEntity(&marketAdapterData, sizeof(marketAdapterData));
}
