#include "appleappstorebackend.h"
#include "appleappstoreproduct.h"
#include "appleappstoretransaction.h"

#include <QDebug>
#include <QJsonObject>
#include <QThread>
#include <QCoreApplication>

#import <StoreKit/StoreKit.h>

AppleAppStoreBackend* AppleAppStoreBackend::s_currentInstance = nullptr;

@interface InAppPurchaseManager : NSObject <SKProductsRequestDelegate, SKPaymentTransactionObserver>
{
    NSMutableArray<SKPaymentTransaction *> *pendingTransactions;
}

-(id)init;
-(void)requestProductData:(NSString *)identifier;

@end

@implementation InAppPurchaseManager

-(id)init {
    if (self = [super init]) {
        pendingTransactions = [[NSMutableArray<SKPaymentTransaction *> alloc] init];
        [[SKPaymentQueue defaultQueue] addTransactionObserver:self];
    }
    return self;
}

-(void)dealloc
{
    [[SKPaymentQueue defaultQueue] removeTransactionObserver:self];
}

-(void)requestProductData:(NSString *)identifier
{
    NSSet<NSString *> * productId = [NSSet<NSString *> setWithObject:identifier];
    SKProductsRequest * productsRequest = [[SKProductsRequest alloc] initWithProductIdentifiers:productId];
    productsRequest.delegate = self;
    [productsRequest start];
}

//SKProductsRequestDelegate
-(void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response
{
    AppleAppStoreBackend * backend = AppleAppStoreBackend::s_currentInstance;
    if (!backend) {
        qCritical() << "Apple Store product callback received but backend instance is null";
        return;
    }

    NSArray<SKProduct *> * skProducts = response.products;
    SKProduct * skProduct = [skProducts count] == 1 ? [skProducts firstObject] : nil;

    if (skProduct == nil) {
        //Invalid product ID
        NSString * invalidId = [response.invalidProductIdentifiers firstObject];
        if (backend->product(QString::fromNSString(invalidId)))
            backend->product(QString::fromNSString(invalidId))->setStatus(AbstractProduct::Unknown);
    } else {
        //Valid product query
        AppleAppStoreProduct * product = reinterpret_cast<AppleAppStoreProduct*>( backend->product(QString::fromNSString(skProduct.productIdentifier)) );

        if (product) {
            // formatting price string
            NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
            [numberFormatter setFormatterBehavior:NSNumberFormatterBehavior10_4];
            [numberFormatter setNumberStyle:NSNumberFormatterCurrencyStyle];
            [numberFormatter setLocale:skProduct.priceLocale];
            NSString * localizedPrice = [numberFormatter stringFromNumber:skProduct.price];

            product->setNativeProduct(skProduct);
            product->setDescription(QString::fromNSString(skProduct.localizedDescription));
            product->setPrice(QString::fromNSString(localizedPrice));
            product->setTitle(QString::fromNSString(skProduct.localizedTitle));
            product->setStatus(AbstractProduct::Registered);

            QMetaObject::invokeMethod(backend, "productRegistered", Qt::AutoConnection, Q_ARG(AbstractProduct*, product));
        } else {
        }
    }
}

//SKPaymentTransactionObserver
- (void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray<SKPaymentTransaction *> *)transactions
{
    AppleAppStoreBackend* backend = AppleAppStoreBackend::s_currentInstance;
    if (!backend) {
        qCritical() << "Apple Store transaction callback received but backend instance is null - transactions will be lost";
        return;
    }

    Q_UNUSED(queue);

    for (SKPaymentTransaction * transaction in transactions) {
        AppleAppStoreTransaction * ta = new AppleAppStoreTransaction(transaction, backend);

        switch (static_cast<AppleAppStoreTransaction::AppleAppStoreTransactionState>(transaction.transactionState)) {
        case AppleAppStoreTransaction::Purchasing:
            //unhandled
            break;
        case AppleAppStoreTransaction::Purchased:
            QMetaObject::invokeMethod(backend, "purchaseSucceeded", Qt::AutoConnection, Q_ARG(AbstractTransaction*, ta));
            break;
        case AppleAppStoreTransaction::Failed:
            {
                int errorCode = transaction.error.code;
                AbstractStoreBackend::PurchaseError error = AppleAppStoreBackend::mapStoreKitErrorToPurchaseError(errorCode);
                QString message = AppleAppStoreBackend::getStoreKitErrorMessage(errorCode);
                QMetaObject::invokeMethod(backend, "purchaseFailed", Qt::AutoConnection, 
                    Q_ARG(AbstractStoreBackend::PurchaseError, error), 
                    Q_ARG(int, errorCode), 
                    Q_ARG(QString, message));
            }
            break;
        case AppleAppStoreTransaction::Restored:
            QMetaObject::invokeMethod(backend, "purchaseRestored", Qt::AutoConnection, Q_ARG(AbstractTransaction*, ta));
            break;
        case AppleAppStoreTransaction::Deferred:
            //unhandled
            break;
        }
    }
}

@end

AppleAppStoreBackend::AppleAppStoreBackend(QObject * parent) : AbstractStoreBackend(parent)
{
    this->startConnection();

    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    s_currentInstance = this;
}

AppleAppStoreBackend::~AppleAppStoreBackend()
{
    if (s_currentInstance == this)
        s_currentInstance = nullptr;
}

void AppleAppStoreBackend::startConnection()
{
    _iapManager = [[InAppPurchaseManager alloc] init];
    setConnected(_iapManager != nullptr);
}

void AppleAppStoreBackend::registerProduct(AbstractProduct * product)
{
    [_iapManager requestProductData:(product->identifier().toNSString())];
}

void AppleAppStoreBackend::purchaseProduct(AbstractProduct * product)
{
    SKProduct * skProduct = reinterpret_cast<AppleAppStoreProduct*>(product)->nativeProduct();

    SKPayment * payment = [SKPayment paymentWithProduct:skProduct];
    [[SKPaymentQueue defaultQueue] addPayment:payment];
}

void AppleAppStoreBackend::consumePurchase(AbstractTransaction * transaction)
{
    [[SKPaymentQueue defaultQueue] finishTransaction:reinterpret_cast<AppleAppStoreTransaction *>(transaction)->nativeTransaction()];
    emit consumePurchaseSucceeded(transaction);
}

AbstractStoreBackend::PurchaseError AppleAppStoreBackend::mapStoreKitErrorToPurchaseError(int errorCode)
{
    switch (errorCode) {
        case SKErrorPaymentCancelled:
            return PurchaseError::UserCanceled;
        case SKErrorPaymentNotAllowed:
            return PurchaseError::NotAllowed;
        case SKErrorPaymentInvalid:
            return PurchaseError::PaymentInvalid;
        case SKErrorClientInvalid:
        case SKErrorStoreProductNotAvailable:
            return PurchaseError::ItemUnavailable;
        case SKErrorCloudServiceNetworkConnectionFailed:
        case SKErrorCloudServiceRevoked:
            return PurchaseError::NetworkError;
        case SKErrorUnknown:
        default:
            return PurchaseError::UnknownError;
    }
}

QString AppleAppStoreBackend::getStoreKitErrorMessage(int errorCode)
{
    switch (errorCode) {
        case SKErrorPaymentCancelled:
            return "User canceled the payment request";
        case SKErrorPaymentNotAllowed:
            return "This device is not allowed to make the payment";
        case SKErrorPaymentInvalid:
            return "One of the payment parameters was not recognized by the App Store";
        case SKErrorClientInvalid:
            return "The client is not allowed to issue the request";
        case SKErrorStoreProductNotAvailable:
            return "The requested product is not available in the store";
        case SKErrorCloudServiceNetworkConnectionFailed:
            return "The device could not connect to the network";
        case SKErrorCloudServiceRevoked:
            return "The user has revoked permission to use this cloud service";
        case SKErrorUnknown:
            return "An unknown error occurred";
        default:
            return QString("Unknown StoreKit error code: %1").arg(errorCode);
    }
}
