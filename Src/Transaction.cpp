
#include <nstd/Time.h>

#include "Transaction.h"
#include "ClientHandler.h"
#include "Session.h"

Transaction::Transaction(Session& session, uint32_t id, double price, double amount, double fee, BotProtocol::Transaction::Type type) :
  session(session),
  id(id), price(price), amount(amount), fee(fee), type(type)
{
  date = Time::time();
}

Transaction::Transaction(Session& session, const Variant& variant) :
   session(session)
{
  const HashMap<String, Variant>& data = variant.toMap();
  id = data.find("id")->toUInt();
  price = data.find("price")->toDouble();
  amount = data.find("amount")->toDouble();
  fee = data.find("fee")->toDouble();
  type = (BotProtocol::Transaction::Type)data.find("type")->toUInt();
  date = data.find("date")->toInt64();
}

void_t Transaction::toVariant(Variant& variant)
{
  HashMap<String, Variant>& data = variant.toMap();
  data.append("id", id);
  data.append("price", price);
  data.append("amount", amount);
  data.append("fee", fee);
  data.append("type", (uint_t)type);
  data.append("date", date);
}

void_t Transaction::send(ClientHandler* client)
{
  BotProtocol::Transaction transactionData;
  transactionData.price = price;
  transactionData.amount = amount;
  transactionData.fee = fee;
  transactionData.type = type;
  transactionData.date = date;
  if(client)
    client->sendEntity(BotProtocol::transaction, id, &transactionData, sizeof(transactionData));
  else
    session.sendEntity(BotProtocol::transaction, id, &transactionData, sizeof(transactionData));
}
