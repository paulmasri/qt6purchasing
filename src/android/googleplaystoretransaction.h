#ifndef GOOGLEPLAYSTORETRANSACTION_H
#define GOOGLEPLAYSTORETRANSACTION_H

#include <qt6purchasing/abstracttransaction.h>

class GooglePlayStoreTransaction : public AbstractTransaction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Transaction)
    QML_UNCREATABLE("Transactions are created by the store backend")

public:
    GooglePlayStoreTransaction(QJsonObject json, QObject * parent = nullptr);

    QString productId() const override;
    QJsonObject json() const { return _json; }

private:
    QJsonObject _json;

};

#endif // GOOGLEPLAYSTORETRANSACTION_H
