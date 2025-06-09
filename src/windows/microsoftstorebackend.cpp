#include "microsoftstorebackend.h"
#include "microsoftstoreproduct.h"
#include "microsoftstoretransaction.h"

#include <QDebug>
#include <QTimer>
#include <QCoreApplication>

// WinRT includes - only in implementation
#include <winrt/base.h>
#include <winrt/Windows.Services.Store.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Services::Store;

MicrosoftStoreBackend * MicrosoftStoreBackend::s_currentInstance = nullptr;

MicrosoftStoreBackend::MicrosoftStoreBackend(QObject * parent) 
    : AbstractStoreBackend(parent), m_storeContext{nullptr}
{
    qDebug() << "Creating Microsoft Store backend";
    
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    s_currentInstance = this;
    
    startConnection();
}

MicrosoftStoreBackend::~MicrosoftStoreBackend()
{
    if (s_currentInstance == this)
        s_currentInstance = nullptr;
        
    qDebug() << "Destroying Microsoft Store backend";
}

void MicrosoftStoreBackend::startConnection()
{
    try {
        qDebug() << "Initializing Microsoft Store connection";
        m_storeContext = StoreContext::GetDefault();
        
        if (m_storeContext) {
            setConnected(true);
            qDebug() << "Microsoft Store connection established";
        } else {
            qWarning() << "Failed to get Microsoft Store context";
            setConnected(false);
        }
    } catch (hresult_error const& ex) {
        qCritical() << "Microsoft Store initialization failed:" 
                   << QString::fromStdWString(ex.message());
        setConnected(false);
    } catch (...) {
        qCritical() << "Unknown error during Microsoft Store initialization";
        setConnected(false);
    }
}

void MicrosoftStoreBackend::registerProduct(AbstractProduct * product)
{
    if (!isConnected()) {
        qWarning() << "Cannot register product - store not connected";
        product->setStatus(AbstractProduct::Unknown);
        return;
    }

    // Use QTimer to make async from library perspective while keeping WinRT sync
    QTimer::singleShot(0, this, [this, product]() {
        registerProductSync(product);
    });
}

void MicrosoftStoreBackend::registerProductSync(AbstractProduct* product)
{
    try {
        qDebug() << "Registering product:" << product->identifier();
        
        // Create product identifier list
        std::vector<hstring> productIds;
        productIds.push_back(winrt::to_hstring(product->identifier().toStdString()));
        
        // Query store for product information
        auto asyncOp = m_storeContext.GetStoreProductsAsync(productIds);
        
        // Wait synchronously with timeout
        auto result = waitForStoreOperation(asyncOp, 10000); // 10 second timeout
        
        if (result.has_value()) {
            handleProductQueryResult(product, result.value());
        } else {
            qWarning() << "Product registration timed out:" << product->identifier();
            product->setStatus(AbstractProduct::Unknown);
        }
        
    } catch (hresult_error const& ex) {
        qCritical() << "WinRT error during product registration:" 
                   << QString::fromWCharArray(ex.message().c_str())
                   << "HRESULT:" << Qt::hex << ex.code();
        product->setStatus(AbstractProduct::Unknown);
    } catch (...) {
        qCritical() << "Unknown error during product registration for:" << product->identifier();
        product->setStatus(AbstractProduct::Unknown);
    }
}

void MicrosoftStoreBackend::handleProductQueryResult(AbstractProduct* product, 
                                                   const StoreProductQueryResult& result)
{
    if (result.HasError()) {
        qWarning() << "Store query error for product:" << product->identifier();
        product->setStatus(AbstractProduct::Unknown);
        return;
    }
    
    auto products = result.Products();
    auto productId = winrt::to_hstring(product->identifier().toStdString());
    
    if (products.HasKey(productId)) {
        auto storeProduct = products.Lookup(productId);
        auto msProduct = qobject_cast<MicrosoftStoreProduct*>(product);
        
        if (msProduct) {
            // Set Microsoft-specific data
            msProduct->setStoreProduct(storeProduct);
            
            // Set common product data
            product->setTitle(QString::fromStdWString(storeProduct.Title()));
            product->setDescription(QString::fromStdWString(storeProduct.Description()));
            product->setPrice(QString::fromStdWString(storeProduct.Price().FormattedPrice()));
            
            // Map Microsoft product types to library types
            if (storeProduct.ProductKind() == StoreProductKind::Durable) {
                product->setProductType(AbstractProduct::Unlockable);
            } else if (storeProduct.ProductKind() == StoreProductKind::UnmanagedConsumable) {
                product->setProductType(AbstractProduct::Consumable);
            }
            
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
    
    auto msProduct = qobject_cast<MicrosoftStoreProduct*>(product);
    if (!msProduct || !msProduct->storeProduct()) {
        qWarning() << "Invalid Microsoft Store product for purchase";
        emit purchaseFailed(static_cast<int>(UnknownError));
        return;
    }
    
    try {
        qDebug() << "Initiating purchase for:" << product->identifier();
        
        // This launches the Store purchase dialog and returns quickly
        auto asyncOp = m_storeContext.RequestPurchaseAsync(
            winrt::to_hstring(product->identifier().toStdString())
        );
        
        // Wait for purchase result (this should be quick - just launching dialog)
        auto result = waitForStoreOperation(asyncOp, 5000); // 5 second timeout
        
        if (result.has_value()) {
            handlePurchaseResult(result.value(), product->identifier());
        } else {
            qWarning() << "Purchase request timed out:" << product->identifier();
            emit purchaseFailed(static_cast<int>(ServiceUnavailable));
        }
        
    } catch (hresult_error const& ex) {
        qCritical() << "WinRT error during purchase:" 
                   << QString::fromStdWString(ex.message());
        emit purchaseFailed(mapStoreError(ex.code()));
    } catch (...) {
        qCritical() << "Unknown error during purchase";
        emit purchaseFailed(static_cast<int>(UnknownError));
    }
}

void MicrosoftStoreBackend::handlePurchaseResult(const StorePurchaseResult& result, 
                                               const QString& productId)
{
    qDebug() << "Purchase result status:" << static_cast<int>(result.Status());
    
    switch (result.Status()) {
        case StorePurchaseStatus::Succeeded: {
            auto transaction = new MicrosoftStoreTransaction(result, productId, this);
            emit purchaseSucceeded(transaction);
            break;
        }
        case StorePurchaseStatus::UserCanceled:
            emit purchaseFailed(static_cast<int>(UserCanceled));
            break;
        case StorePurchaseStatus::NetworkError:
            emit purchaseFailed(static_cast<int>(NetworkError));
            break;
        case StorePurchaseStatus::ServerError:
            emit purchaseFailed(static_cast<int>(ServiceUnavailable));
            break;
        default:
            emit purchaseFailed(static_cast<int>(UnknownError));
            break;
    }
}

void MicrosoftStoreBackend::consumePurchase(AbstractTransaction * transaction)
{
    // Microsoft Store handles consumption automatically for UnmanagedConsumable and Durable
    // Just emit the consumed signal to maintain API consistency
    qDebug() << "Consuming purchase (no-op for Microsoft Store):" << transaction->orderId();
    
    QTimer::singleShot(0, this, [this, transaction]() {
        emit purchaseConsumed(transaction);
    });
}

void MicrosoftStoreBackend::restorePurchases()
{
    if (!isConnected()) {
        qWarning() << "Cannot restore purchases - store not connected";
        return;
    }
    
    try {
        qDebug() << "Restoring Microsoft Store purchases";
        
        // Query all products the user owns
        auto asyncOp = m_storeContext.GetUserCollectionAsync();
        auto result = waitForStoreOperation(asyncOp, 10000);
        
        if (result.has_value()) {
            auto collection = result.value();
            
            if (collection.HasError()) {
                qWarning() << "Error querying user collection for restore";
                return;
            }
            
            auto products = collection.Products();
            for (auto [productId, storeProduct] : products) {
                QString productIdStr = QString::fromStdWString(productId);
                
                // Generate consistent order ID based on acquisition date
                QString orderId = QString("ms_%1_%2")
                    .arg(productIdStr)
                    .arg(storeProduct.AcquiredDate().time_since_epoch().count());
                
                // Create restored transaction
                auto transaction = new MicrosoftStoreTransaction(orderId, productIdStr, this);
                emit purchaseRestored(transaction);
                
                qDebug() << "Restored purchase:" << productIdStr << "with orderId:" << orderId;
            }
        } else {
            qWarning() << "Purchase restoration timed out";
        }
        
    } catch (hresult_error const& ex) {
        qCritical() << "WinRT error during purchase restoration:" 
                   << QString::fromStdWString(ex.message());
    } catch (...) {
        qCritical() << "Unknown error during purchase restoration";
    }
}

template<typename T>
std::optional<T> waitForStoreOperation(IAsyncOperation<T> operation, int timeoutMs = 10000)
{
    try {
        auto status = operation.wait_for(std::chrono::milliseconds(timeoutMs));
        
        if (status == AsyncStatus::Completed) {
            return operation.GetResults();
        } else {
            qWarning() << "Store operation timed out after" << timeoutMs << "ms";
            // Attempt to cancel (may not always work)
            operation.Cancel();
            return std::nullopt;
        }
    } catch (...) {
        qWarning() << "Exception during store operation wait";
        return std::nullopt;
    }
}

MicrosoftStoreBackend::StoreErrorCode MicrosoftStoreBackend::mapStoreError(int32_t hresult)
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
