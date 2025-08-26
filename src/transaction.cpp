#include <qt6purchasing/transaction.h>

Transaction::Transaction(const QString & orderId, const QString & productId, QObject * parent)
    : QObject(parent), _orderId(orderId), _productId(productId)
{
}
