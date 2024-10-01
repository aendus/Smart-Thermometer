#include <DHT.h>                  // DHT sensor library
#include <ESP8266WiFi.h>           // WiFi library for ESP8266
#include <ESP8266WebServer.h>      // Web server library for ESP8266
#include <WiFiUdp.h>               // Required for NTP client
#include <NTPClient.h>             // NTP client to fetch time

// WiFi credentials
const char* ssid = "89guest";
const char* password = "nichtganz90";

String location = "Bern"; // Variable to store the location

// Define the pin where the data pin of AM2302 is connected
#define DHTPIN 5 // D1 corresponds to GPIO 5

// Define the type of sensor
#define DHTTYPE DHT22 // DHT 22 (AM2302)

// Define LED pins
#define GREEN_LED_PIN 12 // GPIO 12 for green LED
#define RED_LED_PIN 13   // GPIO 13 for red LED

// Initialize the DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Create an instance of the web server
ESP8266WebServer server(80);

// NTP settings
WiFiUDP ntpUDP;

// Function to get timezone offset based on the location
int getTimeZoneOffset(String location) {
    if (location == "Bern") return 7200; // UTC+1 for Bern (CET)
    // Add more locations and their respective offsets here
    // Example: if (location == "New York") return -18000; // UTC-5 for New York (EST)
    return 3600; // Default to UTC+1 if not found
}

NTPClient timeClient(ntpUDP, "pool.ntp.org", getTimeZoneOffset(location), 60000); // 60000 ms = 1 minute update interval

// Arrays to store temperature readings (for the graph)
const int maxDataPoints = 720; // Store 2-minute intervals for 24 hours (30 readings/hour * 24 hours = 720)
float temperatureData[maxDataPoints];
String timestampData[maxDataPoints]; // Array to store timestamps
int dataIndex = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("DHT22 (AM2302) Sensor Test");

    // Initialize LED pins
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    digitalWrite(GREEN_LED_PIN, LOW); // Ensure green LED is off initially
    digitalWrite(RED_LED_PIN, LOW);   // Ensure red LED is off initially

    dht.begin();

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(". ");
    }
  
    Serial.println(" ");
    Serial.println("Online");
    Serial.println(WiFi.localIP());
  
    // Define the route for the root URL
    server.on("/", handleRoot);

    // Start the server
    server.begin();
    Serial.println("HTTP server started");

    // Turn on the green LED for 30 seconds
    delay(30000);
    digitalWrite(GREEN_LED_PIN, HIGH);
    delay(30000); // Keep the green LED on for 30 seconds
    digitalWrite(GREEN_LED_PIN, LOW);

    // Initialize temperature data array
    for (int i = 0; i < maxDataPoints; i++) {
        temperatureData[i] = 0.0;
        timestampData[i] = ""; // Initialize timestamps
    }

    // Start the NTP client
    timeClient.begin();
}

void loop() {
    // Handle client requests
    server.handleClient();
  
    // Update NTP client to keep time synced
    timeClient.update();
  
    // Record temperature every 2 minutes (if it's time)
    static unsigned long lastRecordTime = 0;
    if (millis() - lastRecordTime >= 120000) { // 120000 ms = 2 minutes
        recordTemperature();
        lastRecordTime = millis();
    }
}

// Function to record the temperature for graphing
void recordTemperature() {
    float temperature = dht.readTemperature(); // Read temperature in Celsius
    if (!isnan(temperature)) {
        temperatureData[dataIndex] = temperature;

        // Store the current time in HH:MM:SS format
        timestampData[dataIndex] = timeClient.getFormattedTime();
        
        dataIndex = (dataIndex + 1) % maxDataPoints; // Wrap around if the index reaches max
    }
}

// Handle the root URL and display the temperature, humidity, etc.
void handleRoot() {
    // Reading temperature and humidity
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature(); // Celsius

    // Check if any reads failed
    if (isnan(humidity) || isnan(temperature)) {
        humidity = temperature = 0; // Set default values in case of error
    }

    // Prepare the HTML response
    String response = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
    response += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    response += "<title>Temperature, Humidity, and Time</title>";

    // Updated CSS for light and dark modes
    response += "<style>body { background-color: white; font-family: Arial, sans-serif; text-align: center; margin-top: 20px; transition: background-color 0.3s, color 0.3s;}";
    response += ".container { display: flex; justify-content: space-between; margin: 20px; }";
    response += ".data-box { background-color: #f2f2f2; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2); width: 30%; font-size: 1.5em; display: flex; justify-content: center; align-items: center; }";
    response += "#chart { width: 80%; height: 150px; margin: 20px auto; }";

    // Styling for buttons
    response += "button { background-color: #e0e0e0; color: black; border: none; padding: 10px 15px; border-radius: 5px; cursor: pointer; box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2); font-size: 1.2em; margin-left: 10px;}";
    response += "button:hover { background-color: #d0d0d0; }";
    response += ".small-button { padding: 5px 10px; font-size: 1.0em; }"; // Smaller button size for refresh
    response += "body.dark-mode { background-color: #1e1e1e; color: white; }";  // Dark mode styles
    response += ".dark-mode .data-box { background-color: #333; }";  // Dark mode for data boxes
    response += ".dark-mode button { background-color: #555; color: white; }";
    response += "</style>";

    response += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"; // Load Chart.js library

    // Dark mode toggle script
    response += "<script>function toggleDarkMode() { document.body.classList.toggle('dark-mode'); }</script>";
    
    // Dark mode toggle script
    response += "<script>function toggleDarkMode() {";
    response += "  document.body.classList.toggle('dark-mode');";
    response += "  localStorage.setItem('darkMode', document.body.classList.contains('dark-mode'));"; // Save the state
    response += "}";
    response += "window.onload = function() {"; // Check state on load
    response += "  if (localStorage.getItem('darkMode') === 'true') {";
    response += "    document.body.classList.add('dark-mode');";
    response += "  }";
    response += "  startTime();"; // Start the clock as well
    response += "};";
    response += "</script>";

    // Real-time clock script
    response += "<script>";
    response += "function startTime() {";
    response += "  var today = new Date();";
    response += "  var h = today.getHours();";
    response += "  var m = today.getMinutes();";
    response += "  var s = today.getSeconds();";
    response += "  m = checkTime(m);";
    response += "  s = checkTime(s);";
    response += "  document.getElementById('clock').innerHTML = h + ':' + m + ':' + s;";
    response += "  setTimeout(startTime, 1000);";
    response += "}";
    response += "function checkTime(i) { return (i < 10) ? '0' + i : i; }";  // Add zero in front of numbers < 10
    response += "window.onload = startTime;";  // Start clock when page loads
    response += "</script>";

    response += "</head><body>";

    // Temperature graph using Chart.js (placed above the text)
    response += "<canvas id='chart'></canvas>";
    response += "<script>";
    response += "const ctx = document.getElementById('chart').getContext('2d');";
    response += "const chart = new Chart(ctx, { type: 'line', data: {";
    response += "labels: ["; 

    // Add timestamps for the last 720 data points
    for (int i = 0; i < maxDataPoints; i++) {
        response += "'" + timestampData[(dataIndex + maxDataPoints - 720 + i) % maxDataPoints] + "'";
        if (i < maxDataPoints - 1) response += ",";
    }

    response += "], datasets: [{ label: 'Temperature (°C)', data: ["; 

    // Add the last 720 data points to the graph
    for (int i = 0; i < maxDataPoints; i++) {
        response += String(temperatureData[(dataIndex + maxDataPoints - 720 + i) % maxDataPoints]);
        if (i < maxDataPoints - 1) response += ",";
    }

    response += "], borderColor: 'rgba(75, 192, 192, 1)', borderWidth: 2 }] }, options: {";
    response += "scales: { y: { min: 0, max: 40, title: { display: true, text: 'Temperature (°C)' } },";
    response += "x: { title: { display: true, text: 'Time (HH:MM:SS)' } } } } });";
    response += "</script>";

   // Boxes for humidity and temperature, location text centered
    response += "<div class='container'>";
    response += "<div class='data-box'>Humidity: " + String(humidity) + " %</div>";
    response += "<div class='data-box'>Temperature: " + String(temperature) + " °C</div>";
    response += "<div class='data-box' style='text-align: center;'>Location: " + location + "</div>"; // Center location text
    response += "</div>";

    // Display real-time clock
    response += "<div class='data-box' id='clock' style='font-size: 2em; margin-top: 20px;'></div>";

    // Refresh button and dark mode button next to each other
    response += "<div style='margin-top: 20px;'><button onclick='location.reload();' class='small-button'>Refresh Data</button>";
    response += "<button onclick='toggleDarkMode();' class='small-button'>Dark Mode</button></div>"; // Dark mode toggle

    response += "</body></html>";

    // Send the response to the client
    server.send(200, "text/html;charset=utf-8", response);
}
