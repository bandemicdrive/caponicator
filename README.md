<HTML>
  # caponicator
transmission experiments 

..mmoOomm....mmoOomm...mmoOomm....mmoOomm....mmoOomm....mmoOomm....mmoOomm....mmoOomm...
ok ma devo capire come si usa

<img src="diagramma stati pasquale.png">

Per come abbiamo scritto codice quando si sveglia inizializza tutto hardware compreso lora e suo buffer.

quindi se poi ritorna subito in sleep non riuscira' mai a processare un messaggio 
dobbiamo conservare buffer lora  al riavvio da deepsleep
propongo check del tipo di awake che se e' da rx lora processa subito buffer prima di lora.begin

 uso di preambolo al messaggio adatto al duty cycle del SX1262 (radio) in deep sleep
 oppure usare altro sistema...argomento interessante

pensare a menu settaggio radio comandabile da bt e seriale per non dover flashare scheda ogni 2 minuti. 

pensare a piccola gita al mare o magari weekend veloce 3/4 giorni con leandro da bora…..valutando situazione scuola….magari ci festeggiamo compleanno insieme..



