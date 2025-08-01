# qt6purchasing
<a name="readme-top"></a>


<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/moritzstoetter/qt6purchasing">
    <img src="logo.png" alt="Logo" width="500" height="auto">
  </a>

  <h3 align="center">Qt6/QML In-App-Purchasing</h3>

  <p align="center">
    Bringing In-App-Purchasing to Qt6
    <br />
    <br />
    <a href="https://github.com/moritzstoetter/qt6purchasing/issues">Report Bug</a>
    ·
    <a href="https://github.com/moritzstoetter/qt6purchasing/issues">Request Feature</a>
  </p>
</div>



<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
  </ol>
</details>



<!-- ABOUT THE PROJECT -->
## About The Project

This project provides a library wrapper, that provides an abstraction of the native app store libraries, and makes the necessary functionalities available in Qt6 and QML. Compatible with Apple App Store, Google Play Store, and Microsoft Store.

Here's why:
* In-App-Purchasing might be an important way of monetizing your Qt/QML mobile app.
* QtPurchasing didn't make it to Qt6.


<p align="right">(<a href="#readme-top">back to top</a>)</p>



### Built With

[![Qt6][qt-badge]][qt-url]<br>
[![Cpp][cpp-badge]][cpp-url]<br>
[![Objective-C][objc-badge]][objc-url]<br>
[![Java][java-badge]][java-url]<br>
[![Cmake][cmake-badge]][cmake-url]<br>

[qt-badge]: https://img.shields.io/badge/Qt/QML-6.8-20232A?style=for-the-badge&logo=qt&logoColor=41cd52
[qt-url]: https://www.qt.io/product/qt6
[cpp-badge]: https://img.shields.io/badge/C++-20-20232A?style=for-the-badge&logo=cplusplus&logoColor=blue
[cpp-url]: https://isocpp.org/
[objc-badge]: https://img.shields.io/badge/Objective--C-20232A?style=for-the-badge&logo=apple&logoColor=gold
[objc-url]: https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/ProgrammingWithObjectiveC/Introduction/Introduction.html
[java-badge]: https://img.shields.io/badge/Java-20232A?style=for-the-badge
[java-url]: https://java.com
[cmake-badge]: https://img.shields.io/badge/Cmake-20232A?style=for-the-badge&logo=cmake&logoColor=white
[cmake-url]: https://cmake.org

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- GETTING STARTED -->
## Getting Started

To add In-App-Purchasing capabilities to your Qt6/QML project follow the steps below.

### Prerequisites

* Qt/QML 6 (6.8 or higher)
* Apple StoreKit (iOS)
* Android Billing Client (Android)
* Windows SDK with WinRT support (Windows)

### Installation

1. Clone this repo into a folder in your project.
  ```sh
  git clone https://github.com/moritzstoetter/qt6purchasing.git
  ```
2. Copy 'android/GooglePlayBilling.java' to `QT_ANDROID_PACKAGE_SOURCE_DIR/src/com/COMPANY_NAME/APP_NAME/GooglePlayBilling.java`
  For more information on how to include custom Java-Code in your Android App see [Deploying an Application on Android](https://doc.qt.io/qt-6/deployment-android.html).
3. Add the qt6purchasing library's QML plugin to your project. In your project's `CMakeLists.txt` add the following:
   1. Ensure your project applies Qt 6.8 policies.
      ```cmake
	  qt_standard_project_setup(REQUIRES 6.8)
      ```
   2. Make this library's QML components available in the same build folder as all your own.
      ```
	  set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
      ```
   3. Link your app target to this library's QML module.
      ```cmake
      target_link_libraries(APP_TARGET
          PRIVATE
              ...
              qt6purchasinglibplugin
      )
      ```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Windows/Microsoft Store Setup

### Required COM Apartment Initialization

For Windows applications using Microsoft Store integration, you **must** initialize COM apartment mode in your application's `main.cpp`:

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winrt/base.h>
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

#ifdef Q_OS_WIN
    // CRITICAL: Fix COM apartment initialization for WinRT
    try {
        winrt::uninit_apartment();
        winrt::init_apartment(); // Defaults to multi-threaded
    } catch (...) {
        // Continue anyway - some functionality might still work
    }
#endif

    QQmlApplicationEngine engine;
    engine.loadFromModule("YourModule", "Main");
    return app.exec();
}
```

**Why this is required:**
- Qt initializes COM in single-threaded apartment mode
- WinRT Store APIs require multi-threaded apartment mode
- This must be done at the process level before any WinRT usage
- Without this, Store API calls will hang indefinitely

### Microsoft Store Product Configuration

Windows products require both a generic `identifier` and a Microsoft-specific `microsoftStoreId`:

```qml
Qt6Purchasing.Product {
    identifier: "premium_upgrade"        // Generic cross-platform identifier
    microsoftStoreId: "9NBLGGH4TNMP"    // Store ID from Microsoft Partner Center
    type: Qt6Purchasing.Product.Unlockable
}
```

The `microsoftStoreId` should be the exact Store ID from your Microsoft Partner Center add-on configuration.

### Windows Store Product Types

- **Durable** → Maps to `Product.Unlockable` (non-consumable, purchased once)
- **UnmanagedConsumable** → Maps to `Product.Consumable` (can be repurchased after consumption)

### Consumable Fulfillment

The library automatically handles consumable fulfillment for Microsoft Store. When you call `transaction.finalize()` on a consumable purchase, the library reports fulfillment to Microsoft Store with a unique tracking ID, allowing the user to repurchase the same consumable.

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- USAGE EXAMPLES -->
## Usage

1. In your QML file include the purchasing module:
  ```qml
  import Qt6Purchasing
  ```
2. Use it like this, for a product that is called "test_1" in the app store(s):
  ```qml
  Qt6Purchasing.Store {
    id: iapStore
    Qt6Purchasing.Product {
      id: testingProduct
      identifier: "test_1"
      type: Qt6Purchasing.Product.Consumable
    }
  }

  StoreItem {
    id: testingStoreItem
    product: testingProduct

    onIapCompleted: root.accepted()
  }
  ```
  `StoreItem.qml`:
  ```qml
  import Qt6Purchasing

  Item {
    id: root
    required property Qt6Purchasing.Product product

    signal iapCompleted

    enum PurchasingStatus {
        NoPurchase,
        PurchaseProcessing,
        PurchaseSuccess,
        PurchaseFail
    }
    property int purchasingStatus: StoreItem.PurchasingStatus.NoPurchase

    function purchase() {
        purchasingStatus = StoreItem.PurchasingStatus.PurchaseProcessing
        product.purchase()
    }

    function finalize(transaction) {
        purchasingStatus = StoreItem.PurchasingStatus.PurchaseSuccess
        transaction.finalize()
    }

    Connections {
        target: product
        function onPurchaseSucceeded(transaction) {
            finalize(transaction)
        }
        function onPurchaseRestored(transaction) {
            finalize(transaction)
        }
        function onPurchaseFailed(transaction) {
            purchasingStatus = StoreItem.PurchasingStatus.PurchaseFail
        }
        function onPurchaseConsumed(transaction) {
            root.iapCompleted()
        }
    }
  }
  ```
3. **Note**: Transactions will automatically be destroyed after your handler completes. If for some reason you want to hold onto a `transaction`, call `retain()` immediately and then call `destroy()` when you're done.
  ```qml
  Connections {
      target: product
      function onPurchaseConsumed(transaction) {
          transaction.retain() // Retain transaction while we do deferred work
          
          Qt.callLater(function() {
              // Update user preferences, analytics, etc.
              transaction.destroy()
          })
      }
  }
  ```


## Thread Safety

**Important**: Store backends must be created and destroyed on the main thread. The library uses static instances internally for routing platform callbacks, which requires main-thread access for thread safety.

In QML, this happens automatically since QML components are created on the main thread.

<p align="right">(<a href="#readme-top">back to top</a>)</p>


<!-- CONTRIBUTING -->
## Contributing

Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
Don't forget to give the project a star! Thanks again!

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- LICENSE -->
## License

Distributed under the MIT License. See `LICENSE.txt` for more information.

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- CONTACT -->
## Contact

Moritz Stötter - [moritzstoetter.dev](https://www.moritzstoetter.dev) - hi@moritzstoetter.dev


<p align="right">(<a href="#readme-top">back to top</a>)</p>
