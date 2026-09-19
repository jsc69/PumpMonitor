#pragma once
// --- WIFI ---
const char* ssid = "SSID";
const char* password = "PASSWORD";

const char* ota_user = "OTA_USER";
const char* ota_pass = "OTA_PASSWORD";

// --- E-Mail Zugangsdaten ---
const char* SMTP_HOST = "smtp.gmail.com";
const uint16_t SMTP_PORT = 465;
const char* IMAP_HOST = "imap.gmail.com";
const uint16_t IMAP_PORT = 993;
const char* AUTHOR_EMAIL = "nobody@gmail.com";
const char* AUTHOR_PASSWORD = "dein-app-passwort";
const char* email_recipients[] = {
  "somebody@gmail.com"
};

// Konfiguration WhatsApp
const char* whatsapp_recipients[] = {
  "+49xxxxxxx"
};
String apiKey = "xxxxxx";
