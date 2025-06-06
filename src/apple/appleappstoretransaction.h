#ifndef APPLEAPPSTORETRANSACTION_H
#define APPLEAPPSTORETRANSACTION_H

#include <qt6purchasing/abstracttransaction.h>

Q_FORWARD_DECLARE_OBJC_CLASS(Transaction);

class AppleAppStoreTransaction : public AbstractTransaction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Transaction)
    QML_UNCREATABLE("Transactions are created by the store backend")

public:
    AppleAppStoreTransaction(Transaction * transaction, QObject * parent = nullptr);

    QString productId() const override;
    Transaction * nativeTransaction() { return _nativeTransaction; }

private:
    Transaction * _nativeTransaction = nullptr;
};

#endif // APPLEAPPSTORETRANSACTION_H
