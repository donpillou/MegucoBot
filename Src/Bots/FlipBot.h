
#pragma once

#include "Tools/Bot.h"

class FlipBot : public Bot
{
public:

private:
  class Session : public Bot::Session
  {
  public:
    Session(Broker& broker);

  private:
    Broker& broker;

    virtual ~Session() {}

    void_t updateBalance();

    void_t checkBuy(const DataProtocol::Trade& trade, const Values& values);
    void_t checkSell(const DataProtocol::Trade& trade, const Values& values);

  private: // Bot::Session
    virtual void_t handleTrade(const DataProtocol::Trade& trade, const Values& values);
    virtual void_t handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleBuyTimeout(uint32_t orderId);
    virtual void_t handleSellTimeout(uint32_t orderId);
    virtual void_t handlePropertyUpdate(const BotProtocol::SessionProperty& property) {};
    virtual void_t handleAssetUpdate(const BotProtocol::SessionItem& asset);
    virtual void_t handleAssetRemoval(const BotProtocol::SessionItem& asset);
  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
};
