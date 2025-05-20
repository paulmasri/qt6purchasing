#ifndef GOOGLEPLAYSTORETRANSACTION_H
#define GOOGLEPLAYSTORETRANSACTION_H

#include <qt6purchasing/abstracttransaction.h>

class GooglePlayStoreTransaction : public AbstractTransaction
{
    Q_OBJECT
	QML_ELEMENT

public:
    GooglePlayStoreTransaction(AbstractStoreBackend * store, QJsonObject json);

    QString productId() const override;
    QJsonObject json() const { return _json; }

private:
    QJsonObject _json;

};

#endif // GOOGLEPLAYSTORETRANSACTION_H
