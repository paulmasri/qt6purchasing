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

void AbstractTransaction::setRetained(bool retained)
{
    if (_retained == retained)
        return;
    
    _retained = retained;
    emit retainedChanged();
}

void AbstractTransaction::retain()
{
    setRetained(true);
    qDebug() << "Transaction retained:" << _orderId;
}

void AbstractTransaction::destroy()
{
    qDebug() << "Transaction destroyed:" << _orderId;
    deleteLater();
}
