# React App: Handling Unknown Folder Names

## 🔍 **The Problem**

When NTP time isn't synchronized, the ESP32 stores data in unpredictable paths:

```
ESP32/history/sensors/T1457710/  ← How does React find this?
ESP32/history/sensors/T2894567/  ← Or this?
```

## ✅ **Solution 1: Latest Pointer (Implemented)**

The ESP32 now maintains a "latest" pointer that always points to the most recent data:

### **Firebase Structure**

```
ESP32/
├── history/
│   ├── latest: "ESP32/history/sensors/T1457710"  ← Always points to newest
│   ├── sensors/
│   │   ├── T1457710/ ← Actual data location
│   │   └── T2894567/ ← Previous data
│   └── daily/
│       └── 2025-11-07/ ← When NTP works
```

### **React Hook: Latest Data Discovery**

```javascript
// src/hooks/useLatestHistoricalData.js
import { useState, useEffect } from "react";
import { ref, get } from "firebase/database";
import { database } from "../firebase/config";

export const useLatestHistoricalData = () => {
  const [latestData, setLatestData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const fetchLatestData = async () => {
    setLoading(true);
    setError(null);

    try {
      // Step 1: Get the pointer to latest data
      const latestRef = ref(database, "ESP32/history/latest");
      const latestSnapshot = await get(latestRef);

      if (!latestSnapshot.exists()) {
        throw new Error("No latest data pointer found");
      }

      const latestPath = latestSnapshot.val();
      console.log("📍 Latest data path:", latestPath);

      // Step 2: Get the actual data from the pointed location
      const dataRef = ref(database, latestPath);
      const dataSnapshot = await get(dataRef);

      if (dataSnapshot.exists()) {
        const data = dataSnapshot.val();
        setLatestData({
          path: latestPath,
          tempRed: data.tempRed || null,
          tempBlue: data.tempBlue || null,
          tempGreen: data.tempGreen || null,
          timeString: data.timeString || null,
          timestamp: data.timestamp || null,
          iso: data.iso || null,
        });
      } else {
        throw new Error("Latest data not found at pointed location");
      }
    } catch (err) {
      setError(err.message);
      console.error("❌ Error fetching latest data:", err);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchLatestData();

    // Refresh every 30 seconds to catch new data
    const interval = setInterval(fetchLatestData, 30000);
    return () => clearInterval(interval);
  }, []);

  return { latestData, loading, error, refresh: fetchLatestData };
};
```

## 🔍 **Solution 2: Query All Historical Entries**

For comprehensive historical data recovery regardless of folder structure:

```javascript
// src/hooks/useAllHistoricalData.js
import { useState, useEffect } from "react";
import { ref, get } from "firebase/database";
import { database } from "../firebase/config";

export const useAllHistoricalData = () => {
  const [allData, setAllData] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const fetchAllHistoricalData = async () => {
    setLoading(true);
    setError(null);

    try {
      const allHistoricalData = [];

      // Query both possible data locations

      // 1. Check organized daily structure
      const dailyRef = ref(database, "ESP32/history/daily");
      const dailySnapshot = await get(dailyRef);

      if (dailySnapshot.exists()) {
        const dailyData = dailySnapshot.val();
        Object.entries(dailyData).forEach(([date, dayData]) => {
          Object.entries(dayData).forEach(([time, timeData]) => {
            if (timeData && typeof timeData === "object") {
              allHistoricalData.push({
                source: "daily",
                date,
                time,
                fullTimestamp:
                  timeData.iso || `${date}T${time.replace(/-/g, ":")}:00Z`,
                path: `ESP32/history/daily/${date}/${time}`,
                ...timeData,
              });
            }
          });
        });
      }

      // 2. Check fallback sensor structure
      const sensorsRef = ref(database, "ESP32/history/sensors");
      const sensorsSnapshot = await get(sensorsRef);

      if (sensorsSnapshot.exists()) {
        const sensorsData = sensorsSnapshot.val();
        Object.entries(sensorsData).forEach(([key, sensorData]) => {
          if (sensorData && typeof sensorData === "object") {
            allHistoricalData.push({
              source: "sensors",
              key,
              timeString: sensorData.timeString,
              path: `ESP32/history/sensors/${key}`,
              tempRed: sensorData.tempRed,
              tempBlue: sensorData.tempBlue,
              tempGreen: sensorData.tempGreen,
              // Estimate timestamp from millis if no proper timestamp
              estimatedTime: sensorData.timeString || "Unknown",
            });
          }
        });
      }

      // Sort all data by timestamp/time
      allHistoricalData.sort((a, b) => {
        if (a.fullTimestamp && b.fullTimestamp) {
          return new Date(a.fullTimestamp) - new Date(b.fullTimestamp);
        }
        // Put sensor data (without timestamps) at the end
        if (a.source === "sensors" && b.source === "daily") return 1;
        if (a.source === "daily" && b.source === "sensors") return -1;
        return 0;
      });

      setAllData(allHistoricalData);
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchAllHistoricalData();
  }, []);

  return { allData, loading, error, refetch: fetchAllHistoricalData };
};
```

## 📊 **Solution 3: Enhanced Chart Component**

Updated chart component that handles both data structures:

```javascript
// src/components/FlexibleTemperatureChart.js
import React from "react";
import { Line } from "react-chartjs-2";
import { useAllHistoricalData } from "../hooks/useAllHistoricalData";

const FlexibleTemperatureChart = () => {
  const { allData, loading, error } = useAllHistoricalData();

  if (loading) return <div>Loading historical data...</div>;
  if (error) return <div>Error: {error}</div>;

  // Process data for chart
  const chartData = {
    datasets: [
      {
        label: "Red Zone",
        data: allData
          .filter((d) => d.tempRed !== null && d.tempRed !== undefined)
          .map((d, index) => ({
            x: d.fullTimestamp ? new Date(d.fullTimestamp) : index,
            y: d.tempRed,
          })),
        borderColor: "#D0021B",
        backgroundColor: "rgba(208, 2, 27, 0.1)",
      },
      {
        label: "Blue Zone",
        data: allData
          .filter((d) => d.tempBlue !== null && d.tempBlue !== undefined)
          .map((d, index) => ({
            x: d.fullTimestamp ? new Date(d.fullTimestamp) : index,
            y: d.tempBlue,
          })),
        borderColor: "#4A90E2",
        backgroundColor: "rgba(74, 144, 226, 0.1)",
      },
      {
        label: "Green Zone",
        data: allData
          .filter((d) => d.tempGreen !== null && d.tempGreen !== undefined)
          .map((d, index) => ({
            x: d.fullTimestamp ? new Date(d.fullTimestamp) : index,
            y: d.tempGreen,
          })),
        borderColor: "#7ED321",
        backgroundColor: "rgba(126, 211, 33, 0.1)",
      },
    ],
  };

  const options = {
    responsive: true,
    plugins: {
      title: {
        display: true,
        text: "Temperature History (All Available Data)",
      },
      legend: {
        position: "top",
      },
    },
    scales: {
      x: {
        type: allData.some((d) => d.fullTimestamp) ? "time" : "linear",
        title: {
          display: true,
          text: allData.some((d) => d.fullTimestamp) ? "Time" : "Data Points",
        },
      },
      y: {
        title: {
          display: true,
          text: "Temperature (°C)",
        },
      },
    },
  };

  return (
    <div className="flexible-chart">
      <Line data={chartData} options={options} />

      {/* Data source information */}
      <div className="data-info">
        <p>📊 Total data points: {allData.length}</p>
        <p>
          🗓️ Organized data:{" "}
          {allData.filter((d) => d.source === "daily").length}
        </p>
        <p>
          ⏱️ Fallback data:{" "}
          {allData.filter((d) => d.source === "sensors").length}
        </p>
      </div>
    </div>
  );
};

export default FlexibleTemperatureChart;
```

## 🚀 **Solution 4: Real-time Latest Data Component**

Component that always shows the most recent reading:

```javascript
// src/components/LatestTemperatureDisplay.js
import React from "react";
import { useLatestHistoricalData } from "../hooks/useLatestHistoricalData";

const LatestTemperatureDisplay = () => {
  const { latestData, loading, error } = useLatestHistoricalData();

  if (loading) return <div>🔄 Loading latest data...</div>;
  if (error) return <div>❌ Error: {error}</div>;
  if (!latestData) return <div>📭 No data available</div>;

  return (
    <div className="latest-temperature">
      <h3>🌡️ Latest Temperature Reading</h3>

      <div className="temperature-grid">
        <div className="temp-card red">
          <span className="zone">Red Zone</span>
          <span className="value">{latestData.tempRed?.toFixed(1)}°C</span>
        </div>
        <div className="temp-card blue">
          <span className="zone">Blue Zone</span>
          <span className="value">{latestData.tempBlue?.toFixed(1)}°C</span>
        </div>
        <div className="temp-card green">
          <span className="zone">Green Zone</span>
          <span className="value">{latestData.tempGreen?.toFixed(1)}°C</span>
        </div>
      </div>

      <div className="data-source">
        <p>📍 Data source: {latestData.path}</p>
        <p>⏰ Time: {latestData.timeString || latestData.iso || "Unknown"}</p>
      </div>
    </div>
  );
};

export default LatestTemperatureDisplay;
```

## 🎯 **Key Benefits**

### **Latest Pointer Approach**

- ✅ Always finds newest data regardless of folder structure
- ✅ Works with both NTP and fallback timestamps
- ✅ Minimal Firebase queries (just 2 reads)
- ✅ Real-time discovery of new data

### **Comprehensive Query Approach**

- ✅ Finds all historical data regardless of structure
- ✅ Handles mixed data sources (daily + sensors)
- ✅ Perfect for complete historical analysis
- ⚠️ More Firebase reads required

## 🔧 **Implementation Steps**

1. **Upload Enhanced ESP32 Code** (adds latest pointer)
2. **Implement React Hooks** for data discovery
3. **Use Flexible Components** that handle both structures
4. **Test with Both Scenarios** (NTP working and fallback)

Your React app will now find temperature data regardless of whether it's in organized daily folders or unpredictable `T1457710` paths! 🎯📊
