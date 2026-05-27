#include <WiFi.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>


const char* ssid     = "Totalplay-2.4G-80c8";
const char* password = "ugThYJ83k23FFSFY";
const char* serverIP = "192.168.100.27"; // IP de la compu del server

const int   port     = 5001;
const int   LED      = 15;
const int SENSOR =34;

SocketIOclient socketIO;


void socketIOEvent(socketIOmessageType_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case sIOtype_CONNECT:
            Serial.println("Conectado al server");
            socketIO.send(sIOtype_EVENT, "[\"arduino_conectado\",\"ok\"]");
            break;

        case sIOtype_EVENT: {
            String msg = String((char*)payload);
            Serial.println("Evento: " + msg);

            msg.toUpperCase();
            if (msg.indexOf("PRENDER") >= 0 || msg.indexOf("ENCENDER") >= 0) {
                digitalWrite(LED, HIGH);
                Serial.println("LED ON");
            } else if (msg.indexOf("APAGAR") >= 0) {
                digitalWrite(LED, LOW);
                Serial.println("LED OFF");
            }
            break;
        }

        case sIOtype_DISCONNECT:
            Serial.println("Desconectado del server");
            break;

        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED, OUTPUT);

    WiFi.begin(ssid, password);
    Serial.print("Conectando a WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado: " + WiFi.localIP().toString());

    socketIO.begin(serverIP, port, "/socket.io/?EIO=4");
    socketIO.onEvent(socketIOEvent);
}

void loop() {
    socketIO.loop();

    static unsigned long lastTime = 0;
    if (millis() - lastTime > 500) {
        lastTime = millis();
        int valor = analogRead(SENSOR);
        String evento = "[\"sensor\"," + String(valor) + "]";
        socketIO.send(sIOtype_EVENT, evento);
        Serial.println("Sensor: " + String(valor));
    }
}