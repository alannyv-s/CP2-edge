# CP2-edge

# Vinheria Agnello

## Descrição do projeto 

O projeto foi desenvolvido com um sistema de monitoramento das condições ambientais de armazenamento dos vinhos, contando com sensores de luminosidade, temperatura e umidade para melhor capitação.

O sistema é composto por sensores de medição, como o DHT22 que mede a temperatura e a umidade e o LDR, que é um sensor de luminosidade. Com base na definição dos limites, no código, esses sensores repassam as informações do ambiente para o arduino, a cada 1 segundo o Arduino lê os três sensores,e  a cada 10 leituras(10 segundos) se calcula a média de cada um e classifica o valor como OK, ALERTA ou PROBLEMA. Será exibido no LCD o valor com o seu status prórpio, acionando os LEDS e o buzzer.

A novidade é que,esse sistema conta com um display LCD que  além de mostrar a identificação da vinheira, também servirá como monitoramento descritivo, nele constará informações da temperatura, umidade, luminosidade, horário e o horário geral do ambiente. Já o RTC funciona como um relógio, registrando data e hora desses acontecimentos, e quando acontece um alerta ou problema isso fica gravado na memória EEPROM, com todas as informações disponibilizadas no LCD.

No recebimento de informações, a cada 1 segundo o Arduino lê os três sensores. A cada 10 leituras, totalizando 10 segundos 



## Componentes 
  - 1 Arduino UNO R3
  - 1 DHT22
  - 1 RTC DS1307
  - 1 LCD I2C
  - 1 Protoboard( Breadboard)
  - 1 LED vermelho
  - 1 LED amarelo
  - 1 LED verde
  - 3 Resistores (220Ω)
  - 1 Buzzer

## Plataforma
 - Wokwi

## Bibliotecas utilizadas
 <Wire.h>              
 <LiquidCrystal_I2C.h> 
 <DHT.h>               
 <RTClib.h>            
 <EEPROM.h>

 ## Como replicar o projeto
 1. Conete o LCD ao Arduíno, adicione a biblioteca "LiquidCrystal I2C" no código e teste se a mensagem de boas vindas esta funcionando.

 2. Adicione o sensor DHT22, usado para medir a temperatura e a umidade, e a sua biblioteca "DHT sensor library" 

 3. Adicione o LDR e calibre-o, e defina as faixas de valores:
 jsx
 luminosidade = map(valorLDR, 1015, 8, 0, 100); // 1015 = escuro (0%), 8 = claro (100%)

 4. Adicione o relógio DS1307 RTC e a sua biblioteca “RTClib” (da Adafruit)

 5. Adicione o protoboard, alimente os lados positivos e negativos.

 6. Adicione os 3 LEDS e os 3 Resistores, ligando nas respectivas posições
  vermelho - pino 10
  amarelo  - pino 9
  verde    - pino 8

7. Use a ligação padrão:
 ânodo do LED ligado ao resistor, que já estara ligado ao Arduino, e o cátodo do LED ligado ao GND do Arduíno.

 8.Adicione o Buzzer e defina os limites para a calibração do LDR:

<img width="1129" height="259" alt="image" src="https://github.com/user-attachments/assets/210e94ad-8d1c-4d71-82d1-cca1ddfddc0c" />


 -o buzzer também será acionado quando o LED vermelho estiver ligado, não mais só quando o LED amarelo estiver ligado.
