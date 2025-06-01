#include "logger.h"
#include <time.h>
#include <WiFi.h>
#include <cstdarg>

Logger::Logger(const String &url, const String &device)
{
    logstash_url = url;
    device_name = device;
    debug_enabled = false;
    initialized = false;
    logstash_attempts = 0;
    logstash_successes = 0;
    logstash_failures = 0;
}

Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::initialize(const String &url, const String &device, bool enable_debug)
{
    Logger &logger = getInstance();
    if (!logger.initialized)
    {
        logger.setLogstashUrl(url);
        logger.setDeviceName(device);
        logger.begin(enable_debug);
        logger.initialized = true;

        // Initial connection test
        logger.testLogstashConnection();
    }
}

void Logger::begin(bool enable_debug)
{
    debug_enabled = enable_debug;

    Serial.println("=== LOGGER INITIALIZATION ===");
    Serial.println("Device: " + device_name);
    Serial.println("Logstash URL: " + logstash_url);
    Serial.println("Debug enabled: " + String(debug_enabled ? "YES" : "NO"));

    // Initialize NTP for proper timestamps
    Serial.println("Configuring NTP...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    // Wait a bit for NTP sync (non-blocking)
    Serial.println("Waiting for NTP sync (max 5 seconds)...");
    int ntp_wait = 0;
    while (time(nullptr) < 8 * 3600 * 2 && ntp_wait < 50)
    {
        delay(100);
        ntp_wait++;
        if (ntp_wait % 10 == 0)
        {
            Serial.print(".");
        }
    }
    Serial.println();

    time_t now = time(nullptr);
    if (now >= 8 * 3600 * 2)
    {
        Serial.println("NTP synchronized successfully");
        Serial.println("Current time: " + getISO8601Timestamp());
    }
    else
    {
        Serial.println("NTP sync failed, using millis() for timestamps");
    }

    Serial.println("Logger initialized successfully");
    Serial.println("==============================");
}

void Logger::setLogstashUrl(const String &url)
{
    logstash_url = url;
    if (debug_enabled)
    {
        Serial.println("DEBUG: Logstash URL updated to: " + url);
    }
}

void Logger::setDeviceName(const String &device)
{
    device_name = device;
    if (debug_enabled)
    {
        Serial.println("DEBUG: Device name updated to: " + device);
    }
}

String Logger::logLevelToString(LogLevel level)
{
    switch (level)
    {
    case DEBUG:
        return "DEBUG";
    case INFO:
        return "INFO";
    case WARNING:
        return "WARNING";
    case ERROR:
        return "ERROR";
    case CRITICAL:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

void Logger::sendToSerial(LogLevel level, const String &message)
{
    String timestamp = getCurrentTimestamp();
    Serial.println("[" + timestamp + "] [" + logLevelToString(level) + "] " + message);
}

bool Logger::sendToLogstash(LogLevel level, const String &message)
{
    logstash_attempts++;

    if (debug_enabled)
    {
        Serial.println("\n=== LOGSTASH SEND ATTEMPT #" + String(logstash_attempts) + " ===");
        Serial.println("Level: " + logLevelToString(level));
        Serial.println("Message: " + message);
    }

    // Step 1: Check WiFi connection
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("ERROR: WiFi not connected (status: " + String(WiFi.status()) + ")");
        if (debug_enabled)
        {
            Serial.println("WiFi status codes: WL_CONNECTED=3, WL_DISCONNECTED=6");
            Serial.println("Skipping Logstash transmission");
        }
        logstash_failures++;
        return false;
    }

    if (debug_enabled)
    {
        Serial.println("✓ WiFi connected");
        Serial.println("  SSID: " + WiFi.SSID());
        Serial.println("  IP: " + WiFi.localIP().toString());
        Serial.println("  RSSI: " + String(WiFi.RSSI()) + " dBm");
    }

    // Step 2: Initialize HTTP client
    HTTPClient http;
    if (debug_enabled)
    {
        Serial.println("✓ HTTPClient initialized");
    }

    // Step 3: Configure HTTP client
    http.setTimeout(10000);       // 10 second timeout
    http.setConnectTimeout(5000); // 5 second connection timeout

    if (debug_enabled)
    {
        Serial.println("✓ HTTP timeouts configured (connect: 5s, read: 10s)");
        Serial.println("Attempting to connect to: " + logstash_url);
    }

    bool connection_result = http.begin(logstash_url);
    if (!connection_result)
    {
        Serial.println("ERROR: Failed to initialize HTTP connection to " + logstash_url);
        logstash_failures++;
        return false;
    }

    if (debug_enabled)
    {
        Serial.println("✓ HTTP connection initialized");
    }

    // Step 4: Set headers
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32-Logger/1.0");
    http.addHeader("Accept", "*/*");
    http.addHeader("Connection", "close");

    if (debug_enabled)
    {
        Serial.println("✓ HTTP headers set");
    }

    // Step 5: Create JSON payload
    if (debug_enabled)
    {
        Serial.println("Creating JSON payload...");
    }

    DynamicJsonDocument doc(2048); // Increased size for debugging

    try
    {
        // Core logging fields
        doc["@timestamp"] = getISO8601Timestamp();
        doc["level"] = logLevelToString(level);
        doc["message"] = message;
        doc["device"] = device_name;

        // System information
        doc["uptime_ms"] = millis();
        doc["free_heap"] = ESP.getFreeHeap();
        doc["total_heap"] = ESP.getHeapSize();
        doc["min_free_heap"] = ESP.getMinFreeHeap();
        doc["max_alloc_heap"] = ESP.getMaxAllocHeap();

        // Memory usage percentage
        float memory_usage = ((float)(ESP.getHeapSize() - ESP.getFreeHeap()) / ESP.getHeapSize()) * 100;
        doc["memory_usage_percent"] = memory_usage;

        // Chip information
        doc["chip_id"] = String((uint32_t)ESP.getEfuseMac(), HEX);
        doc["chip_model"] = ESP.getChipModel();
        doc["chip_revision"] = ESP.getChipRevision();
        doc["cpu_freq_mhz"] = ESP.getCpuFreqMHz();

        // WiFi information
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["ip_address"] = WiFi.localIP().toString();
        doc["mac_address"] = WiFi.macAddress();
        doc["wifi_ssid"] = WiFi.SSID();
        doc["gateway_ip"] = WiFi.gatewayIP().toString();
        doc["subnet_mask"] = WiFi.subnetMask().toString();

        // Flash information
        doc["flash_chip_size"] = ESP.getFlashChipSize();
        doc["flash_chip_speed"] = ESP.getFlashChipSpeed();

        // SDK version
        doc["sdk_version"] = ESP.getSdkVersion();

        // Logger statistics
        doc["logger_attempts"] = logstash_attempts;
        doc["logger_successes"] = logstash_successes;
        doc["logger_failures"] = logstash_failures;
        doc["logger_success_rate"] = logstash_attempts > 0 ? ((float)logstash_successes / logstash_attempts * 100) : 0;
    }
    catch (const std::exception &e)
    {
        Serial.println("ERROR: Exception while creating JSON: " + String(e.what()));
        logstash_failures++;
        http.end();
        return false;
    }

    String jsonString;
    size_t json_size = serializeJson(doc, jsonString);

    if (debug_enabled)
    {
        Serial.println("✓ JSON created successfully");
        Serial.println("  JSON size: " + String(json_size) + " bytes");
        Serial.println("  Memory usage: " + String(doc.memoryUsage()) + " bytes");
        Serial.println("  JSON payload preview (first 200 chars):");
        Serial.println("  " + jsonString.substring(0, 200) + (jsonString.length() > 200 ? "..." : ""));
    }

    if (json_size == 0)
    {
        Serial.println("ERROR: JSON serialization failed (empty result)");
        logstash_failures++;
        http.end();
        return false;
    }

    if (json_size > 8192)
    { // 8KB limit
        Serial.println("WARNING: JSON payload very large (" + String(json_size) + " bytes)");
    }

    // Step 6: Send POST request
    if (debug_enabled)
    {
        Serial.println("Sending POST request...");
        Serial.println("  Payload size: " + String(jsonString.length()) + " bytes");
    }

    unsigned long start_time = millis();
    int httpResponseCode = http.POST(jsonString);
    unsigned long request_time = millis() - start_time;

    if (debug_enabled)
    {
        Serial.println("✓ POST request completed");
        Serial.println("  Response code: " + String(httpResponseCode));
        Serial.println("  Request time: " + String(request_time) + "ms");
    }

    // Step 7: Handle response
    String response = "";
    if (httpResponseCode > 0)
    {
        response = http.getString();
        if (debug_enabled && response.length() > 0)
        {
            Serial.println("  Response body: " + response.substring(0, 500) + (response.length() > 500 ? "..." : ""));
        }
    }
    else
    {
        if (debug_enabled)
        {
            Serial.println("  No response received (connection error)");
        }
    }

    bool success = (httpResponseCode >= 200 && httpResponseCode < 300);

    if (success)
    {
        logstash_successes++;
        if (debug_enabled)
        {
            Serial.println("✓ SUCCESS: Message sent to Logstash");
            Serial.println("  Success rate: " + String((float)logstash_successes / logstash_attempts * 100, 1) + "%");
        }
    }
    else
    {
        logstash_failures++;
        Serial.println("✗ FAILED: Logstash transmission failed");
        Serial.println("  HTTP Code: " + String(httpResponseCode));
        Serial.println("  Request time: " + String(request_time) + "ms");

        // Detailed error analysis
        if (httpResponseCode == -1)
        {
            Serial.println("  Error: Connection failed (check URL and network)");
        }
        else if (httpResponseCode == -11)
        {
            Serial.println("  Error: Timeout (check network latency)");
        }
        else if (httpResponseCode == 400)
        {
            Serial.println("  Error: Bad Request (check JSON format)");
        }
        else if (httpResponseCode == 404)
        {
            Serial.println("  Error: Not Found (check Logstash URL and port)");
        }
        else if (httpResponseCode == 500)
        {
            Serial.println("  Error: Server Error (check Logstash configuration)");
        }
        else if (httpResponseCode < 0)
        {
            Serial.println("  Error: Network/Connection error");
        }

        if (response.length() > 0)
        {
            Serial.println("  Server response: " + response);
        }

        Serial.println("  Failure rate: " + String((float)logstash_failures / logstash_attempts * 100, 1) + "%");
    }

    // Step 8: Cleanup
    http.end();

    if (debug_enabled)
    {
        Serial.println("✓ HTTP connection closed");
        Serial.println("=== END LOGSTASH ATTEMPT ===\n");
    }

    return success;
}

void Logger::testLogstashConnection()
{
    Serial.println("\n=== TESTING LOGSTASH CONNECTION ===");

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("ERROR: WiFi not connected - cannot test Logstash");
        return;
    }

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(logstash_url);

    Serial.println("Testing connection to: " + logstash_url);

    unsigned long start = millis();
    int httpResponseCode = http.GET(); // Simple GET request
    unsigned long duration = millis() - start;

    Serial.println("Test completed in " + String(duration) + "ms");
    Serial.println("Response code: " + String(httpResponseCode));

    if (httpResponseCode > 0)
    {
        String response = http.getString();
        Serial.println("Response: " + response.substring(0, 100) + (response.length() > 100 ? "..." : ""));

        if (httpResponseCode >= 200 && httpResponseCode < 300)
        {
            Serial.println("✓ Logstash appears to be reachable");
        }
        else
        {
            Serial.println("⚠ Logstash reachable but returned: " + String(httpResponseCode));
        }
    }
    else
    {
        Serial.println("✗ Cannot reach Logstash");
        Serial.println("Possible issues:");
        Serial.println("  - Wrong URL or port");
        Serial.println("  - Firewall blocking connection");
        Serial.println("  - Logstash not running");
        Serial.println("  - Network connectivity issues");
    }

    http.end();
    Serial.println("=== END CONNECTION TEST ===\n");
}

String Logger::getCurrentTimestamp()
{
    time_t now = time(nullptr);
    if (now < 8 * 3600 * 2)
    {
        // NTP not synced yet, use millis
        return String(millis() / 1000) + "s";
    }

    struct tm *timeinfo = localtime(&now);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
    return String(buffer);
}

String Logger::getISO8601Timestamp()
{
    time_t now = time(nullptr);
    if (now < 8 * 3600 * 2)
    {
        // NTP not synced yet, create a timestamp from millis
        unsigned long ms = millis();
        unsigned long seconds = ms / 1000;
        unsigned long milliseconds = ms % 1000;

        // Create a basic timestamp (this won't be accurate date/time, just relative)
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "1970-01-01T00:%02lu:%02lu.%03luZ",
                 (seconds / 60) % 60, seconds % 60, milliseconds);
        return String(buffer);
    }

    struct tm *timeinfo = gmtime(&now);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000Z", timeinfo);
    return String(buffer);
}

void Logger::log(LogLevel level, const String &message)
{
    sendToSerial(level, message);

    // Add delay between serial and logstash to avoid conflicts
    if (debug_enabled)
    {
        delay(10);
    }

    if (!logstash_url.isEmpty())
    {
        sendToLogstash(level, message);
    }
}

void Logger::printStatistics()
{
    Serial.println("\n=== LOGGER STATISTICS ===");
    Serial.println("Total attempts: " + String(logstash_attempts));
    Serial.println("Successes: " + String(logstash_successes));
    Serial.println("Failures: " + String(logstash_failures));
    if (logstash_attempts > 0)
    {
        float success_rate = (float)logstash_successes / logstash_attempts * 100;
        Serial.println("Success rate: " + String(success_rate, 1) + "%");
    }
    Serial.println("========================\n");
}

// ... rest of the methods remain the same (debug, info, warning, error, critical, etc.)
void Logger::debug(const String &message)
{
    if (debug_enabled)
    {
        log(DEBUG, message);
    }
}

void Logger::info(const String &message)
{
    log(INFO, message);
}

void Logger::warning(const String &message)
{
    log(WARNING, message);
}

void Logger::error(const String &message)
{
    log(ERROR, message);
}

void Logger::critical(const String &message)
{
    log(CRITICAL, message);
}

// System monitoring method
void Logger::logSystemStats()
{
    DynamicJsonDocument stats(512);
    stats["free_heap"] = ESP.getFreeHeap();
    stats["total_heap"] = ESP.getHeapSize();
    stats["min_free_heap"] = ESP.getMinFreeHeap();
    stats["max_alloc_heap"] = ESP.getMaxAllocHeap();
    stats["uptime_minutes"] = millis() / 60000;
    stats["cpu_freq_mhz"] = ESP.getCpuFreqMHz();

    if (WiFi.status() == WL_CONNECTED)
    {
        stats["wifi_rssi"] = WiFi.RSSI();
        stats["wifi_connected"] = true;
    }
    else
    {
        stats["wifi_connected"] = false;
    }

    String statsJson;
    serializeJson(stats, statsJson);

    info("System stats: " + statsJson);
}

// Connection test method
bool Logger::isLogstashConnected()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    HTTPClient http;
    http.setTimeout(3000);
    http.begin(logstash_url);

    int httpResponseCode = http.GET();
    http.end();

    // Logstash HTTP input typically returns 200 even for GET
    return (httpResponseCode > 0 && httpResponseCode < 400);
}