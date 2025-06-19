#include "microsoftstorebackend.h"
#include "microsoftstoreproduct.h"
#include "microsoftstoretransaction.h"
#include "windowsstorewrappers.h"

#include <QDebug>
#include <QTimer>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#include <QThread>
#include <optional>
#include <chrono>

// Windows headers for IInitializeWithWindow
#include <Shobjidl.h>
#include <wrl/client.h>

// WinRT includes - only in implementation
#include <winrt/base.h>
#include <winrt/Windows.Services.Store.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Services::Store;
using Microsoft::WRL::ComPtr;

// Helper function to get application window handle
static HWND getApplicationWindowHandle() {
    auto app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (!app) {
        qDebug() << "No QGuiApplication instance found";
        return nullptr;
    }
    
    auto windows = app->topLevelWindows();
    qDebug() << "Found" << windows.size() << "top-level windows";
    if (windows.isEmpty()) {
        qDebug() << "No top-level windows found";
        return nullptr;
    }
    
    // Get the first visible window
    for (auto window : windows) {
        if (window && window->isVisible()) {
            HWND hwnd = reinterpret_cast<HWND>(window->winId());
            qDebug() << "Found application window handle:" << hwnd;
            return hwnd;
        }
    }
    
    // If no visible window, try the first window
    if (!windows.isEmpty()) {
        HWND hwnd = reinterpret_cast<HWND>(windows.first()->winId());
        qDebug() << "Using first available window handle:" << hwnd;
        return hwnd;
    }
    
    qDebug() << "No usable window handle found";
    return nullptr;
}

// Windows Store Manager - handles all WinRT operations
class WindowsStoreManager {
public:
    WindowsStoreManager() : _storeContext{nullptr}, _windowInitialized{false} {}
    
    ~WindowsStoreManager() = default;
    
    bool initialize() {
        try {
            qDebug() << "Getting Store context...";
            
            // Check if running in packaged context
            try {
                auto package = winrt::Windows::ApplicationModel::Package::Current();
                auto packageId = package.Id();
                qDebug() << "Running as packaged app:";
                qDebug() << "  Package Name:" << QString::fromStdWString(packageId.Name().c_str());
                qDebug() << "  Family Name:" << QString::fromStdWString(packageId.FamilyName().c_str());
                qDebug() << "  Publisher:" << QString::fromStdWString(packageId.Publisher().c_str());
                qDebug() << "  Version:" << packageId.Version().Major << "." 
                         << packageId.Version().Minor << "." 
                         << packageId.Version().Build << "."
                         << packageId.Version().Revision;
                qDebug() << "  Architecture:" << static_cast<int>(packageId.Architecture());
                
                // Check install location
                try {
                    auto installLocation = package.InstalledLocation();
                    qDebug() << "  Install Path:" << QString::fromStdWString(installLocation.Path().c_str());
                } catch (...) {
                    qDebug() << "  Could not get install location";
                }
            } catch (const std::exception& e) {
                qDebug() << "NOT running as packaged app - Exception:" << e.what();
                qDebug() << "Store features require app to be installed from .appx/.msix package";
            } catch (...) {
                qDebug() << "NOT running as packaged app - Unknown exception";
            }
            
            _storeContext = StoreContext::GetDefault();
            
            if (_storeContext != nullptr) {
                qDebug() << "Store context obtained successfully";
                
                // Initialize Store context with window handle for Win32 Desktop Bridge apps
                HWND hwnd = getApplicationWindowHandle();
                if (hwnd != nullptr) {
                    qDebug() << "Initializing Store context with window handle...";
                    
                    try {
                        // Query for IInitializeWithWindow interface
                        auto initializeWithWindow = _storeContext.as<IInitializeWithWindow>();
                        
                        if (initializeWithWindow) {
                            // Initialize with the window handle
                            HRESULT hr = initializeWithWindow->Initialize(hwnd);
                            if (SUCCEEDED(hr)) {
                                qDebug() << "Store context successfully initialized with window handle";
                                _windowInitialized = true;
                            } else {
                                qWarning() << "Failed to initialize Store context with window handle. HRESULT:" 
                                          << Qt::hex << hr;
                                _windowInitialized = false;
                            }
                        } else {
                            qWarning() << "Failed to get IInitializeWithWindow interface";
                        }
                    } catch (const std::exception& e) {
                        qWarning() << "Exception during IInitializeWithWindow setup:" << e.what();
                    } catch (...) {
                        qWarning() << "Unknown exception during IInitializeWithWindow setup";
                    }
                } else {
                    qWarning() << "No window handle available - Store context may not work properly";
                    qWarning() << "This is common if Store backend initializes before Qt windows are created";
                    qWarning() << "Store functionality may be limited until window is available";
                    _windowInitialized = false;
                }
                
                // Check user context
                try {
                    auto user = _storeContext.User();
                    if (user != nullptr) {
                        qDebug() << "Store context has user";
                        // Try to get more user info
                        // Note: AuthenticationStatus may not be available in all SDK versions
                    } else {
                        qDebug() << "Store context has no user (running as system or in development mode?)";
                        qDebug() << "This may limit Store functionality. Ensure:";
                        qDebug() << "  1. App is installed from .appx package (not running .exe directly)";
                        qDebug() << "  2. App is associated with Microsoft Store listing";
                        qDebug() << "  3. Running with normal user privileges";
                        qDebug() << "  4. Windows Store services are running";
                    }
                } catch (const std::exception& e) {
                    qDebug() << "Exception getting user from store context:" << e.what();
                } catch (...) {
                    qDebug() << "Unknown exception getting user from store context";
                }
                
                // Try to get Store info
                try {
                    auto storeAppLicense = _storeContext.GetAppLicenseAsync().get();
                    if (storeAppLicense != nullptr) {
                        qDebug() << "App license info:";
                        qDebug() << "  Is active:" << storeAppLicense.IsActive();
                        qDebug() << "  Is trial:" << storeAppLicense.IsTrial();
                        // Note: StoreId property may not be available in all SDK versions
                        try {
                            auto extendedJsonData = storeAppLicense.ExtendedJsonData();
                            if (!extendedJsonData.empty()) {
                                qDebug() << "  Has extended data";
                            }
                        } catch (...) {
                            // ExtendedJsonData might not be available
                        }
                    }
                } catch (const std::exception& e) {
                    qDebug() << "Exception getting app license:" << e.what();
                } catch (...) {
                    qDebug() << "Unknown exception getting app license";
                }
            } else {
                qDebug() << "Failed to get Store context - null pointer returned";
            }
            
            return _storeContext != nullptr;
        } catch (const std::exception& e) {
            qDebug() << "Exception during Store initialization:" << e.what();
            return false;
        } catch (...) {
            qDebug() << "Unknown exception during Store initialization";
            return false;
        }
    }
    
    bool isConnected() const {
        return _storeContext != nullptr;
    }
    
    IAsyncOperation<StoreProductQueryResult> getStoreProductsAsync(const std::vector<hstring>& productIds) {
        std::vector<hstring> productKinds = { L"Durable", L"UnmanagedConsumable" };
        std::vector<hstring> productIdsCopy = productIds; // Create copies for move
        return _storeContext.GetStoreProductsAsync(std::move(productKinds), std::move(productIdsCopy));
    }
    
    IAsyncOperation<StorePurchaseResult> requestPurchaseAsync(const hstring& productId) {
        return _storeContext.RequestPurchaseAsync(productId);
    }
    
    IAsyncOperation<StoreProductQueryResult> getUserCollectionAsync() {
        std::vector<hstring> productKinds = { L"Durable", L"UnmanagedConsumable" };
        return _storeContext.GetUserCollectionAsync(std::move(productKinds));
    }
    
    // Try to initialize with window handle if not already done
    bool ensureWindowInitialization() {
        if (_windowInitialized || _storeContext == nullptr) {
            return _windowInitialized;
        }
        
        HWND hwnd = getApplicationWindowHandle();
        if (hwnd != nullptr) {
            qDebug() << "Retrying Store context window initialization...";
            try {
                auto initializeWithWindow = _storeContext.as<IInitializeWithWindow>();
                
                if (initializeWithWindow) {
                    HRESULT hr = initializeWithWindow->Initialize(hwnd);
                    if (SUCCEEDED(hr)) {
                        qDebug() << "Store context window initialization succeeded on retry";
                        _windowInitialized = true;
                        return true;
                    }
                }
            } catch (...) {
                // Ignore exceptions on retry
            }
        }
        return false;
    }
    
private:
    StoreContext _storeContext;
    bool _windowInitialized;
};


// Helper functions for handling WinRT results
static void handleProductQueryResult(MicrosoftStoreBackend* backend, AbstractProduct* product, 
                                   const StoreProductQueryResult& result);
static void handlePurchaseResult(MicrosoftStoreBackend* backend, const StorePurchaseResult& result, 
                               const QString& productId);

// Helper function template - must be declared before use
template<typename T>
std::optional<T> waitForStoreOperation(IAsyncOperation<T> operation, int timeoutMs = 10000)
{
    try {
        auto status = operation.wait_for(std::chrono::milliseconds(timeoutMs));
        
        if (status == AsyncStatus::Completed) {
            return operation.GetResults();
        } else {
            qWarning() << "Store operation timed out after" << timeoutMs << "ms";
            qWarning() << "  Status:" << static_cast<int>(status);
            qWarning() << "  This often indicates:";
            qWarning() << "    - No network connection to Microsoft Store";
            qWarning() << "    - Store services not running";
            qWarning() << "    - App not properly signed/packaged";
            // Attempt to cancel (may not always work)
            operation.Cancel();
            return std::nullopt;
        }
    } catch (...) {
        qWarning() << "Exception during store operation wait";
        return std::nullopt;
    }
}

MicrosoftStoreBackend * MicrosoftStoreBackend::s_currentInstance = nullptr;

MicrosoftStoreBackend::MicrosoftStoreBackend(QObject * parent) 
    : AbstractStoreBackend(parent), _storeManager(nullptr)
{
    qDebug() << "Creating Microsoft Store backend";
    
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    s_currentInstance = this;
    
    _storeManager = new WindowsStoreManager();
    startConnection();
}

MicrosoftStoreBackend::~MicrosoftStoreBackend()
{
    if (s_currentInstance == this)
        s_currentInstance = nullptr;
        
    delete _storeManager;
    qDebug() << "Destroying Microsoft Store backend";
}

void MicrosoftStoreBackend::startConnection()
{
    try {
        qDebug() << "Initializing Microsoft Store connection";
        
        if (_storeManager && _storeManager->initialize()) {
            setConnected(true);
            qDebug() << "Microsoft Store connection established";
            
            // Debug: List all associated products
            try {
                auto asyncOp = _storeManager->getStoreProductsAsync({});
                auto result = waitForStoreOperation(asyncOp, 10000);
                
                if (result.has_value()) {
                    auto queryResult = result.value();
                    if (!queryResult.ExtendedError()) {
                        auto products = queryResult.Products();
                        qDebug() << "Total add-ons found for this app:" << products.Size();
                        for (auto const& item : products) {
                            qDebug() << "Available add-on:" << QString::fromStdWString(item.Key().c_str())
                                     << "Title:" << QString::fromStdWString(item.Value().Title().c_str());
                        }
                    } else {
                        qDebug() << "Error querying all add-ons:" << Qt::hex << queryResult.ExtendedError().value;
                    }
                }
            } catch (...) {
                qDebug() << "Exception while querying all add-ons";
            }
        } else {
            qWarning() << "Failed to get Microsoft Store context";
            setConnected(false);
        }
    } catch (hresult_error const& ex) {
        qCritical() << "Microsoft Store initialization failed:" 
                   << QString::fromStdWString(ex.message().c_str());
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
        qDebug() << "=== REGISTERING PRODUCT ===" ;
        qDebug() << "  Identifier:" << product->identifier();
        qDebug() << "  Type:" << (product->productType() == AbstractProduct::Consumable ? "Consumable" : "Unlockable");
        
        // Create product identifier list
        std::vector<hstring> productIds;
        productIds.push_back(winrt::to_hstring(product->identifier().toStdString()));
        
        qDebug() << "  Querying Microsoft Store for product...";
        
        // Ensure window initialization if not done yet
        _storeManager->ensureWindowInitialization();
        
        // Query store for product information
        auto asyncOp = _storeManager->getStoreProductsAsync(productIds);
        
        // Wait synchronously with timeout
        auto result = waitForStoreOperation(asyncOp, 10000); // 10 second timeout
        
        if (result.has_value()) {
            qDebug() << "  Query completed successfully";
            handleProductQueryResult(this, product, result.value());
        } else {
            qWarning() << "  Product registration TIMED OUT after 10 seconds:" << product->identifier();
            product->setStatus(AbstractProduct::Unknown);
        }
        
    } catch (hresult_error const& ex) {
        qCritical() << "WinRT error during product registration:" 
                   << QString::fromStdWString(ex.message().c_str())
                   << "HRESULT:" << Qt::hex << ex.code();
        product->setStatus(AbstractProduct::Unknown);
    } catch (...) {
        qCritical() << "Unknown error during product registration for:" << product->identifier();
        product->setStatus(AbstractProduct::Unknown);
    }
}

static void handleProductQueryResult(MicrosoftStoreBackend* backend, AbstractProduct* product, 
                                   const StoreProductQueryResult& result)
{
    if (result.ExtendedError()) {
        qWarning() << "Store query error for product:" << product->identifier()
                   << "Error code:" << Qt::hex << result.ExtendedError().value;
        product->setStatus(AbstractProduct::Unknown);
        return;
    }
    
    auto products = result.Products();
    auto productId = winrt::to_hstring(product->identifier().toStdString());
    
    qDebug() << "  Query returned" << products.Size() << "product(s) for identifier:" << product->identifier();
    
    // List all returned products for debugging
    if (products.Size() > 0) {
        for (auto const& item : products) {
            auto storeProduct = item.Value();
            qDebug() << "  Found product:";
            qDebug() << "    Store ID:" << QString::fromStdWString(item.Key().c_str());
            qDebug() << "    Title:" << QString::fromStdWString(storeProduct.Title().c_str());
            qDebug() << "    Price:" << QString::fromStdWString(storeProduct.Price().FormattedPrice().c_str());
            qDebug() << "    Product Kind:" << QString::fromStdWString(storeProduct.ProductKind().c_str());
        }
    } else {
        qDebug() << "  No products found. Possible reasons:";
        qDebug() << "    1. Product ID mismatch between app and Store listing";
        qDebug() << "    2. Product not published or not available in current market";
        qDebug() << "    3. App not properly associated with Store listing";
    }
    
    if (products.HasKey(productId)) {
        auto storeProduct = products.Lookup(productId);
        auto msProduct = qobject_cast<MicrosoftStoreProduct*>(product);
        
        if (msProduct) {
            // Create wrapper for Microsoft-specific data
            auto * productWrapper = new WindowsStoreProductWrapper(storeProduct);
            msProduct->setStoreProduct(productWrapper);
            
            // Set common product data
            product->setTitle(productWrapper->title());
            product->setDescription(productWrapper->description());
            product->setPrice(productWrapper->price());
            
            // Map Microsoft product types to library types
            auto productKind = productWrapper->productKind();
            if (productKind == "Durable") {
                product->setProductType(AbstractProduct::Unlockable);
            } else if (productKind == "UnmanagedConsumable") {
                product->setProductType(AbstractProduct::Consumable);
            }
            
            product->setStatus(AbstractProduct::Registered);
            emit backend->productRegistered(product);
            
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
    if (!msProduct) {
        qWarning() << "Invalid Microsoft Store product for purchase";
        emit purchaseFailed(static_cast<int>(UnknownError));
        return;
    }
    
    try {
        qDebug() << "Initiating purchase for:" << product->identifier();
        
        // This launches the Store purchase dialog and returns quickly
        auto asyncOp = _storeManager->requestPurchaseAsync(
            winrt::to_hstring(product->identifier().toStdString())
        );
        
        // Wait for purchase result (this should be quick - just launching dialog)
        auto result = waitForStoreOperation(asyncOp, 5000); // 5 second timeout
        
        if (result.has_value()) {
            handlePurchaseResult(this, result.value(), product->identifier());
        } else {
            qWarning() << "Purchase request timed out:" << product->identifier();
            emit purchaseFailed(static_cast<int>(ServiceUnavailable));
        }
        
    } catch (hresult_error const& ex) {
        qCritical() << "WinRT error during purchase:"
                   << QString::fromStdWString(ex.message().c_str());
        emit purchaseFailed(mapStoreError(static_cast<uint32_t>(ex.code())));
    } catch (...) {
        qCritical() << "Unknown error during purchase";
        emit purchaseFailed(static_cast<int>(UnknownError));
    }
}

static void handlePurchaseResult(MicrosoftStoreBackend* backend, const StorePurchaseResult& result, 
                               const QString& productId)
{
    qDebug() << "Purchase result status:" << static_cast<int>(result.Status());
    
    switch (result.Status()) {
        case StorePurchaseStatus::Succeeded: {
            auto * resultWrapper = new WindowsStorePurchaseResultWrapper(result);
            auto transaction = new MicrosoftStoreTransaction(resultWrapper, productId, backend);
            emit backend->purchaseSucceeded(transaction);
            break;
        }
        case StorePurchaseStatus::AlreadyPurchased:
        case StorePurchaseStatus::NotPurchased:
            emit backend->purchaseFailed(static_cast<int>(MicrosoftStoreBackend::StoreErrorCode::UserCanceled));
            break;
        case StorePurchaseStatus::NetworkError:
            emit backend->purchaseFailed(static_cast<int>(MicrosoftStoreBackend::StoreErrorCode::NetworkError));
            break;
        case StorePurchaseStatus::ServerError:
            emit backend->purchaseFailed(static_cast<int>(MicrosoftStoreBackend::StoreErrorCode::ServiceUnavailable));
            break;
        default:
            emit backend->purchaseFailed(static_cast<int>(MicrosoftStoreBackend::StoreErrorCode::UnknownError));
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
        auto asyncOp = _storeManager->getUserCollectionAsync();
        auto result = waitForStoreOperation(asyncOp, 10000);
        
        if (result.has_value()) {
            auto collection = result.value();
            
            if (collection.ExtendedError()) {
                qWarning() << "Error querying user collection for restore";
                return;
            }
            
            auto products = collection.Products();
            for (auto&& pair : products) {
                auto productId = pair.Key();
                auto storeProduct = pair.Value();
                QString productIdStr = QString::fromStdWString(productId.c_str());
                
                // Generate consistent order ID for restored purchase
                QString orderId = QString("ms_restored_%1").arg(productIdStr);
                
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
                   << QString::fromStdWString(ex.message().c_str());
    } catch (...) {
        qCritical() << "Unknown error during purchase restoration";
    }
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
