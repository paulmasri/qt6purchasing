#include "microsoftstoreworkers.h"
#include <QDebug>
#include <QVariantMap>

// Windows headers
#include <Shobjidl.h>
#include <wrl/client.h>

// WinRT includes
#include <winrt/base.h>
#include <winrt/Windows.Services.Store.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Services::Store;

void StoreProductQueryWorker::performQuery()
{
    try {
        // Create StoreContext in worker thread
        auto storeContext = StoreContext::GetDefault();

        // Initialize with window handle
        auto initWindow = storeContext.as<IInitializeWithWindow>();
        initWindow->Initialize(_hwnd);

        // Use Microsoft Store ID if available, otherwise use identifier
        QString storeId = _productId;

#ifdef Q_OS_WIN
        // Check if this looks like a Store ID (has dots) vs a simple identifier
        if (!_productId.contains('.')) {
            // This might be a simple identifier, we should use microsoftStoreId if available
            // For now, we'll just use what we have
            qDebug() << "Using product identifier as Store ID:" << _productId;
        }
#endif

        // Query for specific product
        std::vector<winrt::hstring> productKinds = {L"Durable", L"UnmanagedConsumable"};
        std::vector<winrt::hstring> productIds = {winrt::hstring(storeId.toStdWString())};

        auto result = storeContext.GetStoreProductsAsync(std::move(productKinds), std::move(productIds)).get();

        if (!result.ExtendedError()) {
            auto products = result.Products();

            if (products.Size() > 0) {
                // Get the first (and should be only) product
                auto storeProduct = products.First().Current().Value();

                QVariantMap productData;
                productData["storeId"] = QString::fromWCharArray(storeProduct.StoreId().c_str());
                productData["title"] = QString::fromWCharArray(storeProduct.Title().c_str());
                productData["description"] = QString::fromWCharArray(storeProduct.Description().c_str());
                productData["price"] = QString::fromWCharArray(storeProduct.Price().FormattedPrice().c_str());
                productData["productKind"] = QString::fromWCharArray(storeProduct.ProductKind().c_str());
                productData["isInUserCollection"] = storeProduct.IsInUserCollection();

                emit querySucceeded(productData);
            } else {
                qDebug() << "Product not found in store:" << _productId;
                emit queryFailed(0, "Product not found in store");
            }
        } else {
            uint32_t hresult = result.ExtendedError().value;
            qWarning() << "Store query error for product" << _productId << "- HRESULT:" << Qt::hex << Qt::showbase
                       << hresult;
            emit queryFailed(hresult, QString("Store API error: 0x%1").arg(hresult, 0, 16));
        }
    } catch (const winrt::hresult_error &e) {
        uint32_t hresult = static_cast<uint32_t>(e.code().value);
        QString message = QString::fromWCharArray(e.message().c_str());
        qWarning() << "Exception in product query:" << message << "HRESULT:" << Qt::hex << Qt::showbase << hresult;
        emit queryFailed(hresult, message);
    } catch (...) {
        qWarning() << "Unknown exception in product query";
        emit queryFailed(0x80004005, "Unknown exception");
    }

    emit finished();
}

void StorePurchaseWorker::performPurchase()
{
    try {
        // Create StoreContext in worker thread
        auto storeContext = StoreContext::GetDefault();

        // Initialize with window handle
        auto initWindow = storeContext.as<IInitializeWithWindow>();
        initWindow->Initialize(_hwnd);

        // Synchronous purchase call
        auto result = storeContext.RequestPurchaseAsync(winrt::hstring(_productId.toStdWString())).get();

        emit purchaseComplete(result.Status());
    } catch (const winrt::hresult_error &e) {
        qWarning() << "Purchase HRESULT error:" << QString::fromWCharArray(e.message().c_str());
        emit purchaseComplete(StorePurchaseStatus::ServerError);
    } catch (...) {
        qWarning() << "Unknown exception during purchase";
        emit purchaseComplete(StorePurchaseStatus::ServerError);
    }

    emit finished();
}

void StoreRestoreWorker::performRestore()
{
    try {
        // Create StoreContext in worker thread
        auto storeContext = StoreContext::GetDefault();

        // Initialize with window handle
        auto initWindow = storeContext.as<IInitializeWithWindow>();
        initWindow->Initialize(_hwnd);

        // Get user's product collection
        std::vector<winrt::hstring> productKinds = {L"Durable", L"UnmanagedConsumable"};
        auto result = storeContext.GetUserCollectionAsync(std::move(productKinds)).get();

        QList<QVariantMap> restoredProducts;

        if (!result.ExtendedError()) {
            auto products = result.Products();

            for (auto const &item : products) {
                auto storeProduct = item.Value();

                QVariantMap productData;
                productData["storeId"] = QString::fromWCharArray(storeProduct.StoreId().c_str());
                productData["productId"] = QString::fromWCharArray(item.Key().c_str());
                productData["title"] = QString::fromWCharArray(storeProduct.Title().c_str());
                productData["productKind"] = QString::fromWCharArray(storeProduct.ProductKind().c_str());

                restoredProducts.append(productData);
            }
            emit restoreSucceeded(restoredProducts);
        } else {
            uint32_t hresult = result.ExtendedError().value;
            qWarning() << "Windows Store restore error - HRESULT:" << Qt::hex << Qt::showbase << hresult;
            emit restoreFailed(hresult, QString("Windows Store API error: 0x%1").arg(hresult, 0, 16));
        }
    } catch (const winrt::hresult_error &e) {
        uint32_t errorCode = static_cast<uint32_t>(e.code().value);
        QString message = QString::fromWCharArray(e.message().c_str());
        qWarning() << "Exception in restore:" << message << "Code:" << Qt::hex << errorCode;
        emit restoreFailed(errorCode, message);
    } catch (...) {
        qWarning() << "Unknown exception in restore";
        emit restoreFailed(0xFFFFFFFF, "Unknown exception during restore");
    }

    emit finished();
}

void StoreAllProductsWorker::performQuery()
{
    try {
        // Create StoreContext in worker thread
        auto storeContext = StoreContext::GetDefault();

        // Initialize with window handle
        auto initWindow = storeContext.as<IInitializeWithWindow>();
        initWindow->Initialize(_hwnd);

        // Query all associated products
        std::vector<winrt::hstring> productKinds = {L"Durable", L"UnmanagedConsumable"};
        auto result = storeContext.GetAssociatedStoreProductsAsync(std::move(productKinds)).get();

        QList<QVariantMap> products;

        if (!result.ExtendedError()) {
            auto storeProducts = result.Products();

            for (auto const &item : storeProducts) {
                auto storeProduct = item.Value();

                QVariantMap productData;
                productData["storeId"] = QString::fromWCharArray(storeProduct.StoreId().c_str());
                productData["productId"] = QString::fromWCharArray(item.Key().c_str());
                productData["title"] = QString::fromWCharArray(storeProduct.Title().c_str());
                productData["description"] = QString::fromWCharArray(storeProduct.Description().c_str());
                productData["price"] = QString::fromWCharArray(storeProduct.Price().FormattedPrice().c_str());
                productData["productKind"] = QString::fromWCharArray(storeProduct.ProductKind().c_str());

                products.append(productData);
            }

            qDebug() << "Found" << products.size() << "associated products";
            emit querySucceeded(products);
        } else {
            uint32_t hresult = result.ExtendedError().value;
            qWarning() << "Error querying all products - HRESULT:" << Qt::hex << Qt::showbase << hresult;
            emit queryFailed(hresult, QString("Store API error: 0x%1").arg(hresult, 0, 16));
        }
    } catch (const winrt::hresult_error &e) {
        uint32_t hresult = static_cast<uint32_t>(e.code().value);
        QString message = QString::fromWCharArray(e.message().c_str());
        qWarning() << "Exception querying all products:" << message << "HRESULT:" << Qt::hex << Qt::showbase << hresult;
        emit queryFailed(hresult, message);
    } catch (...) {
        qWarning() << "Unknown exception querying all products";
        emit queryFailed(0x80004005, "Unknown exception");
    }

    emit finished();
}

void StoreConsumableFulfillmentWorker::performFulfillment()
{
    try {
        // Create StoreContext in worker thread
        auto storeContext = StoreContext::GetDefault();

        // Initialize with window handle
        auto initWindow = storeContext.as<IInitializeWithWindow>();
        initWindow->Initialize(_hwnd);

        // Generate unique tracking ID
        winrt::guid trackingGuid = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

        qDebug() << "Reporting consumable fulfillment for Store ID:" << _storeId << "Quantity:" << _quantity
                 << "Tracking ID:" << QString::fromWCharArray(winrt::to_hstring(trackingGuid).c_str());

        // Report fulfillment
        auto result =
            storeContext
                .ReportConsumableFulfillmentAsync(winrt::hstring(_storeId.toStdWString()), _quantity, trackingGuid)
                .get();

        switch (result.Status()) {
        case StoreConsumableStatus::Succeeded:
            qDebug() << "Consumable fulfillment succeeded, balance:" << result.BalanceRemaining();
            emit fulfillmentSucceeded();
            break;
        case StoreConsumableStatus::InsufficentQuantity:
            qWarning() << "Consumable fulfillment failed: Insufficient quantity";
            emit fulfillmentFailed(
                static_cast<uint32_t>(StoreConsumableStatus::InsufficentQuantity), "Insufficient quantity"
            );
            break;
        case StoreConsumableStatus::NetworkError:
            qWarning() << "Consumable fulfillment failed: Network error";
            emit fulfillmentFailed(static_cast<uint32_t>(StoreConsumableStatus::NetworkError), "Network error");
            break;
        case StoreConsumableStatus::ServerError:
            qWarning() << "Consumable fulfillment failed: Server error";
            emit fulfillmentFailed(static_cast<uint32_t>(StoreConsumableStatus::ServerError), "Server error");
            break;
        default:
            qWarning() << "Consumable fulfillment failed: Unknown error";
            emit fulfillmentFailed(0x80004005, "Unknown fulfillment status");
        }
    } catch (const winrt::hresult_error &e) {
        uint32_t hresult = static_cast<uint32_t>(e.code().value);
        QString message = QString::fromWCharArray(e.message().c_str());
        qWarning() << "Exception in consumable fulfillment:" << message << "HRESULT:" << Qt::hex << Qt::showbase
                   << hresult;
        emit fulfillmentFailed(hresult, message);
    } catch (...) {
        qWarning() << "Unknown exception in consumable fulfillment";
        emit fulfillmentFailed(0x80004005, "Unknown exception");
    }

    emit finished();
}
