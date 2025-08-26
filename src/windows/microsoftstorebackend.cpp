#include "microsoftstorebackend.h"
#include "microsoftstoreproduct.h"
#include "microsoftstoreworkers.h"

#include <QDebug>
#include <QSharedPointer>
#include <QThread>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#include <QDateTime>

using namespace winrt::Windows::Services::Store;


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
    setCanMakePurchases(canMakePurchases());
    qDebug() << "Microsoft Store connection established";
    
    // Query all products on startup
    queryAllProducts();
    
    // Note: restorePurchases() will be called after products are queried
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
    connect(worker, &StoreAllProductsWorker::queryComplete, this, &MicrosoftStoreBackend::onAllProductsQueried, Qt::QueuedConnection);
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
    
    // Now that products are available, restore existing purchases
    qDebug() << "Products queried, now calling restorePurchases()";
    restorePurchases();
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
    connect(worker, &StoreProductQueryWorker::queryComplete, this,
            [this, product](bool success, const QVariantMap& productData) {
                this->onProductQueried(product, success, productData);
            }, Qt::QueuedConnection);
    connect(worker, &StoreProductQueryWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &StoreProductQueryWorker::finished, worker, &StoreProductQueryWorker::deleteLater);
    
    thread->start();
}

void MicrosoftStoreBackend::onProductQueried(AbstractProduct * product, bool success, const QVariantMap & productData)
{
    if (success) {
        auto msProduct = qobject_cast<MicrosoftStoreProduct*>(product);
        if (msProduct) {
            // Set product data
            product->setTitle(productData["title"].toString());
            product->setDescription(productData["description"].toString());
            product->setPrice(productData["price"].toString());
            
            // Validate product type matches store configuration
            QString productKind = productData["productKind"].toString();
            AbstractProduct::ProductType storeType;
            if (productKind == "Durable") {
                storeType = AbstractProduct::Unlockable;
            } else if (productKind == "UnmanagedConsumable") {
                storeType = AbstractProduct::Consumable;
            } else {
                qCritical() << "Unknown Microsoft Store product kind:" << productKind << "for product:" << product->identifier();
                product->setStatus(AbstractProduct::Unknown);
                return;
            }

            if (storeType != product->productType()) {
                qCritical() << "Product type mismatch!" << product->identifier() 
                            << "Microsoft Store ID:" << productData["storeId"].toString()
                            << "Expected:" << (product->productType() == AbstractProduct::Consumable ? "Consumable" : "Unlockable")
                            << "Store reports:" << productKind;
                product->setStatus(AbstractProduct::IncorrectProductType);
                return;
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
        // Use a generic service unavailable error code
        constexpr uint32_t SERVICE_UNAVAILABLE = 0x80070005; // E_ACCESSDENIED
        PurchaseError error = PurchaseError::ServiceUnavailable;
        QString message = "Microsoft Store service is unavailable";
        QString productId = product->identifier();
        emit purchaseFailed(productId, static_cast<int>(error), SERVICE_UNAVAILABLE, message);
        return;
    }
    
    if (!_hwnd) {
        qWarning() << "No window handle available for purchase";
        // Use a generic unknown error code
        constexpr uint32_t UNKNOWN_ERROR = 0x80004005; // E_FAIL
        PurchaseError error = PurchaseError::UnknownError;
        QString message = "No window handle available for purchase";
        QString productId = product->identifier();
        emit purchaseFailed(productId, static_cast<int>(error), UNKNOWN_ERROR, message);
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
    connect(worker, &StorePurchaseWorker::purchaseComplete, this,
            [this, product](StorePurchaseStatus status) {
                this->onPurchaseComplete(product, status);
            }, Qt::QueuedConnection);
    connect(worker, &StorePurchaseWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &StorePurchaseWorker::finished, worker, &StorePurchaseWorker::deleteLater);
    
    thread->start();
}

void MicrosoftStoreBackend::onPurchaseComplete(AbstractProduct * product, StorePurchaseStatus status)
{
    qDebug() << "onPurchaseComplete: Backend thread:" << this->thread() << "Current thread:" << QThread::currentThread();
    if (status == StorePurchaseStatus::Succeeded) {
        // Create transaction data for success
        QString orderId = QString("ms_%1_%2").arg(product->identifier()).arg(QDateTime::currentMSecsSinceEpoch());
        auto transaction = QSharedPointer<Transaction>::create(orderId, product->identifier());
        emit purchaseSucceeded(transaction);
    } else {
        // Use the real Windows StorePurchaseStatus as platform code
        PurchaseError error = mapWindowsErrorToPurchaseError(static_cast<uint32_t>(status));
        QString message = getWindowsErrorMessage(static_cast<uint32_t>(status));
        QString productId = product->identifier();
        emit purchaseFailed(productId, static_cast<int>(error), static_cast<uint32_t>(status), message);
    }
}

void MicrosoftStoreBackend::consumePurchase(QSharedPointer<Transaction> transaction)
{
    qDebug() << "Consume transaction called for:" << transaction->orderId() << "Product:" << transaction->productId();
    
    // Look up the product to check its type
    AbstractProduct * product = _registeredProducts.value(transaction->productId(), nullptr);
    
    if (!product) {
        qWarning() << "Cannot find product for transaction:" << transaction.productId;
        emit consumePurchaseFailed(transaction);
        return;
    }
    
    // Only consumables need fulfillment
    if (product->productType() != AbstractProduct::Consumable) {
        qDebug() << "Product is not consumable (type:" << product->productType() << "), no fulfillment needed";
        emit consumePurchaseSucceeded(transaction);
        return;
    }
    
    // For consumables, we need to report fulfillment to Microsoft Store
    qDebug() << "Product is consumable, reporting fulfillment to Microsoft Store";
    qDebug() << "Note: Fulfillment may fail in debug mode - requires proper Store packaging";
    
    if (!_hwnd) {
        qWarning() << "No window handle available for consumable fulfillment";
        emit consumePurchaseFailed(transaction);
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
    
    // Capture transaction data by value for logging
    QString orderId = transaction->orderId();
    QString productId = transaction->productId();
    connect(worker, &StoreConsumableFulfillmentWorker::fulfillmentComplete, this,
            [this, transaction, orderId, productId](bool success, const QString& result) {
                this->onConsumableFulfillmentComplete(orderId, productId, success, result);
                
                // Emit appropriate signal based on fulfillment result
                if (success) {
                    emit consumePurchaseSucceeded(transaction);
                } else {
                    emit consumePurchaseFailed(transaction);
                }
            }, Qt::QueuedConnection);
    connect(worker, &StoreConsumableFulfillmentWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &StoreConsumableFulfillmentWorker::finished, worker, &StoreConsumableFulfillmentWorker::deleteLater);
    
    thread->start();
}

void MicrosoftStoreBackend::restorePurchases()
{
    qDebug() << "restorePurchases() called, products count:" << products().size();
    
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
    connect(worker, &StoreRestoreWorker::restoreComplete, this, &MicrosoftStoreBackend::onRestoreComplete, Qt::QueuedConnection);
    connect(worker, &StoreRestoreWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &StoreRestoreWorker::finished, worker, &StoreRestoreWorker::deleteLater);
    
    thread->start();
}

void MicrosoftStoreBackend::onRestoreComplete(const QList<QVariantMap> &restoredProducts)
{
    qDebug() << "onRestoreComplete: Backend thread:" << this->thread() << "Current thread:" << QThread::currentThread();
    qDebug() << "Restore complete, found" << restoredProducts.size() << "owned products";
    
    for (const auto& productData : restoredProducts) {
        QString msStoreId = productData["productId"].toString();
        QString orderId = QString("ms_restored_%1").arg(msStoreId);
        
        // Find the Qt identifier by searching registered products
        QString qtIdentifier;
        for (AbstractProduct * product : products()) {
            if (product->microsoftStoreId() == msStoreId) {
                qtIdentifier = product->identifier();
                break;
            }
        }
        
        if (!qtIdentifier.isEmpty()) {
            auto transaction = QSharedPointer<Transaction>::create(orderId, qtIdentifier);
            emit purchaseRestored(transaction);
            qDebug() << "Restored purchase: MS Store ID" << msStoreId << "-> Qt ID" << qtIdentifier;
        } else {
            qWarning() << "Could not find Qt product for Microsoft Store ID:" << msStoreId;
        }
    }
}

void MicrosoftStoreBackend::onConsumableFulfillmentComplete(const QString & orderId, const QString & productId, bool success, const QString & result)
{
    if (success) {
        qDebug() << "Consumable fulfillment completed successfully for product:" << productId << "order:" << orderId;
    } else {
        qWarning() << "Consumable fulfillment failed for product:" << productId << "order:" << orderId
                   << "Error:" << result;
        
        // Check if this is a debug mode limitation
        if (result.contains("Server error") || result.contains("0x803f6107")) {
            qWarning() << "Note: Fulfillment errors are common in debug mode. "
                       << "This app needs to be properly packaged and signed for the Microsoft Store "
                       << "for consumable fulfillment to work correctly.";
        }
    }
    
    // Note: consumePurchase success/failure signals are now emitted in the lambda to ensure transaction validity
}

bool MicrosoftStoreBackend::canMakePurchases() const
{
    // For Windows, we can make purchases if we're connected to the store
    return isConnected();
}

AbstractStoreBackend::PurchaseError MicrosoftStoreBackend::mapWindowsErrorToPurchaseError(uint32_t statusCode)
{
    switch (statusCode) {
        case 0: // StorePurchaseStatus::Succeeded
            return PurchaseError::NoError;
        case 1: // StorePurchaseStatus::AlreadyPurchased
            return PurchaseError::AlreadyPurchased;
        case 2: // StorePurchaseStatus::NotPurchased
            return PurchaseError::UserCanceled;
        case 3: // StorePurchaseStatus::NetworkError
            return PurchaseError::NetworkError;
        case 4: // StorePurchaseStatus::ServerError
            return PurchaseError::ServiceUnavailable;
        default:
            return PurchaseError::UnknownError;
    }
}

QString MicrosoftStoreBackend::getWindowsErrorMessage(uint32_t statusCode)
{
    switch (statusCode) {
        case 0: // StorePurchaseStatus::Succeeded
            return "Purchase completed successfully";
        case 1: // StorePurchaseStatus::AlreadyPurchased
            return "User already owns this item";
        case 2: // StorePurchaseStatus::NotPurchased
            return "Purchase was not completed (user canceled or payment failed)";
        case 3: // StorePurchaseStatus::NetworkError
            return "Network connection failed during purchase";
        case 4: // StorePurchaseStatus::ServerError
            return "Microsoft Store server error occurred";
        default:
            return QString("Unknown Windows StorePurchaseStatus: %1").arg(statusCode);
    }
}
