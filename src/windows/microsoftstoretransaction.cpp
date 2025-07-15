#include "microsoftstoretransaction.h"

MicrosoftStoreTransaction::MicrosoftStoreTransaction(
    const QString& orderId,
    const QString& productId,
    QObject * parent)
    : AbstractTransaction(orderId, parent)
    , _productId(productId)
{
}

QString MicrosoftStoreTransaction::productId() const
{
    return _productId;
}
