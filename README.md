# Smart_Indoor_Farming_System

This project uses an Arduino ESP32-S3 and Blynk. It reads sensor values for soil and light, then turns on a pump or an LED light based on set numbers. Than send the values to blynk for real time monitoring. In blynk you have much control, you can choose a plant type to auto-set the numbers or set them manually. you can also choose manually to switch on the water or light, as well as reset values or reset wifi credentials.

Code Overview
The code saves WiFi and Blynk settings. If none are saved, it creates an AP for easy setup.

Sensor Readings:
It reads soil moisture and light.

Control Logic:

The pump runs if the soil moisture is below the set number.

The LED runs based on light status and daily light time.

The data gathered is sent every 10 seconds to blynk.

When a plant is chosen in blynk, the set numbers update automatically.

