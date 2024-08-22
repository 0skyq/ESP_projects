extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "Firebase_ESP_Client.h"
#include "driver/adc.h"
}

#include <string>

#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define API_KEY ""
#define DATABASE_URL ""

#define NUM_LEDS 6
#define NUM_DETECTORS 3
#define NUM_OF_SAMPLES 2000

class WiFiConnection {
public:
    void setup() {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifi_config_t wifi_config = {};
        strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI("WiFi", "Connecting to WiFi...");
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
};

class FirebaseConnection {
private:
    FirebaseData fbdo;
    FirebaseAuth auth;
    FirebaseConfig config;
    bool signupOK = false;

public:
    void setup() {
        config.api_key = API_KEY;
        config.database_url = DATABASE_URL;
        if (Firebase.signUp(&config, &auth, "", "")) {
            ESP_LOGI("Firebase", "Signup OK");
            signupOK = true;
        } else {
            ESP_LOGE("Firebase", "Signup failed: %s", config.signer.signupError.message.c_str());
        }
        config.token_status_callback = tokenStatusCallback;
        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);
    }

    FirebaseData& getFirebaseData() {
        return fbdo;
    }
};

class SensorManager {
private:
    const int ledPins[NUM_LEDS] = {23, 22, 21, 19, 18, 5};
    const int detectorPins[NUM_DETECTORS] = {34, 35, 32};
    float detectorValue[NUM_OF_SAMPLES][NUM_LEDS];
    int pulse_width = 3;
    int delay_bet_LEDs = 3;

public:
    void setup() {
        for (int i = 0; i < NUM_LEDS; i++) {
            gpio_pad_select_gpio(ledPins[i]);
            gpio_set_direction((gpio_num_t)ledPins[i], GPIO_MODE_OUTPUT);
        }
        resetDetectorValues();
    }

    void readSensors(int y) {
        ESP_LOGI("Sensor", "Sensor reading....");
        for (int n = 0; n < y; n++) {
            ESP_LOGI("Sensor", "sample number : %d", n);
            for (int ledIndex = 0; ledIndex < NUM_LEDS; ledIndex++) {
                for (int i = 0; i < NUM_LEDS; i++) {
                    gpio_set_level((gpio_num_t)ledPins[i], 0);
                    vTaskDelay(delay_bet_LEDs / portTICK_PERIOD_MS);
                }
                gpio_set_level((gpio_num_t)ledPins[ledIndex], 1);
                vTaskDelay(pulse_width / portTICK_PERIOD_MS);
                int detectorIndex = ledIndex / 2;
                detectorValue[n][ledIndex] = 0.25 * adc1_get_raw((adc1_channel_t)detectorPins[detectorIndex]);
            }
            ESP_LOGI("Sensor", "[%f,%f,%f,%f,%f,%f]",
                     detectorValue[n][0], detectorValue[n][1], detectorValue[n][2],
                     detectorValue[n][3], detectorValue[n][4], detectorValue[n][5]);
        }
    }

    void resetDetectorValues() {
        for (int i = 0; i < NUM_OF_SAMPLES; i++) {
            for (int j = 0; j < NUM_LEDS; j++) {
                detectorValue[i][j] = 0;
            }
        }
    }

    float getDetectorValue(int sampleIndex, int ledIndex) const {
        return detectorValue[sampleIndex][ledIndex];
    }
};

class DataTransmitter {
private:
    FirebaseConnection& firebaseConnection;
    SensorManager& sensorManager;
    std::string parentPath;
    std::string testPath = "/testData";
    FirebaseJsonArray arr;
    unsigned long sendDataPrevMillis = 0;
    unsigned long timerDelay = 1000;
    int num = 0;
    bool Start = false;
    int Num_of_samples = 0;

public:
    DataTransmitter(FirebaseConnection& fbConn, SensorManager& sensorMgr)
        : firebaseConnection(fbConn), sensorManager(sensorMgr) {}

    void transmitData() {
        FirebaseData& fbdo = firebaseConnection.getFirebaseData();

        if (Firebase.ready() && (esp_log_timestamp() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
            sendDataPrevMillis = esp_log_timestamp();

            if (Firebase.RTDB.getString(&fbdo, "/Start/Details/Number")) {
                if (fbdo.dataType() == "int") {
                    Num_of_samples = fbdo.intData();
                }
            }

            if (Firebase.RTDB.getBool(&fbdo, "/Start/Digital/")) {
                if (fbdo.dataType() == "boolean") {
                    Start = fbdo.boolData();
                }
            }

            if (Start == 1 && num < Num_of_samples) {
                ESP_LOGI("DataTransmitter", "Number of samples: %d", Num_of_samples);
                Firebase.RTDB.setString(&fbdo, "Start/Transmission", "Ongoing");

                sensorManager.readSensors(Num_of_samples);
                ESP_LOGI("DataTransmitter", "-----Readings DONE ---------");

                for (int i = 0; i < Num_of_samples; i++) {
                    parentPath = testPath + "/" + std::to_string(num);
                    arr.add(sensorManager.getDetectorValue(i, 0), sensorManager.getDetectorValue(i, 1),
                            sensorManager.getDetectorValue(i, 2), sensorManager.getDetectorValue(i, 3),
                            sensorManager.getDetectorValue(i, 4), sensorManager.getDetectorValue(i, 5));
                    ESP_LOGI("DataTransmitter", "Set Jsonarray... %s",
                             Firebase.RTDB.setArray(&fbdo, parentPath.c_str(), &arr) ? "ok" : fbdo.errorReason().c_str());

                    num++;
                    arr.clear();
                }
            }

            if (Num_of_samples == num) {
                Firebase.RTDB.setBool(&fbdo, "Start/Digital", false);
                Firebase.RTDB.setString(&fbdo, "Start/Transmission", "Done");
                ESP_LOGI("DataTransmitter", "Transmission Done");
                num = 0;
                for (int i = 0; i < NUM_LEDS; i++) {
                    gpio_set_level((gpio_num_t)sensorManager.getLedPin(i), 0);
                }
                sensorManager.resetDetectorValues();
            }
        }

        if (esp_wifi_get_state() != WIFI_STATE_STA) {
            ESP_LOGI("WiFi", "Reconnecting to WiFi...");
            WiFiConnection wifiConn;
            wifiConn.setup();
        }
    }
};

extern "C" void app_main(void) {
    WiFiConnection wifiConnection;
    FirebaseConnection firebaseConnection;
    SensorManager sensorManager;
    DataTransmitter dataTransmitter(firebaseConnection, sensorManager);

    wifiConnection.setup();
    firebaseConnection.setup();
    sensorManager.setup();

    while (true) {
        dataTransmitter.transmitData();
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Small delay to yield control back to the FreeRTOS scheduler
    }
}
