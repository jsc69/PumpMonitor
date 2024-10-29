struct RECEIVER {
  const char* urlTemplate;
  char* phoneNumber;
  char* apiKey;
};

// WIFI credentials
//const char* ssid = "WLAN-E9ED56";
//const char* password = "7513074084240857";
//const char* ssid = "TP-Link_D938";
//const char* password = "18447750";
const char* ssid = "FRITZ!Box 7520 XK";
const char* password = "33963578811096206111";

// Messenger templates
const char* SIGNAL = "https://signal.callmebot.com/signal/send.php?phone=%s&apikey=%s&text=%s";
const char* WHATSAPP = "https://api.callmebot.com/whatsapp.php?phone=%s&apikey=%s&text=%s";

const int numberOfReceivers = 1;

// +international_country_code + phone number
// Portugal +351, example: +351912345678
RECEIVER receivers[numberOfReceivers] = {
//  {WHATSAPP, "+4917668317706",""}, // Mama
  {WHATSAPP, "+4917642067679","7137227"} // Jörg
};
