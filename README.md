# StockHound Stock Analyzer
## Scoring System
### Moving Average Score (MA Score):
* A score closer to 1.0 indicates that the stock price is very close to the moving average, suggesting alignment with the trend.
* A lower score indicates the price is significantly above or below the moving average, potentially signaling overvaluation or undervaluation.

### Relative Strength Index Score (RSI Score):
* A score closer to 1.0 indicates that the RSI is near 30, which is often considered a good buying opportunity in technical analysis.
* A lower score suggests the RSI is moving toward the overbought (70+) or oversold (below 30) range, which might signal less favorable conditions.

### Bollinger Bands Score (BB Score):
* A score closer to 1.0 means the stock price is well-positioned between the lower and upper Bollinger Bands, indicating less volatility.
* A lower score could suggest the price is too close to the bands' extremes, which might imply instability or a breakout.
Total Score:

**The total weighted score, combining the three scores, will also range between 0.0 and 1.0, where a score closer to 1.0 suggests that the stock is an overall strong candidate for investment based on the weighted criteria.**

## Weighted Criteria
### Moving Average Score (40%):
* Favors prices close to the moving average, which may indicate stability or trend alignment.

### RSI Score (30%):
* Rewards stocks with an RSI near 30, avoiding overbought (RSI > 70) or oversold (RSI < 30) conditions.

### Bollinger Bands Score (30%):
* Prefers prices within the Bollinger Bands, indicating less volatility.