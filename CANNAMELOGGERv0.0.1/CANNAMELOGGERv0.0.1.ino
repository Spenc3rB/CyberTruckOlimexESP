#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
// Replace wth sqlite3.h from https://github.com/siara-cc/esp32-idf-sqlite3
#include <SQLite_ESP32.h>
// Replace with ESP32-EVB implimentation of CAN library
#include <CAN.h> 


// Replace with your network credentials
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";

// Create a database object
SQLiteDB db;
String dbPath = "/spiffs/can_data.db";

// Create a web server object
AsyncWebServer server(80);

// Function to initialize the SQLite database
void initDatabase() {
  if (!db.open(dbPath)) {
    Serial.println("Failed to open database");
    return;
  }
  
  db.exec("PRAGMA journal_mode=WAL;");
  
  String createTable = "CREATE TABLE IF NOT EXISTS can_packets (id INTEGER PRIMARY KEY AUTOINCREMENT, pgn INTEGER, spn INTEGER, data TEXT)";
  if (!db.exec(createTable)) {
    Serial.println("Failed to create table");
    return;
  }

  Serial.println("Database initialized with Write-Ahead Logging");
  db.close();
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Initialize the filesystem
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }

  // Initialize the CAN bus at 500 kbps
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    while (1);
  }
  Serial.println("CAN initialized");

  initDatabase();

  // Configure the web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/www/index.html", "text/html");
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/www/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/www/script.js", "application/javascript");
  });

  server.on("/query", HTTP_GET, handleQuery);
  server.begin();
}

void handleQuery(AsyncWebServerRequest *request) {
  String pgn = request->getParam("pgn")->value();
  String query = "SELECT * FROM can_packets WHERE pgn=" + pgn;

  if (request->hasParam("spn")) {
    String spn = request->getParam("spn")->value();
    query += " AND spn=" + spn;
  }
  
  if (db.open(dbPath)) {
    SQLiteQueryResult result = db.exec(query);
    String response = "[";
    bool first = true;
    
    while (result.next()) {
      if (!first) response += ",";
      response += "{";
      response += "\"id\":" + String(result.getInt(0)) + ",";
      response += "\"pgn\":" + String(result.getInt(1)) + ",";
      response += "\"spn\":" + String(result.getInt(2)) + ",";
      response += "\"data\":\"" + String(result.getString(3)) + "\"";
      response += "}";
      first = false;
    }
    
    response += "]";
    db.close();
    request->send(200, "application/json", response);
  } else {
    request->send(500, "text/plain", "Failed to open database");
  }
}

void loop() {
  int packetSize = CAN.parsePacket();
  if (packetSize) {
    long pgn = CAN.read(); // Modify this to extract PGN from the packet
    long spn = CAN.read(); // Modify this to extract SPN from the packet
    String data = ""; // Extract the rest of the data
    
    while (CAN.available()) {
      data += (char)CAN.read();
    }

    // Insert data into the database
    if (db.open(dbPath)) {
      String insertQuery = "INSERT INTO can_packets (pgn, spn, data) VALUES (" + String(pgn) + ", " + String(spn) + ", '" + data + "')";
      db.exec(insertQuery);
      db.close();
    }
  }
}
