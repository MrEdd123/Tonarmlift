#pragma once
#include <WebServer.h>
#include "Tonarmlift.h"

/// @brief Webinterface zur Steuerung des Tonarmlifts über WLAN.
///        Der ESP32 startet als Access Point – mit einem Browser verbindet
///        man sich mit der IP 192.168.4.1 und kann den Lift steuern.
class WebInterface {
public:
    /// @brief Konstruktor
    /// @param lift        Referenz auf das Tonarmlift-Objekt
    /// @param ssid        WLAN-AP-SSID (default: "Tonarmlift-AP")
    /// @param password    WLAN-Passwort (default: "12345678")
    WebInterface(Tonarmlift& lift,
                 const char* ssid = "Tonarmlift-AP",
                 const char* password = "12345678");

    /// @brief Startet den WLAN-Access-Point und den Webserver
    void begin();

    /// @brief Muss in loop() aufgerufen werden – bearbeitet eingehende Requests
    void handleClient();

private:
    Tonarmlift& _lift;
    WebServer _server;
    const char* _ssid;
    const char* _password;

    // --- Hilfsfunktionen ---
    String _buildStatusJson();
    String _buildHtmlPage();

    // --- HTTP-Route-Handler ---
    void _handleRoot();
    void _handleStatus();
    void _handleToggle();
    void _handleJogStart();
    void _handleJogStop();
    void _handleMoveTop();
    void _handleMoveBottom();
    void _handleSetTop();
    void _handleSetBottom();
    void _handleMove();
    void _handleNotFound();
};