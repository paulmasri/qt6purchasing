#include "appleappstorebackend.h"
#include "appleappstoreproduct.h"

#include <QDebug>
#include <QJsonObject>
#include <QThread>
#include <QCoreApplication>
#include <QTimer>

#import <StoreKit/StoreKit.h>

namespace AppleAppStoreTransactionState {
    enum State {
        Purchasing,
        Purchased,
        Failed,
        Restored,
        Deferred
    };
}

// Helper functions for AppleAppStoreTransaction creation
static Transaction transactionFromSKTransaction(SKPaymentTransaction * skTransaction)
{
    Transaction transaction;
    // For pending transactions, transactionIdentifier is nil - use empty string for now
    // The real identifier will be available when the transaction completes
    transaction.orderId = skTransaction.transactionIdentifier ? 
        QString::fromNSString(skTransaction.transactionIdentifier) : QString();
    transaction.productId = QString::fromNSString(skTransaction.payment.productIdentifier);
    return transaction;
}

// Helper functions for error mapping
static AbstractStoreBackend::PurchaseError mapStoreKitErrorToPurchaseError(int errorCode)
{
    switch (errorCode) {
        case SKErrorPaymentCancelled:
            return AbstractStoreBackend::PurchaseError::UserCanceled;
        case SKErrorPaymentNotAllowed:
            return AbstractStoreBackend::PurchaseError::NotAllowed;
        case SKErrorPaymentInvalid:
        case SKErrorClientInvalid: // See https://stackoverflow.com/a/10975530/457584
        case SKErrorInvalidSignature: // Cryptographic signature against promo code is invalid
            return AbstractStoreBackend::PurchaseError::PaymentInvalid;
        case SKErrorStoreProductNotAvailable:
            return AbstractStoreBackend::PurchaseError::ItemUnavailable;
        case SKErrorCloudServiceNetworkConnectionFailed:
        case SKErrorCloudServiceRevoked:
            return AbstractStoreBackend::PurchaseError::NetworkError;
        case SKErrorUnknown:
        default:
            return AbstractStoreBackend::PurchaseError::UnknownError;
    }
}

static QString getStoreKitErrorMessage(int errorCode)
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

AppleAppStoreBackend * AppleAppStoreBackend::s_currentInstance = nullptr;

// Observer that handles all transactions for the app lifetime
@interface TransactionObserver : NSObject <SKPaymentTransactionObserver>
{
    NSMutableArray<SKPaymentTransaction *> *queuedTransactions;
}

@property (class, readonly) TransactionObserver *shared;
-(NSArray<SKPaymentTransaction *> *)getQueuedTransactions;
-(void)clearQueuedTransactions;
-(void)processTransactions:(NSArray<SKPaymentTransaction *> *)skTransactions;
-(void)processQueuedTransactions;

@end

@implementation TransactionObserver

+ (TransactionObserver *)shared {
    static TransactionObserver *sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
    });
    return sharedInstance;
}

-(id)init {
    if (self = [super init]) {
        queuedTransactions = [[NSMutableArray<SKPaymentTransaction *> alloc] init];
    }
    return self;
}

-(NSArray<SKPaymentTransaction *> *)getQueuedTransactions {
    return [queuedTransactions copy];
}

-(void)clearQueuedTransactions {
    [queuedTransactions removeAllObjects];
}

-(void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray<SKPaymentTransaction *> *)transactions {
    AppleAppStoreBackend* backend = AppleAppStoreBackend::s_currentInstance;
    qDebug() << "TransactionObserver received" << transactions.count << "transactions";

    if (!backend) {
        qDebug() << "No backend instance available - queueing transactions";
        [queuedTransactions addObjectsFromArray:transactions];
        return;
    }

    if (!backend->processingEnabled()) {
        qDebug() << "Processing not enabled - queueing transactions";
        [queuedTransactions addObjectsFromArray:transactions];
        return;
    }

    // Process transactions immediately
    qDebug() << "Processing transactions immediately";
    [self processTransactions:transactions];
}

-(void)processTransactions:(NSArray<SKPaymentTransaction *> *)skTransactions {
    AppleAppStoreBackend* backend = AppleAppStoreBackend::s_currentInstance;

    qDebug() << "iOS: processing" << skTransactions.count << "transactions";
    for (SKPaymentTransaction * skTransaction in skTransactions) {
        qDebug() << "iOS: Processing transaction ID:" << QString::fromNSString(skTransaction.transactionIdentifier) << "state:" << skTransaction.transactionState << "product:" << QString::fromNSString(skTransaction.payment.productIdentifier);
        switch (static_cast<AppleAppStoreTransactionState::State>(skTransaction.transactionState)) {
        case AppleAppStoreTransactionState::Purchasing:
            {
                qDebug() << "iOS: Transaction moving to Purchasing state (user presented with iOS payment dialog)";
            }
            break;
        case AppleAppStoreTransactionState::Purchased:
            {
                auto transaction = transactionFromSKTransaction(skTransaction);
                QMetaObject::invokeMethod(backend, "purchaseSucceeded", Qt::AutoConnection, Q_ARG(Transaction, transaction));
            }
            break;
        case AppleAppStoreTransactionState::Failed:
            {
                // Extract product ID from the transaction
                QString productId = QString::fromNSString(skTransaction.payment.productIdentifier);
                int errorCode = skTransaction.error.code;
                AbstractStoreBackend::PurchaseError error = mapStoreKitErrorToPurchaseError(errorCode);
                QString message = getStoreKitErrorMessage(errorCode);
                QMetaObject::invokeMethod(backend, "purchaseFailed", Qt::AutoConnection,
                    Q_ARG(QString, productId),
                    Q_ARG(int, static_cast<int>(error)),
                    Q_ARG(int, errorCode),
                    Q_ARG(QString, message));
            }
            break;
        case AppleAppStoreTransactionState::Restored:
            {
                auto transaction = transactionFromSKTransaction(skTransaction);
                QMetaObject::invokeMethod(backend, "purchaseRestored", Qt::AutoConnection, Q_ARG(Transaction, transaction));
            }
            break;
        case AppleAppStoreTransactionState::Deferred:
            {
                auto transaction = transactionFromSKTransaction(skTransaction);
                QMetaObject::invokeMethod(backend, "purchasePending", Qt::AutoConnection, Q_ARG(Transaction, transaction));
            }
            break;
        }
    }
}

-(void)processQueuedTransactions {
    AppleAppStoreBackend* backend = AppleAppStoreBackend::s_currentInstance;
    if (!backend) {
        qDebug() << "TransactionObserver: No backend available for processing queued transactions";
        return;
    }

    if (queuedTransactions.count > 0) {
        qDebug() << "TransactionObserver: Processing" << queuedTransactions.count << "queued transactions";
        [self processTransactions:queuedTransactions];
        [queuedTransactions removeAllObjects];
    } else {
        qDebug() << "TransactionObserver: No queued transactions to process";
    }
}

-(void)paymentQueue:(SKPaymentQueue *)queue restoreCompletedTransactionsFailedWithError:(NSError *)error {
    AppleAppStoreBackend* backend = AppleAppStoreBackend::s_currentInstance;
    if (!backend) {
        qWarning() << "TransactionObserver: Restore failed but no backend available";
        return;
    }

    qDebug() << "iOS: Restore purchases failed with error code:" << error.code;

    int errorCode = error.code;
    AbstractStoreBackend::PurchaseError mappedError = mapStoreKitErrorToPurchaseError(errorCode);
    QString message = getStoreKitErrorMessage(errorCode);

    QMetaObject::invokeMethod(backend, "restorePurchasesFailed", Qt::AutoConnection,
        Q_ARG(int, static_cast<int>(mappedError)),
        Q_ARG(int, errorCode),
        Q_ARG(QString, message));
}

-(void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue {
    AppleAppStoreBackend* backend = AppleAppStoreBackend::s_currentInstance;
    if (!backend) {
        qWarning() << "TransactionObserver: Restore completed but no backend available";
        return;
    }

    int count = backend->restoredPurchasesCount();
    qDebug() << "iOS: Restore purchases completed successfully. Count:" << count;

    QMetaObject::invokeMethod(backend, "restorePurchasesSucceeded", Qt::AutoConnection,
        Q_ARG(int, count));
}

@end

@interface InAppPurchaseManager : NSObject <SKProductsRequestDelegate>

-(id)init;
-(void)requestProductData:(NSString *)identifier;

@end

@implementation InAppPurchaseManager

-(id)init {
    if (self = [super init]) {
        qDebug() << "InAppPurchaseManager: Initialized for product queries only";
    }
    return self;
}

-(void)dealloc
{
    // No transaction observer to remove
}


-(void)requestProductData:(NSString *)identifier
{
    qDebug() << "StoreKit: Requesting product data for identifier:" << QString::fromNSString(identifier);

    NSSet<NSString *> * productId = [NSSet<NSString *> setWithObject:identifier];
    SKProductsRequest * productsRequest = [[SKProductsRequest alloc] initWithProductIdentifiers:productId];
    productsRequest.delegate = self;

    // Check if we're using StoreKit testing
    #if TARGET_OS_SIMULATOR
        qDebug() << "StoreKit: Running in iOS Simulator";
    #else
        qDebug() << "StoreKit: Running on physical device";
    #endif

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

    qDebug() << "StoreKit: Received product response; num valid products:" << response.products.count <<
                "; num invalid product identifiers:" << response.invalidProductIdentifiers.count;
    if (response.invalidProductIdentifiers.count > 0) {
        for (NSString *invalidId in response.invalidProductIdentifiers) {
            qDebug() << "StoreKit: Invalid product ID:" << QString::fromNSString(invalidId);
        }
    }
    if (response.products.count > 0) {
        for (SKProduct *product in response.products) {
            qDebug() << "StoreKit: Valid product found:" << QString::fromNSString(product.productIdentifier);
        }
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

@end

AppleAppStoreBackend::AppleAppStoreBackend(QObject * parent) : AbstractStoreBackend(parent)
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    s_currentInstance = this;

    // Track restored purchases count for restoration completion reporting
    connect(this, &AppleAppStoreBackend::purchaseRestored, this, [this](Transaction transaction){
        _restoredPurchasesCount++;
    });

    this->startConnection();
}

AppleAppStoreBackend::~AppleAppStoreBackend()
{
    if (s_currentInstance == this)
        s_currentInstance = nullptr;
}

// Static version for early initialization from main.cpp
void AppleAppStoreBackend::initializeEarlyTransactionQueue()
{
    qDebug() << "iOS IAP: Adding transaction observer early to catch pending transactions";
    [[SKPaymentQueue defaultQueue] addTransactionObserver:[TransactionObserver shared]];
}

void AppleAppStoreBackend::startConnection()
{
    _iapManager = [[InAppPurchaseManager alloc] init];
    setConnected(_iapManager != nullptr);
    setCanMakePurchases(canMakePurchases());
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

void AppleAppStoreBackend::consumePurchase(Transaction transaction)
{
    qDebug() << "iOS: consumePurchase called for" << transaction.orderId;
    
    // Look up the SKPaymentTransaction using orderId (transactionIdentifier)
    NSString *identifier = transaction.orderId.toNSString();
    BOOL found = NO;
    
    for (SKPaymentTransaction *skTransaction in [[SKPaymentQueue defaultQueue] transactions]) {
        if ([skTransaction.transactionIdentifier isEqualToString:identifier]) {
            [[SKPaymentQueue defaultQueue] finishTransaction:skTransaction];
            emit consumePurchaseSucceeded(transaction);
            found = YES;
            break;
        }
    }
    
    if (!found) {
        qWarning() << "iOS: Transaction not found in queue for orderId:" << transaction.orderId;
        emit consumePurchaseFailed(transaction);
    }
}

void AppleAppStoreBackend::restorePurchasesImpl()
{
    qDebug() << "iOS restorePurchasesImpl() called - triggering SKPaymentQueue.restoreCompletedTransactions";
    _restoredPurchasesCount = 0;
    [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
}

bool AppleAppStoreBackend::canMakePurchases() const
{
    return [SKPaymentQueue canMakePayments];
}

void AppleAppStoreBackend::enableProcessing()
{
    if (processingEnabled())
        return;

    AbstractStoreBackend::enableProcessing();

    qDebug() << "iOS: Processing enabled - processing queued transactions";
    [[TransactionObserver shared] processQueuedTransactions];
}
