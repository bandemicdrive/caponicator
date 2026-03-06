#include "heltec.h"

/* * =================================================================
 * LEGENDA STATECHART (LOGICA DI TRANSIZIONE)
 * =================================================================
 * ST_INIT         -> Avvio: Inizializza HW (LoRa, OLED, S3)
 * ST_IDLE         -> Attesa: Monitora eventi (Timeout o RX)
 * ST_TX           -> Invio: Incapsula e trasmette pacchetto DATA
 * ST_RX_CHECK     -> Verifica: Controllo integrità CRC del buffer
 * ST_RX_PROCESS   -> Elaborazione: Legge dati validi e risponde ACK
 * ST_ERROR_MGMT   -> Gestione: Log errori (CRC/Timeout) e Retry
 * ST_SLEEP        -> Fine: Spegnimento HW e attivazione Timer RTC
 * =================================================================
 */
// --- COSTANTI FISICHE E DI PROTOCOLLO ---
#define BAND    868E6       // Frequenza operativa (Centrale 868MHz, SF7, BW 125kHz di default)
#define TIMEOUT_ACK 3000    // Finestra di ricezione (RX Window) post-trasmissione
#define SLEEP_SEC   30      // Intervallo di power-down (Deep Sleep)


// --- MEMORIA PERSISTENTE (RTC FAST MEMORY) ---
// Variabili mappate nella SRAM del controller RTC. Non vengono azzerate durante il reset 
// software causato dal Deep Sleep (Power Domain: RTC_PERIPH).
RTC_DATA_ATTR int msgCount = 0;
RTC_DATA_ATTR int crcErrorCount = 0;

// --- DEFINIZIONE DEGLI STATI (Enum Typedef) ---
enum State { ST_INIT, ST_IDLE, ST_TX, ST_RX_CHECK, ST_RX_PROCESS, ST_ERROR_MGMT, ST_SLEEP };
State currentState = ST_INIT;

void setup() {  // [Trigger: Power On / Wakeup]
  /* * Inizializzazione del System-on-Chip (SoC). 
     * Il metodo begin() configura il multiplexing dei pin (IOMUX), inizializza il bus SPI 
     * dedicato al LoRa e il bus I2C per l'OLED SSD1306.
   */
  Heltec.begin(true /*Display*/, true /*LoRa*/, true /*Serial*/, true /*PABoost*/, BAND);
  Heltec.display->setFont(ArialMT_Plain_10);

  bool sessionActive = true;
  unsigned long startWait = 0;
  int retryCount = 0;

  while (sessionActive) {
    switch (currentState) {

      case ST_INIT:
        // Transizione: INIT -> IDLE (Hardware pronto)
        Serial.println("System Initialized");
        currentState = ST_IDLE;
        break;

      case ST_IDLE:
        // Transizione: IDLE -> TX (Dopo breve delay o condizione)
        // Transizione: IDLE -> RX_CHECK (Se parsePacket > 0)
        /*
         * Polling del flag IRQ del modulo LoRa.
         * parsePacket() interroga i registri del chip radio per verificare la 
         * presenza di valid preamble e header nel buffer FIFO.
         */
        if (LoRa.parsePacket()) {
          currentState = ST_RX_CHECK;
        } else if (millis() > 2000) { 
          currentState = ST_TX;
        }
        break;

      case ST_TX:
        // Transizione: TX -> RX_CHECK (Attesa immediata risposta dopo invio)
        /* * Encapsulation dei dati. Il payload viene caricato nel buffer FIFO dell'SX1262.
         * endPacket() attiva il Power Amplifier (PA) e modula il segnale.
         */
        updateDisplay("TRANSMITTING", "Msg #" + String(msgCount));
        LoRa.beginPacket();
        LoRa.print("PING_"); LoRa.print(msgCount);
        LoRa.endPacket();
        
        startWait = millis();
        currentState = ST_RX_CHECK; 
        break;

      case ST_RX_CHECK:
        // Transizione: RX_CHECK -> ERROR_MGMT (Se CRC fallito)
        // Transizione: RX_CHECK -> RX_PROCESS (Se pacchetto integro)
        if (LoRa.parsePacket()) {
          if (LoRa.crcError()) {
            currentState = ST_ERROR_MGMT;
          } else {
            currentState = ST_RX_PROCESS;
          }
        } else if (millis() - startWait > TIMEOUT_ACK) {
          // Timeout: Nessun downlink rilevato entro la finestra temporale definita.
          currentState = ST_ERROR_MGMT; // Timeout treat as error
        }
        break;

            case ST_RX_PROCESS:
                // Gestione del payload ricevuto (Data Link Layer)
                {
                    String rx = "";
                    while (LoRa.available()) rx += (char)LoRa.read(); 
                    
                    // RSSI (Received Signal Strength Indicator) e SNR (Signal-to-Noise Ratio)
                    // sono metadati fondamentali per il debugging del link radio.
                    updateDisplay("MODE: RX_DATA", "RSSI: " + String(LoRa.packetRssi()));
                    
                    if (rx == "ACK") {
                        msgCount++; 
                    }
                    delay(500);  
                    currentState = ST_SLEEP; 
                }
                break;

            case ST_ERROR_MGMT:
                /* * Logica di ritrasmissione (ARQ - Automatic Repeat Request).
                 * Incremento degli errori CRC salvati in RTC per analisi della stabilità del canale.
                 */
                if (LoRa.crcError()) crcErrorCount++; 
                
                if (retryCount < 3) {
                    retryCount++; 
                    updateDisplay("RETRY_LOGIC", "Counter: " + String(retryCount));
                    currentState = ST_TX; 
                } else {
                    currentState = ST_SLEEP;
                }
                break;

            case ST_SLEEP:
                /*
                 * Preparazione allo stato Power-Down.
                 * 1. LoRa.sleep(): Imposta l'SX1262 in modalità 'Cold Start' (Consumo ~600nA).
                 * 2. displayOff(): Spegne il controller SSD1306 e interrompe il VCC dell'OLED.
                 * 3. esp_deep_sleep_start(): Isola i core della CPU e attiva l'ULP (Ultra Low Power co-processor).
                 */
                updateDisplay("SYSTEM_SUSPEND", "Timer: " + String(SLEEP_SEC) + "s");
                delay(200); 
                
                sessionActive = false; 
                
                LoRa.sleep();                      
                Heltec.display->displayOff();      
                esp_sleep_enable_timer_wakeup(SLEEP_SEC * 1000000ULL); 
                esp_deep_sleep_start();            
                break;
        }
    }
}

void loop() {
    // Dead Code: L'entry point è gestito esclusivamente dal Reset Vector post-DeepSleep.
}

void updateDisplay(String header, String info) {
    // Gestione dell'interfaccia utente tramite bus I2C.
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, header);
    Heltec.display->drawString(0, 15, info);
    Heltec.display->drawString(0, 35, "CRC_ERR_REG: " + String(crcErrorCount));
    Heltec.display->display();
}
