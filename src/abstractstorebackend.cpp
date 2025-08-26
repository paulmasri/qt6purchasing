#include <qt6purchasing/abstractstorebackend.h>
#include <qt6purchasing/abstractproduct.h>
#include <qt6purchasing/abstracttransaction.h>

#include <QTimer>

AbstractStoreBackend::AbstractStoreBackend(QObject * parent) : QObject(parent)
{
    qDebug() << "Creating store backend";

    connect(this, &AbstractStoreBackend::connectedChanged, this, [this]() {
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

    connect(this, &AbstractStoreBackend::productRegistered, this, [](AbstractProduct * product){
        qDebug() << "Product registered:" << product->identifier();
    });

    connect(this, &AbstractStoreBackend::purchaseSucceeded, this, [this](AbstractTransaction * transaction){
        qDebug() << "purchaseSucceeded:" << transaction->orderId();

        AbstractProduct * ap = product(transaction->productId());
        if (ap) {
            emit ap->purchaseSucceeded(transaction);
        } else {
            qCritical() << "Failed to map successful purchase to a product!";
        }

        // Auto-cleanup unless retained
        QTimer::singleShot(0, this, [transaction]() {
            if (!transaction->isRetained()) {
                transaction->deleteLater();
            }
        });
    });

    connect(this, &AbstractStoreBackend::purchasePending, this, [this](AbstractTransaction * transaction){
        qDebug() << "purchasePending:" << transaction->orderId();

        AbstractProduct * ap = product(transaction->productId());
        if (ap) {
            emit ap->purchasePending(transaction);
        } else {
            qCritical() << "Failed to map pending purchase to a product!";
        }

        // Auto-cleanup unless retained
        QTimer::singleShot(0, this, [transaction]() {
            if (!transaction->isRetained()) {
                transaction->deleteLater();
            }
        });
    });

    connect(this, &AbstractStoreBackend::purchaseRestored, this, [this](AbstractTransaction * transaction){
        qDebug() << "purchaseRestored:" << transaction->orderId();

        AbstractProduct * ap = product(transaction->productId());
        if (ap) {
            emit ap->purchaseRestored(transaction);
        } else {
            qCritical() << "Failed to map restored purchase to a product!";
        }

        // Auto-cleanup unless retained
        QTimer::singleShot(0, this, [transaction]() {
            if (!transaction->isRetained()) {
                transaction->deleteLater();
            }
        });
    });

    connect(this, &AbstractStoreBackend::purchaseFailed, this,
            [this](const QString& productId, int error, int platformCode, const QString& message){
        qDebug() << "purchaseFailed:" << "productId=" << productId 
                 << "error=" << error << "platformCode=" << platformCode 
                 << "message=" << message;
        
        // Route to the appropriate product
        AbstractProduct * ap = product(productId);
        if (ap) {
            emit ap->purchaseFailed(error, platformCode, message);
        } else {
            qWarning() << "Failed to find product for purchase failure:" << productId;
        }
    });

    connect(this, &AbstractStoreBackend::consumePurchaseSucceeded, this, [this](AbstractTransaction * transaction){
        qDebug() << "consumePurchaseSucceeded:" << transaction->orderId();

        AbstractProduct * ap = product(transaction->productId());
        if (ap) {
            emit ap->consumePurchaseSucceeded(transaction);
        } else {
            qCritical() << "Failed to map consumed purchase to a product!";
        }

        // Auto-cleanup unless retained
        QTimer::singleShot(0, this, [transaction]() {
            if (!transaction->isRetained()) {
                transaction->deleteLater();
            }
        });
    });

    connect(this, &AbstractStoreBackend::consumePurchaseFailed, this, [this](AbstractTransaction * transaction){
        qDebug() << "consumePurchaseFailed:" << transaction->orderId();

        AbstractProduct * ap = product(transaction->productId());
        if (ap) {
            emit ap->consumePurchaseFailed(transaction);
        } else {
            qCritical() << "Failed to map failed consumption to a product!";
        }

        // Auto-cleanup unless retained
        QTimer::singleShot(0, this, [transaction]() {
            if (!transaction->isRetained()) {
                transaction->deleteLater();
            }
        });
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

void AbstractStoreBackend::setCanMakePurchases(bool canMakePurchases)
{
    if (_canMakePurchases == canMakePurchases)
        return;

    _canMakePurchases = canMakePurchases;
    emit canMakePurchasesChanged();
    qDebug() << "Store canMakePurchases status changed to" << (_canMakePurchases ? "enabled" : "disabled");
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
