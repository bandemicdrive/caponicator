#include "heltec.h"

// --- CONFIGURAZIONE RADIO ---
#define BAND            868.1E6
#define TX_POWER        3  //valore basso per fare test con schede in casa
#define SPREADING_FACTOR 10
#define BANDWIDTH       125E3
#define CODING_RATE     5

// --- PARAMETRI PROTOCOLLO ---
#define MAX_RETRIES     3
#define TIMEOUT_MS      2000
#define DEBOUNCE_MS     500

// --- VARIABILI DI STATO ---
byte localAddress = 0xAA;    
byte destination = 0xFF;     
byte msgCount = 0;
unsigned int discardedCount = 0; // Contatore pacchetti scartati

String lastPayload = "";
bool waitingForAck = false;
unsigned long lastSendTime = 0;
unsigned long lastButtonPress = 0;
int retryCounter = 0;
byte lastMsgId = 0;

void setup() {
  Heltec.begin(true, true, true, true, BAND);
  initLoRa();
  
  Heltec.display->init();
  Heltec.display->flipScreenVertically();
  Heltec.display->setFont(ArialMT_Plain_10);
  
  displayStatus("SISTEMA OK", "In ascolto...");
}

void loop() {
  parseRadio(LoRa.parsePacket());
  handleRetries();

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) processSerialInput(input);
  }

  if (digitalRead(0) == LOW && (millis() - lastButtonPress > DEBOUNCE_MS)) {
    startTransmission("Ping Tasto", destination);
    lastButtonPress = millis();
  }
}

void initLoRa() {
  LoRa.setTxPower(TX_POWER, RF_PACONFIG_PASELECT_RFO);
  LoRa.setSpreadingFactor(SPREADING_FACTOR);
  LoRa.setSignalBandwidth(BANDWIDTH);
  LoRa.setCodingRate4(CODING_RATE);
  LoRa.setSyncWord(0x12);
}

void processSerialInput(String input) {
  if (waitingForAck) return;
  int sep = input.indexOf(':');
  if (sep != -1) {
    String addrStr = input.substring(0, sep);
    String message = input.substring(sep + 1);
    destination = (byte) strtol(addrStr.c_str(), NULL, 16);
    startTransmission(message, destination);
  } else {
    startTransmission(input, destination);
  }
}

void startTransmission(String message, byte target) {
  lastPayload = message;
  lastMsgId = msgCount++;
  retryCounter = 0;
  destination = target;
  sendRawMessage(lastPayload, lastMsgId);
}

void sendRawMessage(String payload, byte id) {
  unsigned long startTX = millis();
  LoRa.beginPacket();
  LoRa.write(destination);
  LoRa.write(localAddress);
  LoRa.write(id);
  LoRa.write(0x01); 
  LoRa.write(payload.length());
  LoRa.print(payload);
  LoRa.endPacket();
  
  unsigned long duration = millis() - startTX;
  lastSendTime = millis();
  waitingForAck = (destination != 0xFF); 

  Serial.println(">> TX ToA: " + String(duration) + "ms");
  displayStatus("INVIATO", "ToA: " + String(duration) + "ms");
}

void parseRadio(int packetSize) {
  if (packetSize == 0) return;

  byte recipient = LoRa.read();
  byte sender = LoRa.read();
  byte incomingMsgId = LoRa.read();
  byte msgType = LoRa.read();
  byte incomingLength = LoRa.read();

  // LOGICA DI FILTRO E CONTATORE SCARTATI
  if (recipient != localAddress && recipient != 0xFF) {
    discardedCount++;
    Serial.print("!! Pacchetto scartato (Per: 0x");
    Serial.print(recipient, HEX);
    Serial.print(") - Totale: ");
    Serial.println(discardedCount);
    
    // Puliamo il buffer LoRa per sicurezza
    while (LoRa.available()) { LoRa.read(); }
    
    displayStatus("FILTRATO", "Dest: 0x" + String(recipient, HEX));
    return;
  }

  if (msgType == 0x02) { // ACK
    if (waitingForAck && incomingMsgId == lastMsgId) {
      waitingForAck = false;
      displayStatus("OK", "ACK da 0x" + String(sender, HEX));
    }
    return;
  }

  String incoming = "";
  while (LoRa.available()) { incoming += (char)LoRa.read(); }
  
  displayStatus("DA 0x" + String(sender, HEX), incoming);
  if (recipient != 0xFF) sendAck(sender, incomingMsgId);
}

void sendAck(byte toAddress, byte msgId) {
  delay(50); 
  LoRa.beginPacket();
  LoRa.write(toAddress);
  LoRa.write(localAddress);
  LoRa.write(msgId);
  LoRa.write(0x02);
  LoRa.write(0);
  LoRa.endPacket();
}

void handleRetries() {
  if (waitingForAck && (millis() - lastSendTime > TIMEOUT_MS)) {
    if (retryCounter < MAX_RETRIES) {
      retryCounter++;
      sendRawMessage(lastPayload, lastMsgId);
    } else {
      waitingForAck = false;
      displayStatus("ERRORE", "Nodo offline");
    }
  }
}

void displayStatus(String h, String b) {
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, h);
  Heltec.display->drawString(0, 15, b);
  Heltec.display->drawString(0, 35, "Scartati: " + String(discardedCount));
  Heltec.display->drawString(0, 50, "ID: 0x" + String(localAddress, HEX));
  Heltec.display->display();
}
