#include <qt6purchasing/abstractstorebackend.h>
#include <qt6purchasing/abstractproduct.h>
#include <qt6purchasing/abstracttransaction.h>

AbstractStoreBackend::AbstractStoreBackend(QObject * parent) : QObject(parent)
{
    qDebug() << "Creating store backend";

    connect(this, &AbstractStoreBackend::connectedChanged, [this]() {
        if (isConnected()) {
            qDebug() << "Connected to store";
            qDebug() << "Found" << _products.size() << "product(s) awaiting registration";
            for (AbstractProduct * product : _products) {
                product->registerInStore();
            }
        } else {
            qDebug() << "Disconnected from store";
        }
    });
    connect(this, &AbstractStoreBackend::productRegistered, [](AbstractProduct * product){
        qDebug() << "Product registered:" << product->identifier();
    });
    connect(this, &AbstractStoreBackend::purchaseSucceeded, [this](AbstractTransaction * transaction){
        qDebug() << "purchaseSucceeded:" << transaction->orderId();

        AbstractProduct * ap = product(transaction->productId());
        if (ap) {
            emit ap->purchaseSucceeded(transaction);
        } else {
            qCritical() << "Failed to map successful purchase to a product!";
        }
    });
    connect(this, &AbstractStoreBackend::purchaseRestored, [this](AbstractTransaction * transaction){
        qDebug() << "purchaseRestored:" << transaction->orderId();

        AbstractProduct * ap = product(transaction->productId());
        if (ap) {
            emit ap->purchaseRestored(transaction);
        } else {
            qCritical() << "Failed to map restored purchase to a product!";
        }
    });
    connect(this, &AbstractStoreBackend::purchaseFailed, [](int code){
        qDebug() << "purchaseFailed:" << code;
    });
    connect(this, &AbstractStoreBackend::purchaseConsumed, [this](AbstractTransaction * transaction){
        qDebug() << "purchaseConsumed:" << transaction->orderId();

        AbstractProduct * ap = product(transaction->productId());
        if (ap) {
            emit ap->purchaseConsumed(transaction);
        } else {
            qCritical() << "Failed to map consumed purchase to a product!";
        }
    });
}

QQmlListProperty<AbstractProduct> AbstractStoreBackend::productsQml()
{
    return QQmlListProperty<AbstractProduct>(this, nullptr,
                                             &appendProduct,
                                             &productCount,
                                             &productAt,
                                             &clearProducts);
}

AbstractProduct * AbstractStoreBackend::product(const QString &identifier)
{
    for (AbstractProduct * ap : _products) {
        if (ap->identifier() == identifier)
            return ap;
    }
    return nullptr;
}

void AbstractStoreBackend::setConnected(bool connected)
{
    if (_connected == connected)
        return;

    _connected = connected;
    emit connectedChanged();
    qDebug() << "Store connection status changed to" << (_connected ? "connected" : "disconnected");
}

// Static QQmlListProperty accessors
void AbstractStoreBackend::appendProduct(QQmlListProperty<AbstractProduct> *list, AbstractProduct *product)
{
    AbstractStoreBackend *store = qobject_cast<AbstractStoreBackend*>(list->object);
    if (store && product) {
        store->_products.append(product);
        emit store->productsChanged();
    }
}

qsizetype AbstractStoreBackend::productCount(QQmlListProperty<AbstractProduct> *list)
{
    AbstractStoreBackend *store = qobject_cast<AbstractStoreBackend*>(list->object);
    return store ? store->_products.count() : 0;
}

AbstractProduct *AbstractStoreBackend::productAt(QQmlListProperty<AbstractProduct> *list, qsizetype index)
{
    AbstractStoreBackend *store = qobject_cast<AbstractStoreBackend*>(list->object);
    return (store && index >= 0 && index < store->_products.count()) ? store->_products.at(index) : nullptr;
}

void AbstractStoreBackend::clearProducts(QQmlListProperty<AbstractProduct> *list)
{
    AbstractStoreBackend *store = qobject_cast<AbstractStoreBackend*>(list->object);
    if (store) {
        store->_products.clear();
        emit store->productsChanged();
    }
}
