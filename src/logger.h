#ifndef LOGGER_H
#define LOGGER_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <cstdarg>

enum LogLevel
{
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

class Logger
{
private:
    String logstash_url;
    String device_name;
    bool debug_enabled;
    bool initialized;

    // Statistics tracking
    int logstash_attempts;
    int logstash_successes;
    int logstash_failures;

    // Private constructor for singleton
    Logger(const String &url = "", const String &device = "ESP32-CAM");

    // Delete copy constructor and assignment operator
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    // Private helper methods
    String logLevelToString(LogLevel level);
    void sendToSerial(LogLevel level, const String &message);
    bool sendToLogstash(LogLevel level, const String &message);
    String getCurrentTimestamp();
    String getISO8601Timestamp();
    void begin(bool enable_debug = true);
    void setLogstashUrl(const String &url);
    void setDeviceName(const String &device);
    void log(LogLevel level, const String &message);

public:
    // Singleton instance getter
    static Logger &getInstance();

    // Static initialization method
    static void initialize(const String &url = "",
                           const String &device = "ESP32-CAM",
                           bool enable_debug = true);

    // Public logging methods
    void debug(const String &message);
    void info(const String &message);
    void warning(const String &message);
    void error(const String &message);
    void critical(const String &message);

    // Utility methods
    void testLogstashConnection();
    void printStatistics();
    void logSystemStats();
    bool isLogstashConnected();
};

#endif