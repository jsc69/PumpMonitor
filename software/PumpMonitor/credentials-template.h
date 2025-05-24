struct RECEIVER{
	const char* urlTemplate;
	const char* phoneNumber;  
	const char* apiKey;
};

// WIFI credentials
const char* ssid = "<YOUR_SSID>";
const char* password = "<YOUR_PASSWORD>";

// Messenger templates
const char* WHATSAPP = "https://api.callmebot.com/whatsapp.php?phone=%s&apikey=%s&text=%s";
const int numberOfReceivers = 2;

// Add phonenumber and callmebot-apikey
// for callmebot-apikey see https://www.callmebot.com/blog/free-api-whatsapp-messages/
RECEIVER receivers[numberOfReceivers] = {
  {WHATSAPP, "<PHONENUMBER1>","<APIKEY1>"}, 
  {WHATSAPP, "<PHONENUMBER2>","<APIKEY2>"}
};
