// Inclusion des bibliothèques nécessaires
#include <DFRobot_DHT20.h>
#include <Arduino.h>
#include <Servo.h>
#include <Nextion.h>
#include <EEPROM.h>
#include <doxygen.h>
#include <NexButton.h>
#include <NexCheckbox.h>
#include <NexConfig.h>
#include <NexCrop.h>
#include <NexDualStateButton.h>
#include <NexGauge.h>
#include <NexGpio.h>
#include <NexHardware.h>
#include <NexHotspot.h>
#include <NexNumber.h>
#include <NexObject.h>
#include <NexPage.h>
#include <NexPicture.h>
#include <NexProgressBar.h>
#include <NexRadio.h>
#include <NexRtc.h>
#include <NexScrolltext.h>
#include <NexSlider.h>
#include <NexText.h>
#include <NexTimer.h>
#include <Nextion.h>
#include <NexTouch.h>
#include <NexUpload.h>
#include <NexVariable.h>
#include <NexWaveform.h>

// Définition des broches pour les composants
#define LED1 11
#define POMPE 6
#define RESI 7
#define TRAPPE 9
#define LUMI A1
#define RAIN_PIN A2
#define ATOM_PIN 5
#define RELAIS 8

// Variables globales
Servo monServo; // Servo pour trappe
DFRobot_DHT20 dht20; // Capteur DHT20

// Variables de contrôle
int ouverture, chauffage, Lumi, pompage;
int thresholdValue = 20;
int valTemp, valLumi, valHumi, valHumidAir;

// Adresses EEPROM
int addrouv = 0;
int addrchauf = 1;

// Variables de temporisation
unsigned long ArrosageMillis = 0;
unsigned long BoutonMillis = 0;
unsigned long previousMillis2 = 0;
unsigned long previousMillis = 0;
unsigned long LEDMillis = 0;
const long interval = 5000;
const long intervalbouton = 60000;
const long interval2 = 10000;
const long intervalarrosage = 5000;
const long intervalLED = 3600000;

// Buffers pour affichage Nextion
char hTemp[10] = {0};
char buffer[100] = {0};

// Déclaration des objets Nextion
NexText t0(1, 5, "t0"); // Température
NexText t1(1, 6, "t1"); // Humidité sol
NexText t2(1, 7, "t2"); // Luminosité
NexDSButton bt0(1, 8, "bt0"); // Bouton LED
NexDSButton bt1(1, 9, "bt1"); // Bouton trappe
NexButton bUpdate(1, 10, "bUpdate"); // Bouton de mise à jour
NexButton b2(1, 3, "b2"); // Bouton arrosage manuel
NexSlider h0(1, 2, "h0"); // Slider pour intensité LED

// Liste des éléments à écouter (Nextion)
NexTouch *nex_listen_list[] = {&bt0, &bt1, &bUpdate, &b2, &h0, NULL};

// Fonction pour lire la luminosité
int getLuminosite() {
    int val = analogRead(LUMI); // Lecture du capteur de lumière
    val = val*6; // Conversion en une unité approximative
    utoa(val, hTemp, 10); // Conversion en string
    t2.setText(hTemp); // Affichage sur écran Nextion
    return val;
}

// Fonction pour lire l’humidité du sol
int getHumiditeSol() {
    int val = 1023 - analogRead(RAIN_PIN); // Inversion de la lecture
    val = (val * 10) / 102; // Échelle de 0 à 10
    utoa(val, hTemp, 10);
    t1.setText(hTemp);
    return val;
}

// Fonction pour lire la température et humidité de l'air
void getTemperatureHumidite(int &temp, int &humidAir) {
    temp = dht20.getTemperature(); // Température
    humidAir = dht20.getHumidity() * 100; // Humidité (%)
    utoa(temp, hTemp, 10);
    t0.setText(hTemp); // Affichage
}

// Callback pour bt0 (LED ON/OFF)
void bt0PopCallback(void *ptr) {
    uint32_t state;
    bt0.getValue(&state); // État du bouton
    digitalWrite(LED1, state ? HIGH : LOW); // LED ON/OFF
    LEDMillis = millis(); // Reset timer
}

// Callback pour bt1 (trappe)
void bt1PopCallback(void *ptr) {
    uint32_t state;
    bt1.getValue(&state);
    if(state==1){
        digitalWrite(RELAIS,HIGH);
        monServo.write(0); // Ouvrir trappe
        delay(500);
        BoutonMillis = millis();
        digitalWrite(RELAIS,LOW);
    }
    if(state==0){
        digitalWrite(RELAIS,HIGH);
        monServo.write(81); // Fermer trappe
        delay(500);
        BoutonMillis = millis();
        digitalWrite(RELAIS,LOW);
    }
    ouverture = state;
    EEPROM.write(addrouv, ouverture); // Sauvegarde état trappe
}

// Callback pour b2 (arrosage manuel)
void b2PopCallback(void *ptr) {
    digitalWrite(POMPE, HIGH);
    digitalWrite(ATOM_PIN, HIGH); // Active pompe + atomiseur
    ArrosageMillis = millis();
    pompage = 1;
}

// Callback slider LED
void h0PopCallback(void *ptr) {
    uint32_t number;
    h0.getValue(&number);
    analogWrite(LED1, (number * 255) / 100); // PWM LED
    LEDMillis = millis();
}

// Callback mise à jour des valeurs Nextion
void bUpdatePopCallback(void *ptr) {
    valLumi = getLuminosite();
    valHumi = getHumiditeSol();
    getTemperatureHumidite(valTemp, valHumidAir);
    utoa(valTemp, hTemp, 10);
    t0.setText(hTemp);
    utoa(valHumi, hTemp, 10);
    t1.setText(hTemp);
    utoa(valLumi, hTemp, 10);
    t2.setText(hTemp);
}

// Initialisation
void setup() {
    nexInit(); // Initialisation Nextion
    monServo.attach(TRAPPE);
    pinMode(LED1, OUTPUT);
    pinMode(RESI, OUTPUT);
    pinMode(ATOM_PIN, OUTPUT);
    pinMode(RELAIS, OUTPUT);
    pinMode(POMPE, OUTPUT);

    // Attache des callbacks
    bt0.attachPop(bt0PopCallback, &bt0);
    bt1.attachPop(bt1PopCallback, &bt1);
    b2.attachPop(b2PopCallback, &b2);
    h0.attachPop(h0PopCallback);
    bUpdate.attachPop(bUpdatePopCallback, &bUpdate);
    
    // Initialisation capteur DHT
    while (dht20.begin()) {
        t1.setText("pas init");
    }
    t1.setText("init good");
    
    ouverture = EEPROM.read(addrouv); // Lecture EEPROM
    chauffage = EEPROM.read(addrchauf);
    digitalWrite(RELAIS,LOW);
    digitalWrite(LED1,LOW);
    pompage = 0;
}

// Boucle principale
void loop() {
    nexLoop(nex_listen_list); // Écoute Nextion
    unsigned long currentMillis = millis(); // Temps actuel

    // Arrêt automatique de l’arrosage après interval
    if(currentMillis - ArrosageMillis >= intervalarrosage){
        digitalWrite(POMPE,LOW);
        digitalWrite(ATOM_PIN,LOW);
        pompage = 0;
    }

    // Extinction automatique des LED
    if (currentMillis - LEDMillis >= intervalLED){
        digitalWrite(LED1,LOW);
    }

    // Mise à jour capteurs
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        valLumi = getLuminosite();
        getTemperatureHumidite(valTemp, valHumidAir);
        valHumi = getHumiditeSol();
    }

    // Logique principale exécutée toutes les 10s
    if (currentMillis - previousMillis2 >= interval2) {
        previousMillis2 = currentMillis;

        // Contrôle trappe en fonction de l’humidité et température
        if (currentMillis - BoutonMillis >= intervalbouton){
            if(valHumidAir >= 70 && ouverture == 0){
                digitalWrite(RELAIS,LOW);
                delay(1000);
                monServo.write(0);
                ouverture = 1;
                EEPROM.write(addrouv, ouverture);
                digitalWrite(RELAIS,HIGH);
            } else if(valHumidAir <= 60 && ouverture == 0){
                if (valTemp >= 26 && ouverture == 0) {
                    digitalWrite(RELAIS,LOW);
                    delay(1000);
                    monServo.write(0);
                    ouverture = 1;
                    EEPROM.write(addrouv, ouverture);
                    digitalWrite(RELAIS,HIGH);
                } else if (valTemp <= 20 && ouverture == 1) {
                    digitalWrite(RELAIS,LOW);
                    delay(1000);
                    monServo.write(81);
                    ouverture = 0;
                    EEPROM.write(addrouv, ouverture);
                    digitalWrite(RELAIS,HIGH);
                }
            }

            // Gestion de la lumière selon la luminosité
            if(valLumi <= 1000) {
                analogWrite(LED1, 150);
                Lumi = 1;
            }
            if(valLumi <= 2500 && Lumi == 1){
                analogWrite(LED1, 150);
                Lumi = 2;
            }
            if (valLumi >= 5000){
                analogWrite(LED1, 0);
            }
            if (valLumi <= 200){
                analogWrite(LED1, 0);
            }
        }

        // Contrôle du chauffage
        if(valTemp <= 17 && chauffage == 0 ){ 
            digitalWrite(RESI,HIGH); 
            chauffage = 1;
            EEPROM.write(addrchauf, chauffage);
        } 

        if(valTemp >= 20 && chauffage == 1){ 
            digitalWrite(RESI,LOW);   
            chauffage = 0;
            EEPROM.write(addrchauf, chauffage);
        } 

        // Arrosage automatique si sol trop sec
        if (valHumi <= 21 && pompage == 0) {
            digitalWrite(POMPE,HIGH);
            digitalWrite(ATOM_PIN,HIGH);
            ArrosageMillis = currentMillis; 
        }

        // Protection contre dépassement millis()
        if (currentMillis < previousMillis){
            previousMillis = currentMillis;
        }
    }
}

