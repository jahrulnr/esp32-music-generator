#pragma once

#include <FTPServer.h>
#include <Notification.h>
#include <AnalogMicrophone.h>
#include <I2SSpeaker.h>
#include <FileManager.h>
#include "display/Display.h"
#include "sensors/TouchDetector.h"
#include "services/WiFiService.h"

extern FTPServer ftpSrv;
extern Notification* notification;
extern Services::WiFiService *wifiService;
extern Utils::FileManager *fileManager;

extern AnalogMicrophone* microphone;
extern I2SSpeaker *i2sSpeaker;
extern Display::Display* display;
extern Sensors::TouchDetector* touchSensor;

extern Services::WeatherService* weatherService;

void setupFilemanager();
void setupWiFi();
void setupFTPServer();
void setupNotification();
void setupMicrophone();
void setupSpeakers();
void setupDisplay();
void setupTouch();

void setupWeather();
void weatherHandler();

bool startLofiStreaming();
void stopLofiStreaming();
bool isLofiStreaming();