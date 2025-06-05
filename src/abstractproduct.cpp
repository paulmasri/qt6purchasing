#include <qt6purchasing/abstractproduct.h>
#include <qt6purchasing/abstracttransaction.h>

AbstractProduct::AbstractProduct(QObject * parent) : QObject(parent)
{
    connect(this, &AbstractProduct::identifierChanged, this, &AbstractProduct::registerInStore);
    connect(this, &AbstractProduct::storeChanged, this, &AbstractProduct::registerInStore);
}

void AbstractProduct::setDescription(QString value)
{
    _description = value;
    emit descriptionChanged();
}

void AbstractProduct::setIdentifier(const QString &value)
{
    _identifier = value;
    emit identifierChanged();
}

void AbstractProduct::setPrice(const QString &value)
{
    _price = value;
    emit priceChanged();
}

void AbstractProduct::setProductType(ProductType type)
{
    _productType = type;
    emit productTypeChanged();
}

void AbstractProduct::setTitle(const QString &value)
{
    _title = value;
    emit titleChanged();
}

void AbstractProduct::setStatus(ProductStatus status)
{
    _status = status;
    qDebug() << "Product" << _identifier << _status;
    emit statusChanged();
}

void AbstractProduct::registerInStore()
{
    if (!AbstractStoreBackend::instance()) {
        qCritical() << "Store unavailable!";
        return;
    }

    if(!AbstractStoreBackend::instance()->isConnected()) {
        qDebug() << "No connection to store - will register when connected";
        return;
    }

    if (_identifier.isEmpty()) {
        qDebug() << "Product has no id - skipping registration";
        return;
    }

    if (_status == PendingRegistration || _status == Registered) {
        qDebug() << "Product" << _identifier << "already registered or pending";
        return;
    }

    setStatus(AbstractProduct::PendingRegistration);
    AbstractStoreBackend::instance()->registerProduct(this);
}


void AbstractProduct::purchase()
{
    if (!AbstractStoreBackend::instance()) {
        qCritical() << "Store unavailable!";
        return;
    }

    if(!AbstractStoreBackend::instance()->isConnected()) {
        qWarning() << "Cannot purchase - store not connected";
        return;
    }

    if (_identifier.isEmpty()) {
        qWarning() << "Cannot purchase - product has no identifier";
        return;
    }

    if (_status != AbstractProduct::Registered) {
        qWarning() << "Cannot purchase unregistered product:" << _identifier;
        return;
    }

    AbstractStoreBackend::instance()->purchaseProduct(this);
}
