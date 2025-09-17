# StockHound Stock Analyzer

StockHound is a **cross-platform C++ stock analysis tool** with a Qt-based GUI.  
It integrates technical indicators such as **Moving Averages, RSI, and Bollinger Bands** to generate a total weighted score for each stock.  

StockHound can be built and run on both **Linux** and **Windows**. On Windows, we use **vcpkg** for dependency management and automatically handle required DLLs.

---

## 📊 Scoring System

### Moving Average Score (MA Score)
- A score closer to **1.0** indicates the stock price is very close to the moving average, suggesting alignment with the trend.
- A lower score indicates the price is significantly above or below the moving average, potentially signaling overvaluation or undervaluation.

### Relative Strength Index Score (RSI Score)
- A score closer to **1.0** indicates that the RSI is near 30, which is often considered a good buying opportunity in technical analysis.
- A lower score suggests the RSI is moving toward the overbought (70+) or oversold (below 30) range, which might signal less favorable conditions.

### Bollinger Bands Score (BB Score)
- A score closer to **1.0** means the stock price is well-positioned between the lower and upper Bollinger Bands, indicating less volatility.
- A lower score could suggest the price is too close to the bands' extremes, implying instability or a potential breakout.

### Total Weighted Score
- Combines all three scores to produce a value between **0.0 and 1.0**.
- A score closer to **>=0.8** indicates the stock is a strong candidate for investment based on the weighted criteria.
---

## Weighted Criteria

| Score Type | Weight | Description |
|------------|--------|-------------|
| Moving Average | 40% | Favors prices close to the moving average, indicating stability or trend alignment. |
| RSI | 30% | Rewards stocks with RSI near 30, avoiding overbought (>70) or oversold (<30) conditions. |
| Bollinger Bands | 30% | Prefers prices within the Bollinger Bands, indicating less volatility. |

---

## Building on Linux

### Install dependencies (Debian/Ubuntu example)
```bash
sudo apt update
sudo apt install -y cmake g++ qt5-base-dev libsqlite3-dev libcurl4-openssl-dev nlohmann-json3-dev
```

### Clone the repository (with submodules)
```bash
git clone --recurse-submodules https://github.com/menevia16a/StockHound.git
cd StockHound
```

### Build
```bash
cmake -B build -S .
cmake --build build
```

### Run
```bash
./build/StockHound
```

---

## Building on Windows (Visual Studio + vcpkg)

### 1. Install vcpkg
```powershell
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

### 2. Install dependencies
```powershell
.\vcpkg install qt5-base:x64-windows sqlite3:x64-windows curl:x64-windows nlohmann-json:x64-windows glog:x64-windows jsoncpp:x64-windows rapidjson:x64-windows cpp-httplib:x64-windows openssl:x64-windows libwebsockets:x64-windows uwebsockets:x64-windows gtest:x64-windows
```

### 3. Clone the repository
```powershell
git clone --recurse-submodules git@github.com:menevia16a/StockHound.git
cd StockHound
mkdir build
cd build
```

### 4. Configure with CMake
Replace `<path-to-vcpkg>` with your vcpkg folder path:
```powershell
cmake .. -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>\scripts\buildsystems\vcpkg.cmake -G "Visual Studio 17 2022" -A x64
```

### 5. Build
Open `StockHound.sln` in Visual Studio and build **Release**, or from CLI:
```powershell
cmake --build . --config Release
```

### 6. Run
After building:
```powershell
.\build\Release\StockHound.exe
```

**Notes for Windows users:**
- `windeployqt` automatically copies all required Qt DLLs.
- SQLite and cURL DLLs are also automatically copied by CMake post-build commands.

---

# Setting Up Environment Variables

StockHound requires a few environment variables to interact with the Alpaca API. Two are **required** and two are **optional**.

## Required

1. **`APCA_API_KEY_ID`**
   Your Alpaca API key ID.

2. **`APCA_API_SECRET_KEY`**
   Your Alpaca API secret key.

## Optional

1. **`APCA_API_BASE_URL`**
   Base URL for the Alpaca API. Defaults to:

   ```
   https://api.alpaca.markets
   ```

2. **`APCA_API_DATA_URL`**
   Base URL for the Alpaca data API. Defaults to:

   ```
   https://data.alpaca.markets
   ```

## Setting Environment Variables

### Linux / macOS (bash/zsh)

```bash
export APCA_API_KEY_ID="your_api_key_id_here"
export APCA_API_SECRET_KEY="your_api_secret_key_here"
export APCA_API_BASE_URL="https://api.alpaca.markets"        # optional
export APCA_API_DATA_URL="https://data.alpaca.markets"       # optional
```

### Windows (PowerShell)

```powershell
$Env:APCA_API_KEY_ID="your_api_key_id_here"
$Env:APCA_API_SECRET_KEY="your_api_secret_key_here"
$Env:APCA_API_BASE_URL="https://api.alpaca.markets"          # optional
$Env:APCA_API_DATA_URL="https://data.alpaca.markets"         # optional
```

> **Note:** You must set the required variables before running StockHound, otherwise the application will not be able to connect to Alpaca.

---

## ⚡ Development Notes
- Use **Debug** build configuration for development and testing.
- On Linux, you can enable debugging via `-DCMAKE_BUILD_TYPE=Debug`.
- On Windows, ensure Visual Studio is configured for x64 builds to match vcpkg libraries.

---

## Contact & Support

For any questions or feedback, you can reach me at:

**Email:** [veilbreaker@voidmaster.xyz](mailto:veilbreaker@voidmaster.xyz)

If you would like to support the development of StockHound, donations are appreciated:

**Donate:** [PayPal](https://www.paypal.me/JosiahWatkins)

