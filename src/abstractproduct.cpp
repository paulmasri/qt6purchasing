#include <qt6purchasing/abstractproduct.h>
#include <qt6purchasing/abstracttransaction.h>
#include <qt6purchasing/abstractstorebackend.h>

AbstractProduct::AbstractProduct(QObject * parent) : QObject(parent)
{
    connect(this, &AbstractProduct::identifierChanged, this, &AbstractProduct::registerInStore);
    connect(this, &AbstractProduct::storeChanged, this, &AbstractProduct::registerInStore);
}

AbstractStoreBackend* AbstractProduct::findStoreBackend() const
{
    QObject* p = parent();
    while (p) {
        if (auto* store = qobject_cast<AbstractStoreBackend*>(p))
            return store;
        p = p->parent();
    }
    return nullptr;
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

#ifdef Q_OS_WIN
void AbstractProduct::setMicrosoftStoreId(const QString &value)
{
    _microsoftStoreId = value;
    emit microsoftStoreIdChanged();
}
#endif

void AbstractProduct::registerInStore()
{
    auto* store = findStoreBackend();
    if (!store) {
        qCritical() << "Product not child of a store backend!";
        return;
    }

    if(!store->isConnected()) {
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
    store->registerProduct(this);
}


void AbstractProduct::purchase()
{
    auto* store = findStoreBackend();
    if (!store) {
        qCritical() << "Product not child of a store backend!";
        return;
    }

    if(!store->isConnected()) {
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

    store->purchaseProduct(this);
}
