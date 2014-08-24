
#pragma once

#include "Tools/Bot.h"

class TestBot : public Bot
{
public:

private:
  class Session : public Bot::Session
  {
  public:
    Session(Broker& broker) : broker(broker), updateCount(0) {}

  private:
    Broker& broker;
    int_t updateCount;

  private:
    virtual ~Session() {}

  private: // Bot::Session
    virtual void_t handleTrade(const DataProtocol::Trade& trade, const Values& values);
    virtual void_t handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleBuyTimeout(uint32_t orderId) {}
    virtual void_t handleSellTimeout(uint32_t orderId) {}
    virtual void_t handlePropertyUpdate(BotProtocol::SessionProperty& property) {};
  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
};
