// ===== VINHERIA AGNELLO - CHECKPOINT 02 =====
// Monitoramento de temperatura, umidade e luminosidade da adega

//adicionando as bibliotecas
#include <Wire.h>              //comunicação I2C (o LCD e o RTC usam os mesmos fios SDA e SCL)
#include <LiquidCrystal_I2C.h> //LCD com módulo I2C
#include <DHT.h>               //sensor de temperatura e umidade
#include <RTClib.h>            //relógio RTC
#include <EEPROM.h>            //memória que não apaga quando o Arduino desliga

// -- componentes --
LiquidCrystal_I2C LCD(0x27, 16, 2); //endereço I2C do LCD, 16 colunas e 2 linhas
DHT dht(2, DHT22);                  //DHT22 ligado no pino digital 2
RTC_DS1307 rtc;

// -- pinos --
int LDR = A0;
int LED_VERDE = 8;
int LED_AMARELO = 9;
int LED_VERMELHO = 10;
int buzzer = 12;

// -- calibração do LDR (valores medidos no Wokwi) --
int ldrEscuro = 1015; //leitura com a luz no mínimo
int ldrClaro = 8;     //leitura com a luz no máximo

// -- limites --
//temperatura: ideal de 13°C, aceitando variação de 3°C
float tempOkMin = 10, tempOkMax = 16;
float tempAlertaMin = 8, tempAlertaMax = 18;
//umidade: ideal entre 60% e 80%
float umidOkMin = 60, umidOkMax = 80;
float umidAlertaMin = 50, umidAlertaMax = 90;
//luminosidade: mesmos limites do checkpoint 1
int limiteAlerta = 30;
int limiteProblema = 70;

// -- status: 0 = OK, 1 = alerta, 2 = problema --
int statusTemp = 0, statusUmid = 0, statusLuz = 0, statusGeral = 0;

// -- média de 10 segundos --
float somaTemp = 0, somaUmid = 0, somaLuz = 0;
int leituras = 0; //quantas leituras já foram somadas
float mediaTemp = 0, mediaUmid = 0, mediaLuz = 0;
bool temMedia = false; //vira true depois da primeira média

// -- controle de tempo com millis() --
unsigned long ultimaLeitura = 0;
unsigned long ultimaTroca = 0;
unsigned long inicioBuzzer = 0;
bool buzzerLigado = false;
int tela = 0; //qual tela o LCD está mostrando (0 a 3)

// -- EEPROM --
struct Config {   //configurações (setup) salvas no início da EEPROM
  byte assinatura; //se for 123, a EEPROM já foi configurada antes
  int utc;         //fuso horário em relação ao UTC (Brasília = -3)
  char unidade;    //unidade da temperatura: 'C' ou 'F'
  int proximoLog;  //posição onde o próximo log será gravado
  int totalLogs;   //quantos logs existem
};

struct Registro {         //um log
  unsigned long dataHora; //data e hora em UTC (segundos desde 1970)
  float temp;
  float umid;
  float luz;
};

Config config;
int enderecoLogs = 16; //os logs começam no byte 16 da EEPROM
int maxLogs = 60;      //60 logs x 16 bytes = 960 bytes (a EEPROM do Uno tem 1024)

// -- desenhos da taça para a animação (cada caractere tem 5x8 pixels) --
byte tacaVazia[8] = {B10001, B10001, B10001, B10001, B10001, B01010, B00100, B00100};
byte taca1[8]     = {B10001, B10001, B10001, B10001, B11111, B01110, B00100, B00100};
byte taca2[8]     = {B10001, B10001, B10001, B11111, B11111, B01110, B00100, B00100};
byte tacaCheia[8] = {B10001, B11111, B11111, B11111, B11111, B01110, B00100, B00100};
byte haste[8]     = {B00100, B00100, B00100, B00100, B00100, B01110, B11111, B00000};

void setup() {
  Serial.begin(9600);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(buzzer, OUTPUT);

  LCD.init();      //inicializa o LCD
  LCD.backlight(); //liga a luz de fundo
  dht.begin();     //inicializa o DHT22
  rtc.begin();     //inicializa o RTC

  carregarConfig(); //lê o setup da EEPROM
  animacaoLogo();   //logo animado com mensagem de boas-vindas

  Serial.println("Digite L para listar os logs");
}

void loop() {
  unsigned long agora = millis(); //tempo em ms desde que o Arduino ligou

  // -- a cada 1 segundo: lê os sensores e atualiza o LCD --
  if (agora - ultimaLeitura >= 1000) {
    ultimaLeitura = agora;
    lerSensores();

    // -- a cada 10 leituras (10 segundos): calcula a média e verifica o ambiente --
    if (leituras == 10) {
      calcularMedia();
      verificarStatus();
      atualizarLeds();
      mostrarSerial();

      if (statusGeral != 0) { //algo fora do ideal
        tone(buzzer, 1000);   //liga o buzzer
        inicioBuzzer = agora; //guarda quando ele ligou
        buzzerLigado = true;
        salvarLog();          //grava o ocorrido na EEPROM
      }
    }

    // -- troca de tela a cada 3 segundos --
    if (agora - ultimaTroca >= 3000) {
      ultimaTroca = agora;
      tela = tela + 1;
      if (tela > 3) tela = 0;
      LCD.clear();
    }
    mostrarTela();
  }

  // -- desliga o buzzer depois de 3 segundos (sem usar delay) --
  if (buzzerLigado && agora - inicioBuzzer >= 3000) {
    noTone(buzzer);
    buzzerLigado = false;
  }

  // -- se digitar L no monitor serial, lista os logs --
  if (Serial.available()) {
    char comando = Serial.read();
    if (comando == 'L' || comando == 'l') listarLogs();
  }
}

// ===== LEITURA E MÉDIA =====

void lerSensores() {
  float temp = dht.readTemperature(); //em °C
  float umid = dht.readHumidity();    //em %

  if (isnan(temp) || isnan(umid)) { //isnan = "não é um número", ou seja, a leitura falhou
    Serial.println("Erro ao ler o DHT22");
    return; //descarta essa leitura
  }

  int valorLDR = analogRead(LDR);                    //valor de 0 a 1023
  int luz = map(valorLDR, ldrEscuro, ldrClaro, 0, 100); //converte para 0 a 100%
  //OBS: a faixa é invertida porque mais luz resulta em um valor menor no A0
  luz = constrain(luz, 0, 100); //caso um valor saia da faixa

  //soma as leituras para calcular a média depois
  somaTemp = somaTemp + temp;
  somaUmid = somaUmid + umid;
  somaLuz = somaLuz + luz;
  leituras++;
}

void calcularMedia() {
  mediaTemp = somaTemp / leituras;
  mediaUmid = somaUmid / leituras;
  mediaLuz = somaLuz / leituras;

  //zera tudo para começar a próxima média
  somaTemp = 0;
  somaUmid = 0;
  somaLuz = 0;
  leituras = 0;
  temMedia = true;
}

// ===== ALARMES =====

void verificarStatus() {
  //temperatura
  if (mediaTemp >= tempOkMin && mediaTemp <= tempOkMax) statusTemp = 0;
  else if (mediaTemp >= tempAlertaMin && mediaTemp <= tempAlertaMax) statusTemp = 1;
  else statusTemp = 2;

  //umidade
  if (mediaUmid >= umidOkMin && mediaUmid <= umidOkMax) statusUmid = 0;
  else if (mediaUmid >= umidAlertaMin && mediaUmid <= umidAlertaMax) statusUmid = 1;
  else statusUmid = 2;

  //luminosidade
  if (mediaLuz < limiteAlerta) statusLuz = 0;        //0% até 29%
  else if (mediaLuz < limiteProblema) statusLuz = 1; //30% até 69%
  else statusLuz = 2;                                //70% até 100%

  //o status geral é o pior dos três (o maior número)
  statusGeral = max(statusTemp, max(statusUmid, statusLuz));
}

void atualizarLeds() {
  //apaga todos e acende só o LED do status geral
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERMELHO, LOW);

  if (statusGeral == 0) digitalWrite(LED_VERDE, HIGH);
  else if (statusGeral == 1) digitalWrite(LED_AMARELO, HIGH);
  else digitalWrite(LED_VERMELHO, HIGH);
}

// ===== LCD =====

String nomeStatus(int status) {
  if (status == 0) return "OK";
  if (status == 1) return "Alerta";
  return "Problema";
}

//escreve na linha de baixo o status e se o valor está alto ou baixo
void mostrarStatus(int status, bool alto) {
  LCD.setCursor(0, 1);
  LCD.print(nomeStatus(status));
  if (status != 0) {
    if (alto) LCD.print(": ALTA");
    else LCD.print(": BAIXA");
  }
  LCD.print("      "); //espaços apagam o que sobrou da escrita anterior
}

void mostrarTela() {
  //antes da primeira média, mostra a contagem regressiva
  if (!temMedia) {
    LCD.setCursor(0, 0);
    LCD.print("Coletando dados");
    LCD.setCursor(0, 1);
    LCD.print("Aguarde... ");
    LCD.print(10 - leituras);
    LCD.print("s ");
    return;
  }

  LCD.setCursor(0, 0);

  if (tela == 0) { //data, hora e status geral
    DateTime agora = rtc.now();
    char linha[17];
    //sprintf monta o texto com 2 dígitos (ex: 5 vira 05)
    sprintf(linha, "%02d/%02d/%02d  %02d:%02d", agora.day(), agora.month(), agora.year() % 100, agora.hour(), agora.minute());
    LCD.print(linha);
    LCD.setCursor(0, 1);
    LCD.print("Geral: ");
    LCD.print(nomeStatus(statusGeral));
    LCD.print("    ");
  }
  else if (tela == 1) { //temperatura
    LCD.print("Temp: ");
    LCD.print(converterTemp(mediaTemp), 1);
    LCD.write(byte(223)); //símbolo de grau (°), que já vem gravado na memória do LCD
    LCD.print(config.unidade);
    LCD.print("   ");
    mostrarStatus(statusTemp, mediaTemp > tempOkMax);
  }
  else if (tela == 2) { //umidade
    LCD.print("Umidade: ");
    LCD.print(mediaUmid, 0);
    LCD.print("%   ");
    mostrarStatus(statusUmid, mediaUmid > umidOkMax);
  }
  else { //luminosidade
    LCD.print("Luz: ");
    LCD.print(mediaLuz, 0);
    LCD.print("%   ");
    mostrarStatus(statusLuz, true); //a luz só tem problema quando está alta
  }
}

void animacaoLogo() {
  //desenha a taça vazia no meio da tela
  LCD.clear();
  LCD.createChar(0, tacaVazia);
  LCD.createChar(1, haste);
  LCD.setCursor(7, 0);
  LCD.write(byte(0)); //parte de cima da taça
  LCD.setCursor(7, 1);
  LCD.write(byte(1)); //haste da taça
  delay(600);

  //troca o desenho do caractere 0: a taça na tela vai enchendo sozinha
  //(depois do createChar é preciso usar o setCursor de novo)
  LCD.createChar(0, taca1);
  LCD.setCursor(0, 0);
  delay(600);
  LCD.createChar(0, taca2);
  LCD.setCursor(0, 0);
  delay(600);
  LCD.createChar(0, tacaCheia);
  LCD.setCursor(0, 0);
  delay(1000);

  //escreve o nome letra por letra
  LCD.clear();
  String nome = "VINHERIA AGNELLO";
  for (int i = 0; i < 16; i++) {
    LCD.setCursor(i, 0);
    LCD.print(nome[i]);
    delay(120);
  }

  //mensagem de boas-vindas com taças cheias nas pontas
  LCD.setCursor(0, 1);
  LCD.write(byte(0));
  LCD.setCursor(3, 1);
  LCD.print("Bem-vindo!");
  LCD.setCursor(15, 1);
  LCD.write(byte(0));
  delay(2500);
  LCD.clear();
}

// ===== EEPROM =====

void carregarConfig() {
  EEPROM.get(0, config); //lê a configuração salva no byte 0

  //se não tiver a assinatura, a EEPROM está vazia: grava o setup padrão
  if (config.assinatura != 123) {
    config.assinatura = 123;
    config.utc = -3;       //horário de Brasília
    config.unidade = 'C';  //troque para 'F' para usar Fahrenheit
    config.proximoLog = 0;
    config.totalLogs = 0;
    EEPROM.put(0, config);
    Serial.println("EEPROM vazia: configuracao padrao gravada");
  }

  Serial.print("Configuracao -> UTC");
  Serial.print(config.utc);
  Serial.print(" | Unidade: ");
  Serial.println(config.unidade);
}

void salvarLog() {
  Registro registro;
  DateTime agora = rtc.now();

  //o RTC está no horário local, então tiramos o fuso para guardar em UTC
  registro.dataHora = agora.unixtime() - config.utc * 3600L;
  registro.temp = mediaTemp;
  registro.umid = mediaUmid;
  registro.luz = mediaLuz;

  EEPROM.put(enderecoLogs + config.proximoLog * sizeof(Registro), registro);

  //avança a posição; quando chega no fim, volta para o início e sobrescreve os mais antigos
  config.proximoLog++;
  if (config.proximoLog >= maxLogs) config.proximoLog = 0;
  if (config.totalLogs < maxLogs) config.totalLogs++;
  EEPROM.put(0, config); //salva a nova posição

  Serial.println(">> Log gravado na EEPROM");
}

void listarLogs() {
  Serial.println("----- LOGS -----");
  if (config.totalLogs == 0) {
    Serial.println("Nenhum log gravado");
    return;
  }

  for (int i = 0; i < config.totalLogs; i++) {
    Registro registro;
    EEPROM.get(enderecoLogs + i * sizeof(Registro), registro);

    //converte de UTC para o horário local antes de mostrar
    DateTime dataHora(registro.dataHora + config.utc * 3600L);
    char data[20];
    sprintf(data, "%02d/%02d/%04d %02d:%02d:%02d", dataHora.day(), dataHora.month(), dataHora.year(), dataHora.hour(), dataHora.minute(), dataHora.second());

    Serial.print(data);
    Serial.print(" | T: ");
    Serial.print(converterTemp(registro.temp), 1);
    Serial.print(config.unidade);
    Serial.print(" | U: ");
    Serial.print(registro.umid, 1);
    Serial.print("% | L: ");
    Serial.print(registro.luz, 0);
    Serial.println("%");
  }
}

//converte a temperatura para Fahrenheit se essa for a unidade do setup
float converterTemp(float celsius) {
  if (config.unidade == 'F') return celsius * 1.8 + 32;
  return celsius;
}

// ===== MONITOR SERIAL =====

void mostrarSerial() {
  Serial.print("[Media 10s] T: ");
  Serial.print(converterTemp(mediaTemp), 1);
  Serial.print(config.unidade);
  Serial.print(" (");
  Serial.print(nomeStatus(statusTemp));
  Serial.print(") | U: ");
  Serial.print(mediaUmid, 1);
  Serial.print("% (");
  Serial.print(nomeStatus(statusUmid));
  Serial.print(") | L: ");
  Serial.print(mediaLuz, 0);
  Serial.print("% (");
  Serial.print(nomeStatus(statusLuz));
  Serial.print(") | Geral: ");
  Serial.println(nomeStatus(statusGeral));
}
