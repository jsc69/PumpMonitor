struct RECEIVER {
  const char* urlTemplate;
  char* phoneNumber;
  char* apiKey;
};

// WIFI credentials
const char* ssid = "SSID";
const char* password = "PASSWORD";

// Messenger templates
const char* SIGNAL = "https://signal.callmebot.com/signal/send.php?phone=%s&apikey=%s&text=%s";
const char* WHATSAPP = "https://api.callmebot.com/whatsapp.php?phone=%s&apikey=%s&text=%s";

const int numberOfReceivers = 1;

// +international_country_code + phone number
// Portugal +351, example: +351912345678
RECEIVER receivers[numberOfReceivers] = {
  {WHATSAPP, "PHONENUMBER","CallMeBot-API"}
};
