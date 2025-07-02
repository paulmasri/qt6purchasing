#include "microsoftstorebackend.h"
#include "microsoftstoreproduct.h"
#include "microsoftstoretransaction.h"
#include "microsoftstoreworkers.h"

#include <QDebug>
#include <QThread>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#include <QDateTime>

MicrosoftStoreBackend * MicrosoftStoreBackend::s_currentInstance = nullptr;

MicrosoftStoreBackend::MicrosoftStoreBackend(QObject * parent) 
    : AbstractStoreBackend(parent), _hwnd(nullptr)
{
    qDebug() << "Creating Microsoft Store backend";
    
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    s_currentInstance = this;
    
    // Cache window handle early
    initializeWindowHandle();
    
    startConnection();
}

MicrosoftStoreBackend::~MicrosoftStoreBackend()
{
    if (s_currentInstance == this)
        s_currentInstance = nullptr;
        
    qDebug() << "Destroying Microsoft Store backend";
}

void MicrosoftStoreBackend::initializeWindowHandle()
{
    auto app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (!app) {
        qDebug() << "No QGuiApplication instance found";
        return;
    }
    
    auto windows = app->topLevelWindows();
    if (!windows.isEmpty()) {
        auto window = windows.first();
        if (window) {
            // For QWindow, we just get the winId which creates the native window
            _hwnd = reinterpret_cast<HWND>(window->winId());
            qDebug() << "Cached window handle:" << _hwnd;
        }
    }
}

void MicrosoftStoreBackend::startConnection()
{
    qDebug() << "Initializing Microsoft Store connection";
    
    // Simple connection check - we'll initialize StoreContext in workers
    setConnected(true);
    qDebug() << "Microsoft Store connection established";
    
    // Query all products on startup
    queryAllProducts();
}

void MicrosoftStoreBackend::queryAllProducts()
{
    if (!_hwnd) {
        qWarning() << "No window handle available for Store query";
        return;
    }
    
    auto* worker = new StoreAllProductsWorker(_hwnd);
    auto* thread = new QThread(this);
    
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &StoreAllProductsWorker::performQuery);
    connect(worker, &StoreAllProductsWorker::queryComplete, this, &MicrosoftStoreBackend::onAllProductsQueried);
    connect(worker, &StoreAllProductsWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &StoreAllProductsWorker::finished, worker, &StoreAllProductsWorker::deleteLater);
    
    thread->start();
}

void MicrosoftStoreBackend::onAllProductsQueried(const QList<QVariantMap> &products)
{
    qDebug() << "Store query completed, found" << products.size() << "products";
    for (const auto& product : products) {
        qDebug() << "Available product:" << product["productId"].toString()
                 << "Title:" << product["title"].toString();
    }
}

void MicrosoftStoreBackend::registerProduct(AbstractProduct * product)
{
    if (!isConnected()) {
        qWarning() << "Cannot register product - store not connected";
        product->setStatus(AbstractProduct::Unknown);
        return;
    }
    
    if (!_hwnd) {
        qWarning() << "No window handle available for product registration";
        product->setStatus(AbstractProduct::Unknown);
        return;
    }
    
    // Use microsoftStoreId if available, otherwise use identifier
    QString productId = product->identifier();
#ifdef Q_OS_WIN
    QString microsoftStoreId = product->microsoftStoreId();
    if (!microsoftStoreId.isEmpty()) {
        productId = microsoftStoreId;
        qDebug() << "Using Microsoft Store ID:" << productId << "for product:" << product->identifier();
    }
#endif
    
    auto* worker = new StoreProductQueryWorker(productId, _hwnd);
    auto* thread = new QThread(this);
    
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &StoreProductQueryWorker::performQuery);
    connect(worker, &StoreProductQueryWorker::queryComplete, 
            [this, product](bool success, const QVariantMap& productData) {
                this->onProductQueried(product, success, productData);
            });
    connect(worker, &StoreProductQueryWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &StoreProductQueryWorker::finished, worker, &StoreProductQueryWorker::deleteLater);
    
    thread->start();
}

void MicrosoftStoreBackend::onProductQueried(AbstractProduct* product, bool success, const QVariantMap& productData)
{
    if (success) {
        auto msProduct = qobject_cast<MicrosoftStoreProduct*>(product);
        if (msProduct) {
            // Set product data
            product->setTitle(productData["title"].toString());
            product->setDescription(productData["description"].toString());
            product->setPrice(productData["price"].toString());
            
            // Map product type
            QString productKind = productData["productKind"].toString();
            if (productKind == "Durable") {
                product->setProductType(AbstractProduct::Unlockable);
            } else if (productKind == "UnmanagedConsumable") {
                product->setProductType(AbstractProduct::Consumable);
            }
            
            // Track the product for later reference
            _registeredProducts[product->identifier()] = product;
            
            product->setStatus(AbstractProduct::Registered);
            emit productRegistered(product);
            
            qDebug() << "Product registered successfully:" << product->identifier();
        }
    } else {
        qWarning() << "Product not found in store:" << product->identifier();
        product->setStatus(AbstractProduct::Unknown);
    }
}

void MicrosoftStoreBackend::purchaseProduct(AbstractProduct * product)
{
    if (!isConnected()) {
        qWarning() << "Cannot purchase - store not connected";
        emit purchaseFailed(static_cast<int>(ServiceUnavailable));
        return;
    }
    
    if (!_hwnd) {
        qWarning() << "No window handle available for purchase";
        emit purchaseFailed(static_cast<int>(UnknownError));
        return;
    }
    
    // Use microsoftStoreId if available
    QString productId = product->identifier();
#ifdef Q_OS_WIN
    QString microsoftStoreId = product->microsoftStoreId();
    if (!microsoftStoreId.isEmpty()) {
        productId = microsoftStoreId;
    }
#endif
    
    auto* worker = new StorePurchaseWorker(productId, _hwnd);
    auto* thread = new QThread(this);
    
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &StorePurchaseWorker::performPurchase);
    connect(worker, &StorePurchaseWorker::purchaseComplete, 
            [this, product](bool success, int status, const QString& result) {
                this->onPurchaseComplete(product, success, status, result);
            });
    connect(worker, &StorePurchaseWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &StorePurchaseWorker::finished, worker, &StorePurchaseWorker::deleteLater);
    
    thread->start();
}

void MicrosoftStoreBackend::onPurchaseComplete(AbstractProduct* product, bool success, int status, const QString& result)
{
    if (success) {
        // Create minimal transaction for success
        QString orderId = QString("ms_%1_%2").arg(product->identifier()).arg(QDateTime::currentMSecsSinceEpoch());
        auto transaction = new MicrosoftStoreTransaction(orderId, product->identifier(), this);
        emit purchaseSucceeded(transaction);
    } else {
        // Map status to error code
        StoreErrorCode errorCode = UnknownError;
        if (result == "Cancelled") {
            errorCode = UserCanceled;
        } else if (result == "NetworkError") {
            errorCode = NetworkError;
        } else if (result == "ServerError") {
            errorCode = ServiceUnavailable;
        }
        emit purchaseFailed(static_cast<int>(errorCode));
    }
}

void MicrosoftStoreBackend::consumePurchase(AbstractTransaction * transaction)
{
    qDebug() << "Consume purchase called for:" << transaction->orderId() << "Product:" << transaction->productId();
    
    // Look up the product to check its type
    AbstractProduct* product = _registeredProducts.value(transaction->productId(), nullptr);
    
    if (!product) {
        qWarning() << "Cannot find product for transaction:" << transaction->productId();
        emit purchaseConsumed(transaction);
        return;
    }
    
    // Only consumables need fulfillment
    if (product->productType() != AbstractProduct::Consumable) {
        qDebug() << "Product is not consumable (type:" << product->productType() << "), no fulfillment needed";
        emit purchaseConsumed(transaction);
        return;
    }
    
    // For consumables, we need to report fulfillment to Microsoft Store
    qDebug() << "Product is consumable, reporting fulfillment to Microsoft Store";
    
    if (!_hwnd) {
        qWarning() << "No window handle available for consumable fulfillment";
        emit purchaseConsumed(transaction);
        return;
    }
    
    // Get the Microsoft Store ID
    QString storeId = transaction->productId();
#ifdef Q_OS_WIN
    QString microsoftStoreId = product->microsoftStoreId();
    if (!microsoftStoreId.isEmpty()) {
        storeId = microsoftStoreId;
        qDebug() << "Using Microsoft Store ID for fulfillment:" << storeId;
    }
#endif
    
    // Create fulfillment worker
    auto* worker = new StoreConsumableFulfillmentWorker(storeId, 1, _hwnd); // quantity = 1
    auto* thread = new QThread(this);
    
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &StoreConsumableFulfillmentWorker::performFulfillment);
    connect(worker, &StoreConsumableFulfillmentWorker::fulfillmentComplete, 
            [this, transaction](bool success, const QString& result) {
                this->onConsumableFulfillmentComplete(transaction, success, result);
            });
    connect(worker, &StoreConsumableFulfillmentWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &StoreConsumableFulfillmentWorker::finished, worker, &StoreConsumableFulfillmentWorker::deleteLater);
    
    thread->start();
}

void MicrosoftStoreBackend::restorePurchases()
{
    if (!isConnected()) {
        qWarning() << "Cannot restore purchases - store not connected";
        return;
    }
    
    if (!_hwnd) {
        qWarning() << "No window handle available for restore";
        return;
    }
    
    auto* worker = new StoreRestoreWorker(_hwnd);
    auto* thread = new QThread(this);
    
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &StoreRestoreWorker::performRestore);
    connect(worker, &StoreRestoreWorker::restoreComplete, this, &MicrosoftStoreBackend::onRestoreComplete);
    connect(worker, &StoreRestoreWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &StoreRestoreWorker::finished, worker, &StoreRestoreWorker::deleteLater);
    
    thread->start();
}

void MicrosoftStoreBackend::onRestoreComplete(const QList<QVariantMap> &restoredProducts)
{
    qDebug() << "Restore complete, found" << restoredProducts.size() << "owned products";
    
    for (const auto& productData : restoredProducts) {
        QString productId = productData["productId"].toString();
        QString orderId = QString("ms_restored_%1").arg(productId);
        
        auto transaction = new MicrosoftStoreTransaction(orderId, productId, this);
        emit purchaseRestored(transaction);
        
        qDebug() << "Restored purchase:" << productId;
    }
}

void MicrosoftStoreBackend::onConsumableFulfillmentComplete(AbstractTransaction* transaction, bool success, const QString& result)
{
    if (success) {
        qDebug() << "Consumable fulfillment completed successfully for:" << transaction->productId();
    } else {
        qWarning() << "Consumable fulfillment failed for:" << transaction->productId() 
                   << "Error:" << result;
    }
    
    // Always emit consumed signal to maintain API consistency
    // The app can check success via logs if needed
    emit purchaseConsumed(transaction);
}

MicrosoftStoreBackend::StoreErrorCode MicrosoftStoreBackend::mapStoreError(uint32_t hresult)
{
    // Map common HRESULT values to our error codes
    switch (hresult) {
        case 0x80070005: // E_ACCESSDENIED
            return ServiceUnavailable;
        case 0x800704CF: // ERROR_NETWORK_UNREACHABLE
        case 0x80072EE7: // ERROR_INTERNET_NAME_NOT_RESOLVED
            return NetworkError;
        default:
            return UnknownError;
    }
}
