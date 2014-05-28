
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
    virtual void setParameters(double* parameters);

    virtual void handle(const DataProtocol::Trade& trade, const Values& values);
    virtual void handleBuy(const BotProtocol::Transaction& transaction);
    virtual void handleSell(const BotProtocol::Transaction& transaction);

  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
  virtual unsigned int getParameterCount() const {return 0;}
};
