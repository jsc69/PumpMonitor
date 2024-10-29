# PumpMonitor

PumpMonitor is a small project to monitor to drainage pumps. There is a main pump and a second pump which kicks in when the main pump do not work.
The pumps are monitored by two LEDs.
1. GREEN: main pump is powered
2. RED: second pump is powered

The ESP32 checks this LEDs and send a WhatsApp message if the second pump runs (red LED is on)
