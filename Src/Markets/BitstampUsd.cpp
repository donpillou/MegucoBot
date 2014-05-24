
#include <nstd/Time.h>
#include <nstd/Thread.h>
#include <nstd/Math.h>

#include "Tools/Sha256.h"
#include "Tools/Hex.h"
#include "Tools/Json.h"

#include "BitstampUsd.h"

BitstampMarket::BitstampMarket(const String& clientId, const String& key, const String& secret) :
  clientId(clientId), key(key), secret(secret),
  balanceLoaded(false), lastRequestTime(0), lastNonce(0), lastLiveTradeUpdateTime(0), nextEntityId(1) {}

bool_t BitstampMarket::loadBalanceAndFee()
{
  if(balanceLoaded)
    return true;
  BotProtocol::MarketBalance balance;
  if(!loadBalance(balance))
    return false;
  return true;
}

bool_t BitstampMarket::createOrder(uint32_t entityId, BotProtocol::Order::Type type, double price, double amount, BotProtocol::Order& order)
{
  if(!loadBalanceAndFee())
    return false;

  bool buy = type == BotProtocol::Order::Type::buy;
  double maxAmount = Math::abs(buy ? getMaxBuyAmout(price) : getMaxSellAmout());
  if(Math::abs(amount) > maxAmount)
    amount = maxAmount;

  String priceStr, amountStr;
  priceStr.printf("%.2f", price);
  amountStr.printf("%.8f", Math::abs(amount));

  HashMap<String, Variant> args;
  args.append("amount", amountStr);
  args.append("price", priceStr);

  String url = buy ? String("https://www.bitstamp.net/api/buy/") : String("https://www.bitstamp.net/api/sell/");
  Variant result;
  if(!request(url, false, args, result))
    return false;

  const HashMap<String, Variant>& orderData = result.toMap();

  String bitstampId = String("order_") + orderData.find("id")->toString();
  order.entityType = BotProtocol::marketOrder;
  if(entityId != 0)
  {
    setEntityId(bitstampId, entityId);
    order.entityId = entityId;
  }
  else
    order.entityId = getNewEntityId(bitstampId);
  String btype = orderData.find("type")->toString();
  if(btype != "0" && btype != "1")
  {
    error = "Received invalid order type.";
    return false;
  }

  buy = btype == "0";
  order.type = buy ? BotProtocol::Order::buy : BotProtocol::Order::sell;

  String dateStr = orderData.find("datetime")->toString();
  const tchar_t* lastDot = dateStr.findLast('.');
  if(lastDot)
    dateStr.resize(lastDot - (const tchar_t*)dateStr);
  Time time(true);
  if(dateStr.scanf("%d-%d-%d %d:%d:%d", &time.year, &time.month, &time.day, &time.hour, &time.min, &time.sec) != 6)
  {
    error = "Received invalid order date.";
    return false;
  }
  order.date = time.toTimestamp();

  order.price = orderData.find("price")->toDouble();
  order.amount = Math::abs(orderData.find("amount")->toDouble());
  double total = getOrderCharge(buy ? order.amount : -order.amount, order.price);
  order.fee = Math::abs(total) - Math::abs(order.price * order.amount);
  this->orders.append(order.entityId, order);

  // update balance
  if(order.amount > 0) // buy order
  {
    balance.availableUsd -= total;
    balance.reservedUsd += total;
  }
  else // sell order
  {
    balance.availableBtc -= order.amount;
    balance.reservedBtc += order.amount;
  }
  return true;
}

bool_t BitstampMarket::getOrder(uint32_t entityId, BotProtocol::Order& order)
{
  HashMap<uint32_t, BotProtocol::Order>::Iterator it = orders.find(entityId);
  if(it == orders.end())
  {
    error = "Unknown order.";
    return false;
  }
  order = *it;
  return true;
}

bool_t BitstampMarket::cancelOrder(uint32_t entityId)
{
  String bitstampId = *entityIds.find(entityId);
  if(!bitstampId.startsWith("order_"))
  {
    error = "Unknown order.";
    return false;
  }
  bitstampId = bitstampId.substr(6);

  HashMap<uint32_t, BotProtocol::Order>::Iterator it = orders.find(entityId);
  if(it == orders.end())
  {
    error = "Unknown order.";
    return false;
  }
  const BotProtocol::Order& order = *it;

  HashMap<String, Variant> args;
  args.append("id", bitstampId);
  Variant result;
  if(!request("https://www.bitstamp.net/api/cancel_order/", false, args, result))
    return false;

  // update balance
  if(order.type == BotProtocol::Order::buy) // buy order
  {
    double total = Math::abs(getOrderCharge(order.amount, order.price));
    balance.availableUsd += total;
    balance.reservedUsd -= total;
  }
  else // sell order
  {
    balance.availableBtc += order.amount;
    balance.reservedBtc -= order.amount;
  }
  orders.remove(it);
  removeEntityId(entityId);
  return true;
}

bool_t BitstampMarket::loadOrders(List<BotProtocol::Order>& orders)
{
  if(!loadBalanceAndFee())
    return false;

  Variant result;
  if(!request("https://www.bitstamp.net/api/open_orders/", false, HashMap<String, Variant>(), result))
    return false;

  const List<Variant>& ordersData = result.toList();
  this->orders.clear();
  BotProtocol::Order order;
  order.entityType = BotProtocol::marketOrder;
  Time time(true);
  for(List<Variant>::Iterator i = ordersData.begin(), end = ordersData.end(); i != end; ++i)
  {
    const Variant& orderDataVar = *i;
    HashMap<String, Variant> orderData = orderDataVar.toMap();
    
    String bitstampId = String("order_") + orderData.find("id")->toString();
    String type = orderData.find("type")->toString();
    if(type != "0" && type != "1")
      continue;

    order.entityId = getNewEntityId(bitstampId);
    bool buy = type == "0";
    order.type = buy ? BotProtocol::Order::buy : BotProtocol::Order::sell;

    String dateStr = orderData.find("datetime")->toString();

    if(dateStr.scanf("%d-%d-%d %d:%d:%d", &time.year, &time.month, &time.day, &time.hour, &time.min, &time.sec) != 6)
      continue;
    order.date = time.toTimestamp();

    order.price = orderData.find("price")->toDouble();
    order.amount = Math::abs(orderData.find("amount")->toDouble());
    double total = getOrderCharge(buy ? order.amount : -order.amount, order.price);
    order.fee = Math::abs(Math::abs(total) - order.price * order.amount);

    this->orders.append(order.entityId, order);
    orders.append(order);
  }
  return true;
}

bool_t BitstampMarket::loadBalance(BotProtocol::MarketBalance& balance)
{
  Variant result;
  if(!request("https://www.bitstamp.net/api/balance/", false, HashMap<String, Variant>(), result))
    return false;

  const HashMap<String, Variant>& balanceData = result.toMap();
  balance.entityType = BotProtocol::marketBalance;
  balance.entityId = 0;
  balance.reservedUsd = balanceData.find("usd_reserved")->toDouble();
  balance.reservedBtc = balanceData.find("btc_reserved")->toDouble();
  balance.availableUsd = balanceData.find("usd_available")->toDouble();
  balance.availableBtc = balanceData.find("btc_available")->toDouble();
  balance.fee =  balanceData.find("fee")->toDouble() * 0.01;
  this->balance = balance;
  this->balanceLoaded = true;
  return true;
}

bool_t BitstampMarket::loadTransactions(List<BotProtocol::Transaction>& transactions)
{
  Variant result;
  if(!request("https://www.bitstamp.net/api/user_transactions/", false, HashMap<String, Variant>(), result))
    return false;

  BotProtocol::Transaction transaction;
  transaction.entityType = BotProtocol::marketTransaction;
  const List<Variant>& transactionData = result.toList();
  Time time(true);
  for(List<Variant>::Iterator i = transactionData.begin(), end = transactionData.end(); i != end; ++i)
  {
    const Variant& transactionDataVar = *i;
    const HashMap<String, Variant>& transactionData = transactionDataVar.toMap();
    
    String bitstampId = String("transaction_") + transactionData.find("id")->toString();
    String type = transactionData.find("type")->toString();
    if(type != "2")
      continue;

    transaction.entityId = getNewEntityId(bitstampId);

    String dateStr = transactionData.find("datetime")->toString();
    if(dateStr.scanf("%d-%d-%d %d:%d:%d", &time.year, &time.month, &time.day, &time.hour, &time.min, &time.sec) != 6)
      continue;
    transaction.date = time.toTimestamp();

    transaction.fee = Math::abs(transactionData.find("fee")->toDouble());

    double value = transactionData.find("usd")->toDouble();
    bool buy = value < 0.;
    transaction.type = buy ? BotProtocol::Transaction::buy : BotProtocol::Transaction::sell;
    //transaction.total = buy ? -(Math::abs(value) + transaction.fee) : (fabs(value) - transaction.fee);
    transaction.amount = Math::abs(transactionData.find("btc")->toDouble());
    transaction.price = Math::abs(value) / Math::abs(transaction.amount);

    transactions.append(transaction);
  }

  return true;
}

double BitstampMarket::getMaxSellAmout() const
{
  return balance.availableBtc;
}

double BitstampMarket::getMaxBuyAmout(double price) const
{
  double fee = balance.fee; // e.g. 0.0044
  double availableUsd = balance.availableUsd;
  double additionalAvailableUsd = 0.; //floor(canceledAmount * canceledPrice * (1. + fee) * 100.) / 100.;
  double usdAmount = availableUsd + additionalAvailableUsd;
  double result = Math::floor(((100. / ( 100. + (fee * 100.))) * usdAmount) * 100.) / 100.;
  result /= price;
  result = Math::floor(result * 100000000.) / 100000000.;
  return result;
}

double BitstampMarket::getOrderCharge(double amount, double price) const
{
  if(amount < 0.) // sell order
    return Math::floor(-amount * price / (1. + balance.fee) * 100.) / 100.;
  else // buy order
    return Math::floor(amount * price * (1. + balance.fee) * -100.) / 100.;
}

bool_t BitstampMarket::request(const String& url, bool_t isPublic, const HashMap<String, Variant>& params, Variant& result)
{
  avoidSpamming();

  Buffer buffer;
  if (isPublic)
  {
    if(!httpRequest.get(url, buffer))
    {
      error = httpRequest.getErrorString();
      return false;
    }
  }
  else
  {
    uint64_t newNonce = Time::time() / 1000LL;
    if(newNonce <= lastNonce)
      newNonce = lastNonce + 1;
    lastNonce = newNonce;

    String nonce;
    nonce.printf("%llu", newNonce);
    String message = nonce + clientId + key;
    byte_t signatureBuffer[Sha256::digestSize];
    Sha256::hmac((const byte_t*)(const char_t*)secret, secret.length(), (const byte_t*)(const char_t*)message, message.length(), signatureBuffer);
    String signature = Hex::toString(signatureBuffer, sizeof(signatureBuffer));
    signature.toUpperCase();
    
    HashMap<String, String> formData;
    formData.append("key", key);
    formData.append("signature", signature);
    formData.append("nonce", nonce);
    for(HashMap<String, Variant>::Iterator j = params.begin(), end = params.end(); j != end; ++j)
      formData.append(j.key(), j->toString());

    if(!httpRequest.post(url, formData, buffer))
    {
      error = httpRequest.getErrorString();
      return false;
    }
  }

  if(!Json::parse(buffer, result) || result.isNull())
  {
    error = "Received unparsable data.";
    return false;
  }
  else if(result.getType() == Variant::mapType && result.toMap().contains("error"))
  {
    List<String> errors;
    struct ErrorStringCollector
    {
      static void collect(const Variant& var, List<String>& errors)
      {
        switch(var.getType())
        {
        case Variant::stringType:
          errors.append(var.toString());
          break;
        case Variant::listType:
          {
            const List<Variant>& list = var.toList();
            for(List<Variant>::Iterator i = list.begin(), end = list.end(); i != end; ++i)
              collect(*i, errors);
          }
          break;
        case Variant::mapType:
          {
            const HashMap<String, Variant>& map = var.toMap();
            for(HashMap<String, Variant>::Iterator i = map.begin(), end = map.end(); i != end; ++i)
              collect(*i, errors);
          }
          break;
        default:
          break;
        }
      }
    };
    ErrorStringCollector::collect(result, errors);
    error.clear();
    if(!errors.isEmpty())
      for(List<String>::Iterator i = errors.begin(), end = errors.end();; ++i)
      {
        error += *i;
        if(i == end)
          break;
        error += ' ';
      }
    return false;
  }
  else
    return true;
}

void_t BitstampMarket::avoidSpamming()
{
  const timestamp_t queryDelay = 1337LL;
  timestamp_t now = Time::time();
  timestamp_t elapsed = lastRequestTime == 0 ? queryDelay : (now - lastRequestTime);
  if(elapsed < queryDelay)
  {
    timestamp_t sleepMs = queryDelay - elapsed;
    Thread::sleep(sleepMs);
    lastRequestTime = now + sleepMs;
  }
  else
    lastRequestTime = now;
  // TODO: allow more than 1 request per second but limit requests to 600 per 10 minutes
}

void_t BitstampMarket::setEntityId(const String& bitstampId, uint32_t entityId)
{
  entityIds.append(entityId, bitstampId);
  entityIdsById.append(bitstampId, entityId);
}

uint32_t BitstampMarket::getNewEntityId(const String& bitstampId)
{
  HashMap<String, uint32_t>::Iterator it = entityIdsById.find(bitstampId);
  if(it == entityIdsById.end())
  {
    uint32_t entityId = nextEntityId++;
    entityIds.append(entityId, bitstampId);
    entityIdsById.append(bitstampId, entityId);
    return entityId;
  }
  return *it;
}

void_t BitstampMarket::removeEntityId(uint32_t entityId)
{
  HashMap<uint32_t, String>::Iterator it = entityIds.find(entityId);
  if(it != entityIds.end())
  {
    entityIdsById.remove(*it);
    entityIds.remove(it);
  }
}
