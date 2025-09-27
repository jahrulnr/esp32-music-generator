#include <app/setup.h>

Services::WeatherService* weatherService = nullptr;

void weatherCallback(const Services::WeatherService::WeatherData &data, bool success);

void setupWeather(){
	if (weatherService) return;

	Services::WeatherService::WeatherConfig cfg;
	cfg.adm4Code = "31.71.03.1001"; // Kemayoran, Jakarta Pusat
	cfg.cacheExpiryMinutes = 60;

	weatherService = new Services::WeatherService(fileManager);
	weatherService->init(cfg);

	if (WiFi.status() == WL_CONNECTED) {
		weatherService->getCurrentWeather(weatherCallback, true);
	}
}

void weatherCallback(const Services::WeatherService::WeatherData &data, bool success){
	static const char* TAG = "weatherCallback";

	if (success) {
		ESP_LOGI(TAG, "Location=%s, Temperature=%d°C", data.location.c_str(), data.temperature);

		// Update display with weather data
		if (display) {
			display->updateWeatherData(data);
		}
	} else {
		ESP_LOGE(TAG, "Failed to retrieve weather data");
	}
}

void weatherHandler() {
	static long needUpdate = millis();
	const long updateFrequency = 60000;
	const char* TAG = "weatherTask";

	if (WiFi.status() != WL_CONNECTED)
		return;

	if(millis() > needUpdate) {
		weatherService->getCurrentWeather(weatherCallback, false);
		needUpdate = millis() + updateFrequency;
	}
}