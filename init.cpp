/*

*/

void setup() {
  // --- STATO: INIT ---
  Heltec.begin(true, true, true, true, BAND);
  currentState = ST_IDLE;

  bool sessionActive = true;
  while (sessionActive) {
    switch (currentState) {
      case ST_IDLE:          /* Gestione Trigger 3 e 4 */ break;
      case ST_TX:            /* Gestione Trigger 5 */     break;
      case ST_WAIT_ACK:      /* Gestione Trigger 6 e 7 */ break;
      case ST_RX_HANDLING:   /* Gestione Trigger 10 */    break;
      case ST_RETRY:         /* Gestione Trigger 8 e 9 */ break;
      
      case ST_SLEEP:
        enterDeepSleep();    /* Trigger 11 */
        sessionActive = false;
        break;      
    }
  }
}
