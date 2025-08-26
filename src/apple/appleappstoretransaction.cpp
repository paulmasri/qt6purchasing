#include "appleappstoretransaction.h"

AppleAppStoreTransaction::AppleAppStoreTransaction(const QString & orderId, const QString & productId,
                                                   SKPaymentTransaction * nativeTransaction, QObject * parent)
    : Transaction(orderId, productId, parent), _nativeTransaction(nativeTransaction)
{
}
