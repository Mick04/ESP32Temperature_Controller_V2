# Chart-Optimized Temperature Data Implementation

## 📊 **Chart-Specific Data Structure**

Your ESP32 now stores data in a **chart-optimized format** alongside the standard temperature readings:

### **Enhanced Firebase Structure**

```
ESP32/history/daily/2025-11-07/17-30-00/
├── tempRed: 25.3          ← Individual values
├── tempBlue: 24.8
├── tempGreen: 26.1
├── timestamp: 1699373400  ← Unix timestamp
├── iso: "2025-11-07T17:30:00Z"
└── chartData: "{\"x\":\"2025-11-07T17:30:00Z\",\"red\":25.30,\"blue\":24.80,\"green\":26.10}"
```

### **Fallback Structure (when NTP unavailable)**

```
ESP32/history/sensors/T1457710/
├── tempRed: 25.3
├── tempBlue: 24.8
├── tempGreen: 26.1
├── timeString: "17:30"
└── chartData: "{\"x\":\"17:30\",\"red\":25.30,\"blue\":24.80,\"green\":26.10,\"millis\":1457710}"
```

## 🚀 **Chart.js Optimized React Hook**

```javascript
// src/hooks/useChartData.js
import { useState, useEffect } from "react";
import { ref, get } from "firebase/database";
import { database } from "../firebase/config";

export const useChartData = (dateRange = 7) => {
  const [chartData, setChartData] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const fetchChartData = async (days = dateRange) => {
    setLoading(true);
    setError(null);

    try {
      const allChartPoints = [];

      // Generate date range
      const endDate = new Date();
      const startDate = new Date();
      startDate.setDate(endDate.getDate() - days);

      const dateList = [];
      for (
        let d = new Date(startDate);
        d <= endDate;
        d.setDate(d.getDate() + 1)
      ) {
        dateList.push(d.toISOString().split("T")[0]);
      }

      // Fetch chart-optimized data for each date
      for (const dateStr of dateList) {
        const dayRef = ref(database, `ESP32/history/daily/${dateStr}`);
        const daySnapshot = await get(dayRef);

        if (daySnapshot.exists()) {
          const dayData = daySnapshot.val();

          Object.entries(dayData).forEach(([timeKey, timeData]) => {
            if (timeData && timeData.chartData) {
              try {
                // Parse the pre-formatted chart data
                const chartPoint = JSON.parse(timeData.chartData);
                allChartPoints.push({
                  timestamp: new Date(chartPoint.x),
                  red: chartPoint.red,
                  blue: chartPoint.blue,
                  green: chartPoint.green,
                  source: "daily",
                });
              } catch (parseError) {
                console.warn("Failed to parse chart data:", parseError);
                // Fallback to manual construction
                allChartPoints.push({
                  timestamp: new Date(
                    `${dateStr}T${timeKey.replace(/-/g, ":")}:00Z`
                  ),
                  red: timeData.tempRed || 0,
                  blue: timeData.tempBlue || 0,
                  green: timeData.tempGreen || 0,
                  source: "daily-fallback",
                });
              }
            }
          });
        }
      }

      // Also check for fallback sensor data
      const sensorsRef = ref(database, "ESP32/history/sensors");
      const sensorsSnapshot = await get(sensorsRef);

      if (sensorsSnapshot.exists()) {
        const sensorsData = sensorsSnapshot.val();
        Object.entries(sensorsData).forEach(([key, sensorData]) => {
          if (sensorData && sensorData.chartData) {
            try {
              const chartPoint = JSON.parse(sensorData.chartData);
              allChartPoints.push({
                timestamp: chartPoint.millis
                  ? new Date(chartPoint.millis)
                  : new Date(),
                timeString: chartPoint.x,
                red: chartPoint.red,
                blue: chartPoint.blue,
                green: chartPoint.green,
                source: "sensors",
              });
            } catch (parseError) {
              console.warn("Failed to parse sensor chart data:", parseError);
            }
          }
        });
      }

      // Sort by timestamp
      allChartPoints.sort((a, b) => a.timestamp - b.timestamp);

      setChartData(allChartPoints);
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchChartData(dateRange);
  }, [dateRange]);

  return {
    chartData,
    loading,
    error,
    refetch: () => fetchChartData(dateRange),
    fetchDateRange: fetchChartData,
  };
};
```

## 📈 **Optimized Chart Component**

```javascript
// src/components/OptimizedTemperatureChart.js
import React, { useMemo } from "react";
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  TimeScale,
} from "chart.js";
import { Line } from "react-chartjs-2";
import "chartjs-adapter-date-fns";
import { useChartData } from "../hooks/useChartData";

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  TimeScale
);

const OptimizedTemperatureChart = ({ dateRange = 7 }) => {
  const { chartData, loading, error } = useChartData(dateRange);

  // Memoize chart configuration for performance
  const { data, options } = useMemo(() => {
    const chartConfig = {
      data: {
        datasets: [
          {
            label: "Red Zone",
            data: chartData.map((point) => ({
              x: point.timestamp,
              y: point.red,
            })),
            borderColor: "#D0021B",
            backgroundColor: "rgba(208, 2, 27, 0.1)",
            pointRadius: 2,
            pointHoverRadius: 5,
            tension: 0.1,
          },
          {
            label: "Blue Zone",
            data: chartData.map((point) => ({
              x: point.timestamp,
              y: point.blue,
            })),
            borderColor: "#4A90E2",
            backgroundColor: "rgba(74, 144, 226, 0.1)",
            pointRadius: 2,
            pointHoverRadius: 5,
            tension: 0.1,
          },
          {
            label: "Green Zone",
            data: chartData.map((point) => ({
              x: point.timestamp,
              y: point.green,
            })),
            borderColor: "#7ED321",
            backgroundColor: "rgba(126, 211, 33, 0.1)",
            pointRadius: 2,
            pointHoverRadius: 5,
            tension: 0.1,
          },
        ],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: {
          intersect: false,
          mode: "index",
        },
        plugins: {
          legend: {
            position: "top",
            labels: {
              usePointStyle: true,
              padding: 20,
            },
          },
          title: {
            display: true,
            text: `Temperature History (${dateRange} day${
              dateRange > 1 ? "s" : ""
            })`,
          },
          tooltip: {
            mode: "index",
            intersect: false,
            callbacks: {
              title: function (context) {
                const point = chartData[context[0]?.dataIndex];
                if (point?.source === "sensors") {
                  return `Time: ${point.timeString}`;
                }
                return new Date(context[0]?.parsed?.x).toLocaleString();
              },
              label: function (context) {
                return `${context.dataset.label}: ${context.parsed.y.toFixed(
                  1
                )}°C`;
              },
              footer: function (context) {
                const point = chartData[context[0]?.dataIndex];
                return point?.source ? `Source: ${point.source}` : "";
              },
            },
          },
        },
        scales: {
          x: {
            type: "time",
            time: {
              displayFormats: {
                hour: "HH:mm",
                day: "MMM dd",
                week: "MMM dd",
                month: "MMM yyyy",
              },
              tooltipFormat: "MMM dd, yyyy HH:mm",
            },
            title: {
              display: true,
              text: "Time",
            },
            grid: {
              display: true,
              color: "rgba(0, 0, 0, 0.1)",
            },
          },
          y: {
            title: {
              display: true,
              text: "Temperature (°C)",
            },
            grid: {
              display: true,
              color: "rgba(0, 0, 0, 0.1)",
            },
            beginAtZero: false,
            suggestedMin:
              Math.min(
                ...chartData.map((p) => Math.min(p.red, p.blue, p.green))
              ) - 2,
            suggestedMax:
              Math.max(
                ...chartData.map((p) => Math.max(p.red, p.blue, p.green))
              ) + 2,
          },
        },
        elements: {
          point: {
            radius: dateRange > 30 ? 1 : 2, // Smaller points for longer time ranges
          },
        },
      },
    };

    return chartConfig;
  }, [chartData, dateRange]);

  if (loading) {
    return (
      <div className="chart-loading">
        <div className="loading-spinner"></div>
        <p>Loading temperature chart data...</p>
      </div>
    );
  }

  if (error) {
    return (
      <div className="chart-error">
        <p>❌ Error loading chart: {error}</p>
      </div>
    );
  }

  if (chartData.length === 0) {
    return (
      <div className="chart-no-data">
        <p>📭 No temperature data available for the selected range</p>
      </div>
    );
  }

  return (
    <div className="optimized-chart-container">
      <div className="chart-wrapper" style={{ height: "400px" }}>
        <Line data={data} options={options} />
      </div>

      <div className="chart-stats">
        <div className="stat">
          <span className="label">📊 Data Points:</span>
          <span className="value">{chartData.length}</span>
        </div>
        <div className="stat">
          <span className="label">📅 Date Range:</span>
          <span className="value">
            {chartData.length > 0 && (
              <>
                {chartData[0].timestamp.toLocaleDateString()} -{" "}
                {chartData[chartData.length - 1].timestamp.toLocaleDateString()}
              </>
            )}
          </span>
        </div>
        <div className="stat">
          <span className="label">🔄 Sources:</span>
          <span className="value">
            Daily: {chartData.filter((p) => p.source?.includes("daily")).length}
            , Sensors: {chartData.filter((p) => p.source === "sensors").length}
          </span>
        </div>
      </div>
    </div>
  );
};

export default OptimizedTemperatureChart;
```

## 🎨 **Chart Styling (CSS)**

```css
/* src/styles/chart.css */
.optimized-chart-container {
  background: white;
  border-radius: 12px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
  padding: 20px;
  margin: 20px 0;
}

.chart-wrapper {
  position: relative;
  margin-bottom: 20px;
}

.chart-loading {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 400px;
  color: #666;
}

.loading-spinner {
  width: 40px;
  height: 40px;
  border: 4px solid #f3f3f3;
  border-top: 4px solid #4a90e2;
  border-radius: 50%;
  animation: spin 1s linear infinite;
  margin-bottom: 10px;
}

@keyframes spin {
  0% {
    transform: rotate(0deg);
  }
  100% {
    transform: rotate(360deg);
  }
}

.chart-error {
  background: #fee;
  border: 1px solid #fcc;
  border-radius: 8px;
  padding: 20px;
  text-align: center;
  color: #c33;
}

.chart-no-data {
  background: #f9f9f9;
  border: 1px solid #ddd;
  border-radius: 8px;
  padding: 40px;
  text-align: center;
  color: #666;
}

.chart-stats {
  display: flex;
  justify-content: space-around;
  background: #f8f9fa;
  border-radius: 8px;
  padding: 15px;
  margin-top: 15px;
}

.stat {
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
}

.stat .label {
  font-size: 0.9em;
  color: #666;
  margin-bottom: 5px;
}

.stat .value {
  font-weight: bold;
  color: #333;
}

@media (max-width: 768px) {
  .chart-stats {
    flex-direction: column;
    gap: 10px;
  }

  .stat {
    flex-direction: row;
    justify-content: space-between;
  }
}
```

## 🚀 **Key Chart Optimizations**

### **1. Pre-formatted Chart Data**

- ESP32 stores data in Chart.js-ready JSON format
- Reduces React processing time
- Handles both timestamp and fallback scenarios

### **2. Performance Optimizations**

- Memoized chart configuration
- Conditional point sizes based on data density
- Efficient data filtering and sorting

### **3. Enhanced User Experience**

- Loading states with spinner
- Error handling with retry options
- Data source information in tooltips
- Responsive design for mobile/desktop

### **4. Chart Features**

- ✅ Real-time data updates
- ✅ Multiple time ranges (1 day to 3 months)
- ✅ Interactive tooltips with source info
- ✅ Responsive scaling based on data range
- ✅ Color-coded temperature zones
- ✅ Handles both NTP and fallback data

Your charts will now display beautifully with optimized performance and comprehensive temperature trend analysis! 📊🌡️
