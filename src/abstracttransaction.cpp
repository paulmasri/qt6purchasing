#include <qt6purchasing/abstractstorebackend.h>
#include <qt6purchasing/abstracttransaction.h>
#include <QJsonObject>

AbstractTransaction::AbstractTransaction(QString orderId, QObject * parent) : QObject(parent),
    _orderId(orderId)
{
    _store = qobject_cast<AbstractStoreBackend*>(parent);
    Q_ASSERT(_store);
}

void AbstractTransaction::finalize()
{
    if (!_store)
        return;

    qDebug() << "Transaction: Finalizing purchase" << this->_orderId;
    _store->consumePurchase(this);
}
