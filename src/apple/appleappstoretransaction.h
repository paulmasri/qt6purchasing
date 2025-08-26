#ifndef APPLEAPPSTORETRANSACTION_H
#define APPLEAPPSTORETRANSACTION_H

#include <qt6purchasing/transaction.h>

Q_FORWARD_DECLARE_OBJC_CLASS(SKPaymentTransaction);

class AppleAppStoreTransaction : public Transaction
{
    Q_OBJECT
    
public:
    explicit AppleAppStoreTransaction(const QString & orderId, const QString & productId,
                                     SKPaymentTransaction * nativeTransaction, QObject * parent = nullptr);
    
    SKPaymentTransaction * nativeTransaction() const { return _nativeTransaction; }
    
private:
    SKPaymentTransaction * _nativeTransaction;
};

#endif // APPLEAPPSTORETRANSACTION_H
