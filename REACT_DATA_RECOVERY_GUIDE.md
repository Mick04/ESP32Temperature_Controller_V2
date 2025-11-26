# React App Firebase Data Recovery Guide

## Firebase Configuration

First, set up Firebase in your React app:

```bash
npm install firebase
```

### 1. Firebase Config (`src/firebase/config.js`)

```javascript
import { initializeApp } from "firebase/app";
import { getDatabase } from "firebase/database";

const firebaseConfig = {
  apiKey: "your-api-key",
  authDomain: "your-project.firebaseapp.com",
  databaseURL: "https://your-project-default-rtdb.firebaseio.com/",
  projectId: "your-project-id",
  storageBucket: "your-project.appspot.com",
  messagingSenderId: "123456789",
  appId: "your-app-id",
};

const app = initializeApp(firebaseConfig);
export const database = getDatabase(app);
```

## Data Recovery Hooks

### 2. Real-time Temperature Hook (`src/hooks/useTemperatureData.js`)

```javascript
import { useState, useEffect } from "react";
import { ref, onValue, off } from "firebase/database";
import { database } from "../firebase/config";

export const useTemperatureData = () => {
  const [currentData, setCurrentData] = useState({
    tempRed: null,
    tempBlue: null,
    tempGreen: null,
    timestamp: null,
    lastUpdated: null,
  });
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    const temperatureRef = ref(database, "ESP32/temperatureSensors");

    const unsubscribe = onValue(
      temperatureRef,
      (snapshot) => {
        try {
          if (snapshot.exists()) {
            const data = snapshot.val();
            setCurrentData({
              tempRed: data.tempRed || null,
              tempBlue: data.tempBlue || null,
              tempGreen: data.tempGreen || null,
              timestamp: data.timestamp || null,
              lastUpdated: new Date().toISOString(),
            });
          }
          setLoading(false);
        } catch (err) {
          setError(err.message);
          setLoading(false);
        }
      },
      (error) => {
        setError(error.message);
        setLoading(false);
      }
    );

    return () => off(temperatureRef, "value", unsubscribe);
  }, []);

  return { currentData, loading, error };
};
```

### 3. Historical Data Hook (`src/hooks/useHistoricalData.js`)

```javascript
import { useState, useEffect } from "react";
import { ref, get, query, orderByKey } from "firebase/database";
import { database } from "../firebase/config";

export const useHistoricalData = (dateRange = 7) => {
  const [historicalData, setHistoricalData] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const fetchHistoricalData = async (days = dateRange) => {
    setLoading(true);
    setError(null);

    try {
      const endDate = new Date();
      const startDate = new Date();
      startDate.setDate(endDate.getDate() - days);

      const dateList = [];
      for (
        let d = new Date(startDate);
        d <= endDate;
        d.setDate(d.getDate() + 1)
      ) {
        dateList.push(d.toISOString().split("T")[0]); // YYYY-MM-DD format
      }

      const allData = [];

      // Fetch data for each date
      for (const dateStr of dateList) {
        const dayRef = ref(database, `ESP32/history/daily/${dateStr}`);
        const daySnapshot = await get(dayRef);

        if (daySnapshot.exists()) {
          const dayData = daySnapshot.val();

          // Convert time entries to array and sort
          Object.entries(dayData).forEach(([timeKey, timeData]) => {
            if (timeData && typeof timeData === "object") {
              allData.push({
                date: dateStr,
                time: timeKey,
                fullTimestamp:
                  timeData.iso ||
                  `${dateStr}T${timeKey.replace(/-/g, ":")}:00Z`,
                unixTimestamp: timeData.timestamp || null,
                tempRed: timeData.tempRed || null,
                tempBlue: timeData.tempBlue || null,
                tempGreen: timeData.tempGreen || null,
              });
            }
          });
        }
      }

      // Sort by timestamp
      allData.sort(
        (a, b) => new Date(a.fullTimestamp) - new Date(b.fullTimestamp)
      );

      setHistoricalData(allData);
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchHistoricalData(dateRange);
  }, [dateRange]);

  return {
    historicalData,
    loading,
    error,
    refetch: () => fetchHistoricalData(dateRange),
    fetchDateRange: fetchHistoricalData,
  };
};
```

### 4. Single Day Data Hook (`src/hooks/useDayData.js`)

```javascript
import { useState, useEffect } from "react";
import { ref, get } from "firebase/database";
import { database } from "../firebase/config";

export const useDayData = (selectedDate) => {
  const [dayData, setDayData] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);

  useEffect(() => {
    if (!selectedDate) return;

    const fetchDayData = async () => {
      setLoading(true);
      setError(null);

      try {
        const dateStr = selectedDate.toISOString().split("T")[0]; // YYYY-MM-DD
        const dayRef = ref(database, `ESP32/history/daily/${dateStr}`);
        const snapshot = await get(dayRef);

        if (snapshot.exists()) {
          const data = snapshot.val();
          const timeEntries = Object.entries(data).map(
            ([timeKey, timeData]) => ({
              time: timeKey,
              fullTimestamp:
                timeData.iso || `${dateStr}T${timeKey.replace(/-/g, ":")}:00Z`,
              tempRed: timeData.tempRed || null,
              tempBlue: timeData.tempBlue || null,
              tempGreen: timeData.tempGreen || null,
            })
          );

          // Sort by time
          timeEntries.sort((a, b) => a.time.localeCompare(b.time));
          setDayData(timeEntries);
        } else {
          setDayData([]);
        }
      } catch (err) {
        setError(err.message);
      } finally {
        setLoading(false);
      }
    };

    fetchDayData();
  }, [selectedDate]);

  return { dayData, loading, error };
};
```

## React Components

### 5. Temperature Dashboard (`src/components/TemperatureDashboard.js`)

```javascript
import React, { useState } from "react";
import { useTemperatureData } from "../hooks/useTemperatureData";
import { useHistoricalData } from "../hooks/useHistoricalData";
import TemperatureChart from "./TemperatureChart";
import CurrentReadings from "./CurrentReadings";

const TemperatureDashboard = () => {
  const [dateRange, setDateRange] = useState(7); // Default 7 days
  const { currentData, loading: currentLoading } = useTemperatureData();
  const {
    historicalData,
    loading: historyLoading,
    fetchDateRange,
  } = useHistoricalData(dateRange);

  const handleDateRangeChange = (days) => {
    setDateRange(days);
    fetchDateRange(days);
  };

  if (currentLoading && historyLoading) {
    return <div className="loading">Loading temperature data...</div>;
  }

  return (
    <div className="temperature-dashboard">
      <h1>ESP32 Temperature Controller</h1>

      {/* Current Readings */}
      <CurrentReadings data={currentData} />

      {/* Date Range Selector */}
      <div className="date-range-controls">
        <button
          onClick={() => handleDateRangeChange(1)}
          className={dateRange === 1 ? "active" : ""}
        >
          24 Hours
        </button>
        <button
          onClick={() => handleDateRangeChange(7)}
          className={dateRange === 7 ? "active" : ""}
        >
          7 Days
        </button>
        <button
          onClick={() => handleDateRangeChange(30)}
          className={dateRange === 30 ? "active" : ""}
        >
          30 Days
        </button>
        <button
          onClick={() => handleDateRangeChange(90)}
          className={dateRange === 90 ? "active" : ""}
        >
          3 Months
        </button>
      </div>

      {/* Historical Chart */}
      {historicalData.length > 0 && (
        <TemperatureChart data={historicalData} dateRange={dateRange} />
      )}

      {historyLoading && <div>Loading historical data...</div>}
    </div>
  );
};

export default TemperatureDashboard;
```

### 6. Current Readings Component (`src/components/CurrentReadings.js`)

```javascript
import React from "react";

const CurrentReadings = ({ data }) => {
  const formatTemperature = (temp) => {
    return temp !== null ? `${temp.toFixed(1)}°C` : "N/A";
  };

  const getTemperatureColor = (temp) => {
    if (temp === null) return "#ccc";
    if (temp < 20) return "#4A90E2"; // Blue
    if (temp < 25) return "#7ED321"; // Green
    if (temp < 30) return "#F5A623"; // Orange
    return "#D0021B"; // Red
  };

  return (
    <div className="current-readings">
      <h2>Current Temperature Readings</h2>
      <div className="reading-cards">
        <div className="reading-card red-zone">
          <h3>Red Zone</h3>
          <div
            className="temperature-value"
            style={{ color: getTemperatureColor(data.tempRed) }}
          >
            {formatTemperature(data.tempRed)}
          </div>
        </div>

        <div className="reading-card blue-zone">
          <h3>Blue Zone</h3>
          <div
            className="temperature-value"
            style={{ color: getTemperatureColor(data.tempBlue) }}
          >
            {formatTemperature(data.tempBlue)}
          </div>
        </div>

        <div className="reading-card green-zone">
          <h3>Green Zone</h3>
          <div
            className="temperature-value"
            style={{ color: getTemperatureColor(data.tempGreen) }}
          >
            {formatTemperature(data.tempGreen)}
          </div>
        </div>
      </div>

      {data.lastUpdated && (
        <div className="last-updated">
          Last updated: {new Date(data.lastUpdated).toLocaleString()}
        </div>
      )}
    </div>
  );
};

export default CurrentReadings;
```

### 7. Temperature Chart Component (`src/components/TemperatureChart.js`)

Using Chart.js for visualization:

```bash
npm install react-chartjs-2 chart.js
```

```javascript
import React from "react";
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

const TemperatureChart = ({ data, dateRange }) => {
  const chartData = {
    datasets: [
      {
        label: "Red Zone",
        data: data
          .filter((d) => d.tempRed !== null)
          .map((d) => ({
            x: new Date(d.fullTimestamp),
            y: d.tempRed,
          })),
        borderColor: "#D0021B",
        backgroundColor: "rgba(208, 2, 27, 0.1)",
        tension: 0.1,
      },
      {
        label: "Blue Zone",
        data: data
          .filter((d) => d.tempBlue !== null)
          .map((d) => ({
            x: new Date(d.fullTimestamp),
            y: d.tempBlue,
          })),
        borderColor: "#4A90E2",
        backgroundColor: "rgba(74, 144, 226, 0.1)",
        tension: 0.1,
      },
      {
        label: "Green Zone",
        data: data
          .filter((d) => d.tempGreen !== null)
          .map((d) => ({
            x: new Date(d.fullTimestamp),
            y: d.tempGreen,
          })),
        borderColor: "#7ED321",
        backgroundColor: "rgba(126, 211, 33, 0.1)",
        tension: 0.1,
      },
    ],
  };

  const options = {
    responsive: true,
    plugins: {
      legend: {
        position: "top",
      },
      title: {
        display: true,
        text: `Temperature History (${dateRange} day${
          dateRange > 1 ? "s" : ""
        })`,
      },
    },
    scales: {
      x: {
        type: "time",
        time: {
          displayFormats: {
            hour: "HH:mm",
            day: "MMM dd",
          },
        },
        title: {
          display: true,
          text: "Time",
        },
      },
      y: {
        title: {
          display: true,
          text: "Temperature (°C)",
        },
        min: 15,
        max: 35,
      },
    },
    interaction: {
      intersect: false,
      mode: "index",
    },
  };

  return (
    <div className="temperature-chart">
      <Line data={chartData} options={options} />
    </div>
  );
};

export default TemperatureChart;
```

## Usage Example

### 8. Main App Component (`src/App.js`)

```javascript
import React from "react";
import TemperatureDashboard from "./components/TemperatureDashboard";
import "./App.css";

function App() {
  return (
    <div className="App">
      <TemperatureDashboard />
    </div>
  );
}

export default App;
```

### 9. Basic Styling (`src/App.css`)

```css
.temperature-dashboard {
  max-width: 1200px;
  margin: 0 auto;
  padding: 20px;
}

.current-readings {
  margin-bottom: 30px;
}

.reading-cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 20px;
  margin: 20px 0;
}

.reading-card {
  padding: 20px;
  border-radius: 8px;
  background: white;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  text-align: center;
}

.reading-card h3 {
  margin: 0 0 10px 0;
  color: #333;
}

.temperature-value {
  font-size: 2em;
  font-weight: bold;
}

.date-range-controls {
  margin: 20px 0;
  display: flex;
  gap: 10px;
}

.date-range-controls button {
  padding: 10px 20px;
  border: 1px solid #ddd;
  background: white;
  cursor: pointer;
  border-radius: 4px;
}

.date-range-controls button.active {
  background: #4a90e2;
  color: white;
}

.temperature-chart {
  margin-top: 30px;
  background: white;
  padding: 20px;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

.loading {
  text-align: center;
  padding: 50px;
  font-size: 1.2em;
  color: #666;
}

.last-updated {
  margin-top: 15px;
  color: #666;
  font-size: 0.9em;
  text-align: center;
}
```

## Key Features

### Real-time Updates

- Automatic real-time updates for current temperature readings
- Firebase onValue listeners for live data

### Historical Data Recovery

- Efficient daily-based data structure queries
- Flexible date range selection (1 day to 3 months)
- Optimized for the ESP32's organized storage structure

### Interactive Charts

- Multi-line charts showing all three temperature zones
- Time-based x-axis with proper scaling
- Responsive design for different screen sizes

### Error Handling

- Graceful handling of missing data
- Loading states and error messages
- Fallback for offline scenarios

This React implementation efficiently recovers and displays both real-time and historical temperature data from your ESP32's Firebase storage structure! 🌡️📊
