#ifndef APPLEAPPSTORETRANSACTION_H
#define APPLEAPPSTORETRANSACTION_H

#include <qt6purchasing/abstracttransaction.h>

Q_FORWARD_DECLARE_OBJC_CLASS(SKPaymentTransaction);

class AppleAppStoreTransaction : public AbstractTransaction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Transaction)

public:
    enum AppleAppStoreTransactionState {
        Purchasing,
        Purchased,
        Failed,
        Restored,
        Deferred
    };
    Q_ENUM(AppleAppStoreTransactionState)
    AppleAppStoreTransaction(AbstractStoreBackend * store, SKPaymentTransaction * transaction);

    QString productId() const override;
    SKPaymentTransaction * nativeTransaction() { return _nativeTransaction; }

private:
    SKPaymentTransaction * _nativeTransaction;
};

#endif // APPLEAPPSTORETRANSACTION_H
