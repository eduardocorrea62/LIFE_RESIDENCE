//PROJETO PILOTO DE MONITORAMENTO DE NÍVEL DE CAIXA D'ÁGUA PREDIAL
//TÉCNICO RESPONSÁVEL: Eduardo R Corrêa
//Life Residence
//Cisterna_A
//ArduinoOTA - Password = cisternaa
//--------------------------------------------------------------------------------------------------------------------------------------------------
//Habilita a funcionalidade de atualização de firmware Over-The-Air (OTA) no ESP32.
#include <ArduinoOTA.h> 
//--------------------------------------------------------------------------------------------------------------------------------------------------
#define BLYNK_TEMPLATE_ID "TMPL2nmkbHGjF"
#define BLYNK_TEMPLATE_NAME "LIFE RESIDENCE"
#define BLYNK_AUTH_TOKEN "2tZHqrkpe35HajM4eOxt84_AQOWpwXfB"
//--------------------------------------------------------------------------------------------------------------------------------------------------
//Credenciais para as redes Wi-Fi

const char* ssid = "Fastsignal_Salao Kids"; 
const char* pass = "33482323";

// const char* ssid = "AquaMonitor";
// const char* pass = "12345678";

//--------------------------------------------------------------------------------------------------------------------------------------------------
// Bibliotecas utilizadas no projeto para funcionalidades diversas:
#include <Adafruit_GFX.h>          // Biblioteca para gráficos em displays, usada para trabalhar com textos, formas e imagens.
#include <Adafruit_SH110X.h>       // Biblioteca para displays OLED com driver SH110X.
#include <Adafruit_SSD1306.h>      // Biblioteca para displays OLED com driver SSD1306.
#include <BlynkSimpleEsp32.h>      // Biblioteca para integração do ESP32 com a plataforma Blynk.
#include <WiFi.h>                  // Biblioteca para gerenciamento de conexão Wi-Fi.
#include <WiFiClient.h>            // Biblioteca para conexões de cliente Wi-Fi.
#include <HTTPClient.h>            // Biblioteca para realizar requisições HTTP.
#include <UrlEncode.h>             // Biblioteca para codificação de URLs em requisições HTTP.
#include <NTPClient.h>             // Biblioteca para sincronização de hora via protocolo NTP.
#include <WiFiUDP.h>               // Biblioteca para conexões UDP, necessária para o NTPClient.
#include <WiFiClientSecure.h>      // Biblioteca para conexões seguras (SSL/TLS) via Wi-Fi.
#include <UniversalTelegramBot.h>  // Biblioteca para comunicação com o Telegram via bot.
#include <time.h>
//--------------------------------------------------------------------------------------------------------------------------------------------------
//SUPORTE TÉCNICO: BOT para controle das mensagens da reinicialização  e reconexão dos ESP32
String botTokenWF = "8444010748:AAEQitDxvJKIPcv4NS8S6aOUZVkKMqImKB0";
String chatIdsWF[] = {"207223980"}; // Adicione os IDs aqui
const int totalChatsWF = sizeof(chatIdsWF) / sizeof(chatIdsWF[0]);
//--------------------------------------------------------------------------------------------------------------------------------------------------
// Configuração da biblioteca
WiFiClientSecure client; // Cliente seguro para comunicação HTTPS
//--------------------------------------------------------------------------------------------------------------------------------------------------
// DEFINIÇÕES DE PINOS
#define pinTrigger 27
#define pinEcho 26
#define VPIN_NIVEL_PERCENTUAL V1
#define VPIN_DISTANCIA V16
//--------------------------------------------------------------------------------------------------------------------------------------------------
// Flag para permitir ou bloquear envio de mensagens
bool mensagensPermitidas = false; 
//--------------------------------------------------------------------------------------------------------------------------------------------------
// Definições do display OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_SH1106G oled = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1, 400000, 100000);
#define BLYNK_HEARTBEAT 5   // mantém o "online" a cada 5s
//--------------------------------------------------------------------------------------------------------------------------------------------------
// Variáveis para o cálculo do nível de água no tanque:
float distancia;                 // Armazena a distância medida pelo sensor ultrassônico.
float nivelPercentual;           // Nível do tanque em porcentagem, com precisão de uma casa decimal.
float emptyTankDistance = 163;    // Distância medida pelo sensor quando o tanque está vazio (em cm). ATUALIZADO em 13/06/2025
float fullTankDistance =   34;    // Distância medida pelo sensor quando o tanque está cheio (em cm). ATUALIZADO em 13/06/2025
// float emptyTankDistance = 52.13;   // Medidas para teste de bancada(em cm).
// float fullTankDistance =  29.95;   // Medidas para teste de bancada(em cm).
const float VOLUME_TOTAL = 15000; // Volume total do tanque em litros. FALTA ATUALIZAR
float volumeFormatado = 0.0;  // Variável global
//--------------------------------------------------------------------------------------------------------------------------------------------------
//Notificção de nível abaixo do limite
unsigned long ultimaNotificacao50 = 0;
const unsigned long intervaloNotificacao = 3600000; // 1 hora
//const unsigned long intervaloNotificacao = 60000; // 1 minuto para teste
//--------------------------------------------------------------------------------------------------------------------------------------------------
//ALERTA do Problema na Bóia a cada 60 segundos/1 min
unsigned long lastMessageTime = 0;
const unsigned long messageInterval = 1000; // 10 segundos--- 1 minuto
//--------------------------------------------------------------------------------------------------------------------------------------------------
// Flags para cada nível de alerta
bool flag05 = true;
bool flag25 = true;
bool flag50 = true;
bool flag75 = true;
bool flag100 = true;
bool boiaNormal = false;  // Variável de controle para garantir que a mensagem será enviada uma única vez quando o nível estiver normal
bool mensagemEnviada = false; // Nova flag para controlar o envio da mensagem
bool exibirVolume = true; // Alternador de mensagens
//--------------------------------------------------------------------------------------------------------------------------------------------------
// BLYNK AUTENTICAÇÃO
char auth[] = BLYNK_AUTH_TOKEN;
BlynkTimer timer;
//--------------------------------------------------------------------------------------------------------------------------------------------------
// Configuração do cliente NTP para sincronização de data e hora:
WiFiUDP ntpUDP;                                    // Objeto para comunicação via protocolo UDP.
NTPClient timeClient(ntpUDP, "pool.ntp.org",       // Cliente NTP usando o servidor "pool.ntp.org".
                     -3 * 3600,                   // Fuso horário definido para UTC-3 (Brasília).
                     60000);                      // Atualização do horário a cada 60 segundos.
//--------------------------------------------------------------------------------------------------------------------------------------------------
// Variável para o status da rede Wi-Fi
String statusWiFi = "Wi-Fi: ...";
//--------------------------------------------------------------------------------------------------------------------------------------------------
const char* firmwareURL = "https://eduardocorrea62.github.io/LIFE_RESIDENCE/Cisterna_A_LR.ino.bin";
bool firmwareUpdated = false;  // Flag global para controle de atualização de firmware
//--------------------------------------------------------------------------------------------------------------------------------------------------
const char* ntpServer = "pool.ntp.org";  // Servidor NTP
const long gmtOffset_sec = -3 * 3600;   // Offset UTC-3 para o Brasil
const int daylightOffset_sec = 0;      // Sem horário de verão
unsigned long startMillis;  // Marca o momento em que o dispositivo iniciou
time_t baseTime = 0;        // Tempo base para a sincronização inicial
const char* tz = "BRT3"; // BRT é o fuso horário de Brasília (UTC-3)
//--------------------------------------------------------------------------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    // Conectar ao Wi-Fi
    Serial.println("Tentando conectar ao Wi-Fi...");
    WiFi.begin(ssid, pass);
    
    // Configuração do fuso horário
    configTzTime(tz, "pool.ntp.org");  // Ajuste do fuso horário e NTP
    
    startMillis = millis();

    // Tentar obter o tempo local
    struct tm timeInfo;
    if (getLocalTime(&timeInfo, 5000)) {
        Serial.println("Tempo local obtido!");
        baseTime = mktime(&timeInfo);  // Define o tempo base
    } else {
        Serial.println("Falha ao obter tempo local. Usando RTC interno.");
        // Usando o tempo atual do RTC interno se o NTP falhar
        struct tm defaultTime;
        time_t rtcTime = time(nullptr); // Obtém o tempo atual do RTC
        localtime_r(&rtcTime, &defaultTime);
        baseTime = rtcTime; // Usa o tempo do RTC como base
    }
    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 7000;

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConectado ao Wi-Fi");
        enviarmensagemWiFi("O ESP32 da Cisterna A foi reinicializado com sucesso");
    } else {
        Serial.println("\nFalha ao conectar ao Wi-Fi.");
    }
    // Inicialização de outros periféricos
    Blynk.config(auth);
    sonarBegin(pinTrigger, pinEcho);
    oled.begin(0x3C, true);
    oled.clearDisplay();
    oled.display();
    arduinoOTA();

    // Configuração de temporizadores
    timer.setTimeout(2000L, habilitarMensagens);
    timer.setInterval(500L, checkWaterLevel);         // Verifica o nível de água
    timer.setInterval(800L, blynkVirtualWrite);
    timer.setInterval(1000L, verificaStatusWifi);
    timer.setInterval(500L, atualizarDados);
    timer.setInterval(900L, enviarStatusBlynk);
    timer.setInterval(1200L, volumeAtual);
    timer.setInterval(1000L, verificarWiFi);          // Verifica e reconecta o Wi-Fi, se necessário
    timer.setInterval(500L, displayData);
    timer.setInterval(5000L, verificarNivelCisterna); // a cada 5s
    
    // Configuração de pinos
    pinMode(pinTrigger, OUTPUT);
    pinMode(pinEcho, INPUT);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void loop() {

    ArduinoOTA.handle();           // Processa as atualizações OTA
    Blynk.run();                   // Mantém a conexão com o Blynk
    timer.run();                   // Executa funções programadas pelo timer
}
//-------------------------------------------------------------------------------------------------------
void volumeAtual() {
    float volumeAtual = (nivelPercentual / 100.0) * VOLUME_TOTAL;
    char volumeFormatado[10];
    sprintf(volumeFormatado, "%.3f", volumeAtual / 1000.0);
    // Blynk.virtualWrite(V18, String(volumeFormatado) + "L");  // Envia para o Blynk no Virtual Pin V14
    Blynk.virtualWrite(V18, "_  _  _  _  _ L");  // Envia para o Blynk no Virtual Pin V14
    Serial.print("Volume da Caixa: ");
    Serial.println(volumeAtual); 
   }
//-------------------------------------------------------------------------------------------------------
void verificaStatusWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        if (testaInternet()) {
            statusWiFi = "Internet:OK"; // Tem internet
        } else {
            statusWiFi = "Sem internet"; // Sem internet
        }
    } else {
        statusWiFi = "Wi-Fi: FORA"; // Sem Wi-Fi
    }
    Serial.println(statusWiFi);
}
//----------------------------------------------------------------------------------------------------------------------------------------
bool testaInternet() {
    HTTPClient http;
    http.setTimeout(1000); // 1 segundo de timeout
    http.begin("http://clients3.google.com/generate_204"); // Endereço que responde rápido sem dados
    int httpCode = http.GET();
    http.end();

    if (httpCode > 0 && httpCode == 204) {
        return true; // Internet OK
    } else {
        return false; // Sem internet
    }
}
//-------------------------------------------------------------------------------------------------------
// Função para enviar status ao Blynk
void enviarStatusBlynk() {
    String statusESP32;
    if (WiFi.status() == WL_CONNECTED) {
        statusESP32 = "ONLINE";
    } else {
        statusESP32 = "OFFLINE";
    }

    struct tm timeInfo;
    if (!getLocalTime(&timeInfo)) {
        Serial.println("Falha ao obter o tempo local");
        return;
    }

    // Obtém a data e hora formatadas
    char dateBuffer[11]; // Formato: DD/MM/YYYY
    strftime(dateBuffer, sizeof(dateBuffer), "%d/%m/%Y", &timeInfo);
    char timeBuffer[9]; // Formato: HH:MM:SS
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeInfo);

    // Formata a mensagem com status, data e hora
    String fullMessage = statusESP32 + " em " + String(dateBuffer) + " às " + String(timeBuffer);

    // Envia o status ao Blynk pelo pino virtual V0
    Blynk.virtualWrite(V4, fullMessage);
}                           
//--------------------------------------------------------------------------------------------------------------------------------------------------
// Função chamada pelo botão no Blynk
BLYNK_WRITE(V23) {  
  if (param.asInt() == 1) {  
    Serial.println("Botão pressionado no Blynk. Atualizando firmware...");
    enviarmensagemWiFi("Cisterna A: Botão acionado no Blynk WEB. Aguarde... Atualizando firmware do ESP32");
    performFirmwareUpdate(firmwareURL);
  }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void verificarNivelCisterna() {
  // Usa APENAS o nível REAL medido pelo sensor
  if (nivelPercentual <= 50.0) {

    unsigned long agora = millis();

    // Envia no máximo 1 alerta por intervalo
    if (ultimaNotificacao50 == 0 || (agora - ultimaNotificacao50 >= intervaloNotificacao)) {

      Serial.print("Vai enviar alerta. Nivel = ");
      Serial.print(nivelPercentual, 1);
      Serial.print("%  millis = ");
      Serial.println(agora);

      String mensagem = "🚨 ALERTA: Nível da cisterna está em " + String(nivelPercentual, 1) + "%";

      Blynk.logEvent("cisterna_a", mensagem);

      ultimaNotificacao50 = agora;
    }

  } else {
    // Se o nível voltar a ficar acima de 50%, reseta o controle de notificação
    if (ultimaNotificacao50 != 0) {
      Serial.println("Nível voltou acima de 50%, resetando controle de notificação.");
    }
    ultimaNotificacao50 = 0;
  }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void performFirmwareUpdate(const char* firmwareURL) {
    Serial.println("Iniciando atualização de firmware...");

    HTTPClient http;
    http.begin(firmwareURL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        WiFiClient* client = http.getStreamPtr();

        if (contentLength > 0) {
            if (Update.begin(contentLength)) {
                size_t written = Update.writeStream(*client);

                if (written == contentLength) {
                    if (Update.end() && Update.isFinished()) {
                        Serial.println("Atualização concluída com sucesso. Reiniciando...");
                        Serial.println("");
                        enviarmensagemWiFi("Atualização concluída com sucesso");
                        delay(500);
                        ESP.restart();  // Reinicia após atualização bem-sucedida
                    } else {
                        Serial.println("Falha ao finalizar a atualização.");
                    }
                } else {
                    Serial.println("Falha na escrita do firmware.");
                }
            } else {
                Serial.println("Espaço insuficiente para OTA.");
            }
        } else {
            Serial.println("Tamanho do conteúdo inválido.");
        }
    } else {
        Serial.printf("Erro HTTP ao buscar firmware. Código: %d\n", httpCode);
    }
      http.end();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
// Função para gerenciar a conexão Wi-Fi
void conectarWiFi() {
    Serial.println("Tentando conectar ao Wi-Fi...");
    WiFi.begin(ssid, pass);
  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 10000; // Tempo limite para tentar conexão (10 segundos)
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    delay(500);         // Aguarda 500ms entre as tentativas
    Serial.print(".");  // Exibe um ponto a cada tentativa
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado ao Wi-Fi!");
    enviarmensagemWiFi("REDE Wi-Fi: Cisterna A conectado à rede " + String(ssid));
  } else {
    Serial.println("\nFalha ao conectar ao Wi-Fi.");
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void verificarWiFi() {
    static unsigned long lastWiFiCheck = 0;
    const unsigned long wifiCheckInterval = 15000; // Verificar Wi-Fi a cada 15 segundos
    if (millis() - lastWiFiCheck >= wifiCheckInterval) {
        lastWiFiCheck = millis();

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Wi-Fi desconectado! Tentando reconectar...");
            conectarWiFi();
        }
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void arduinoOTA() {                                                             //ATUALIZAÇÃO DO CÓDIGO DO ESP32 VIA WI-FI ATRAVÉS DO OTA(Over-the-Air)
  //ATUALIZAÇÃO DO CÓDIGO DO ESP32 VIA WI-FI ATRAVÉS DO OTA(Over-the-Air)
  ArduinoOTA.setHostname("Cisterna_A");                                 // Define o nome do dispositivo para identificação no processo OTA.
  ArduinoOTA.setPassword("cisternaa");                                           // Define a senha de autenticação para o processo OTA.
  ArduinoOTA.onStart([]() {                                                      // Callback para o início da atualização OTA.
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem"; // Identifica o tipo de atualização.
    Serial.println("Iniciando atualização de " + type);                          // Exibe o tipo de atualização no Serial Monitor.
  });
  ArduinoOTA.onEnd([]() {                                                        // Callback para o final da atualização OTA.
    Serial.println("\nAtualização concluída.");                                  // Confirma que a atualização foi finalizada com sucesso.
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {          // Callback para progresso da atualização.
    Serial.printf("Progresso: %u%%\r", (progress / (total / 100)));              // Mostra o progresso em porcentagem.
  });
  ArduinoOTA.onError([](ota_error_t error) {                                     // Callback para tratamento de erros durante a atualização.
    Serial.printf("Erro [%u]: ", error);                                         // Exibe o código do erro no Serial Monitor.
    if (error == OTA_AUTH_ERROR) Serial.println("Falha de autenticação");        // Mensagem para erro de autenticação.
    else if (error == OTA_BEGIN_ERROR) Serial.println("Falha ao iniciar");       // Mensagem para erro de inicialização.
    else if (error == OTA_CONNECT_ERROR) Serial.println("Falha de conexão");     // Mensagem para erro de conexão.
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Falha de recebimento"); // Mensagem para erro de recebimento.
    else if (error == OTA_END_ERROR) Serial.println("Falha ao finalizar");       // Mensagem para erro ao finalizar.
  });
  ArduinoOTA.begin();  // Inicializa o serviço OTA para permitir atualizações.
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void enviarmensagemWiFi(String message) {
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo)) {
        Serial.println("Falha ao obter o tempo local");
        return;
    }

    // Obtém a data e hora formatadas
    char dateBuffer[11]; // Formato: DD/MM/YYYY
    strftime(dateBuffer, sizeof(dateBuffer), "%d/%m/%Y", &timeInfo);
    char timeBuffer[9]; // Formato: HH:MM:SS
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeInfo);

    // Formata a mensagem
    String fullMessage = message + " em " + String(dateBuffer) + " às " + String(timeBuffer);
    String encodedMessage = urlEncode(fullMessage);  

    for (int i = 0; i < totalChatsWF; i++) {
        HTTPClient http;
        String url = "https://api.telegram.org/bot" + botTokenWF + "/sendMessage?chat_id=" + chatIdsWF[i] + "&text=" + encodedMessage;
        http.begin(url);
        int httpResponseCode = http.GET();

        if (httpResponseCode == 200) {
            Serial.println("Mensagem enviada com sucesso para o Chat ID: " + chatIdsWF[i]);
        } else {
            Serial.println("Erro ao enviar para o Chat ID: " + chatIdsWF[i] + ". Código: " + String(httpResponseCode));
        }
        http.end();  // Finaliza a conexão HTTP
        delay(500);  // Atraso para evitar bloqueio por excesso de requisições
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void habilitarMensagens() {
  mensagensPermitidas = true;                                   // Habilita o envio de mensagens
  Serial.println("Mensagens habilitadas após inicialização.");  // Informa que as mensagens estão habilitadas
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void atualizarDados() {                                                    //Envia a Blynk os valores do nível e a distâmcia do sensor a superfície.
  distancia = calcularDistancia();                                         // Calcula a distância usando o sensor ultrassônico
  nivelPercentual = calcularPercentual(distancia);                         // Calcula o percentual do nível com base na distância
  Blynk.virtualWrite(VPIN_NIVEL_PERCENTUAL, String(nivelPercentual, 1));   // Atualiza o valor do nível percentual no Blynk com uma casa decimal
  Blynk.virtualWrite(VPIN_DISTANCIA, String(distancia));
  displayData();                                                         // Exibe os dados no display OLED
  Serial.print("Distância: ");
  Serial.print(distancia);                                                // Exibe a distância no Serial Monitor
  Serial.println(" cm");
  Serial.print("Nível: ");
  Serial.print(nivelPercentual, 1);                                      // Exibe o nível com uma casa decimal no Serial Monitor
  Serial.println(" %");
  Serial.println("");                                                    // Adiciona uma linha em branco para separar as leituras
  delay(500);                                                            // Aguarda 500 ms antes de realizar a próxima leitura
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void displayData() {
    static float lastNivel = -1;       // Armazena o último valor do nível para evitar atualização desnecessária
    static String lastStatusWiFi = ""; // Armazena o último status do Wi-Fi
    float volumeAtual = (nivelPercentual / 100.0) * VOLUME_TOTAL;
    char volumeFormatado[10];
    sprintf(volumeFormatado, "%.3f", volumeAtual / 1000.0);

    // Atualiza o display se o nível ou o status Wi-Fi mudar
    if (nivelPercentual != lastNivel || statusWiFi != lastStatusWiFi) {
        oled.clearDisplay();  // Limpa o display OLED
        oled.setTextSize(1.9);  // Define o tamanho do texto
        oled.setTextColor(WHITE);  // Define a cor do texto como branco
        oled.setCursor(3, 8);  // Define a posição inicial do cursor
        oled.println(statusWiFi);  // Exibe o status da rede Wi-Fi
        oled.setCursor(4, 19);  // Define nova posição do cursor
        oled.println("Cisterna A");  // Exibe o título "CAIXA"
        oled.setCursor(16, 30);
        // oled.print(volumeFormatado);
        oled.print("LIFE");
        oled.print("");
        oled.setCursor(3, 44);  // Define a posição do cursor para o valor do nível
        oled.setTextSize(2);  // Aumenta o tamanho do texto
        oled.print(nivelPercentual, 1);  // Exibe o valor do nível com uma casa decimal
        oled.print("%");
        oled.drawRect(0, 0, SCREEN_WIDTH - 51, SCREEN_HEIGHT, WHITE);  // Desenha o retângulo da borda do display
        int tankWidth = 22;  // Largura do tanque
        int tankX = SCREEN_WIDTH - tankWidth - 2;  // Posição X do tanque
        int tankY = 2;  // Posição Y do tanque
        int tankHeight = SCREEN_HEIGHT - 10;  // Altura do tanque
        oled.drawRect(tankX, tankY, tankWidth, tankHeight, WHITE);  // Desenha o contorno do tanque
        int fillHeight = map(nivelPercentual, 0, 100, 0, tankHeight);  // Calcula a altura da água no tanque
        oled.fillRect(tankX + 1, tankY + tankHeight - fillHeight, tankWidth - 2, fillHeight, WHITE);  // Preenche o tanque com base no nível
        oled.setTextSize(1);  // Restaura o tamanho do texto
        oled.setCursor(tankX - 24, tankY - 2);  // Define a posição do texto "100%"
        oled.print("100%");
        oled.setCursor(tankX - 18, tankY + tankHeight / 2 - 4);  // Define a posição do texto "50%"
        oled.print("50%");
        oled.setCursor(tankX - 12, tankY + tankHeight - 8);  // Define a posição do texto "0%"
        oled.print("0%");
        oled.display();  // Atualiza o display com os dados
        lastNivel = nivelPercentual;  // Atualiza o valor do nível armazenado
        lastStatusWiFi = statusWiFi; // Atualiza o último status do Wi-Fi armazenado
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void sonarBegin(byte trig ,byte echo) { // Função para configurar o sensor ultrassônico e inicializar os pinos de disparo e recepção.
                                        // Define as constantes necessárias e prepara os pinos para as medições de distância.
  #define divisor 57.74                  // Fator de conversão para calcular a distância (em cm) com base no tempo de retorno do som
  #define intervaloMedida 50          // Intervalo entre medições (em milissegundos)
  #define qtdMedidas 50                 // Quantidade de medições para calcular a média
  pinMode(trig, OUTPUT);                // Define o pino 'trig' como saída
  pinMode(echo, INPUT);                 // Define o pino 'echo' como entrada
  digitalWrite(trig, LOW);              // Garante que o pino 'trig' comece com nível baixo
  delayMicroseconds(500);               // Aguarda 500 microssegundos para garantir estabilidade nos pinos
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
float calcularDistancia() {   // Função que calcula a distância média com base em várias leituras do sensor ultrassônico.
                              // A média das leituras é calculada para melhorar a precisão da medição e um valor de correção é adicionado no final.
  float leituraSum = 0;                               // Inicializa a variável para somar as leituras
  for (int index = 0; index < qtdMedidas; index++) {  // Loop para realizar múltiplas leituras
    delay(intervaloMedida);                           // Aguarda o intervalo entre as leituras
    leituraSum += leituraSimples();                   // Adiciona o valor da leitura simples à soma total
  }
  return (leituraSum / qtdMedidas) + 2.2;             // Retorna a média das leituras mais um valor de correção
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
float leituraSimples() {  // Função que realiza uma leitura simples do sensor ultrassônico, 
                          //...calculando o tempo de retorno do sinal e convertendo-o em distância.
  long duracao = 0;                    // Variável para armazenar a duração do pulso recebido
  digitalWrite(pinTrigger, HIGH);      // Envia um pulso de ativação no pino trigger
  delayMicroseconds(10);               // Aguarda 10 microssegundos
  digitalWrite(pinTrigger, LOW);       // Desliga o pulso de ativação
  duracao = pulseIn(pinEcho, HIGH);    // Mede o tempo que o sinal leva para retornar no pino echo
  return ((float) duracao / divisor);  // Converte o tempo de retorno em distância usando o divisor
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
float calcularPercentual(float distancia) {   // Função que calcula o percentual de nível de água com base na distância medida
  // Calcula o percentual de nível com base na distância
  float nivelPercentual = ((emptyTankDistance - distancia) / (emptyTankDistance - fullTankDistance)) * 100.0;
  // Retorna o percentual, limitado entre 0 e 100
  return constrain(nivelPercentual, 0.0, 100.0);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void checkWaterLevel() {

    // Alerta para Nível de 50%
    if (nivelPercentual > 50) {
        Blynk.virtualWrite(V27, "O nível está dentro da normalidade.");
        Blynk.setProperty(V27, "color", "#0000FF"); // Azul

        // Envia mensagem de alerta apenas na transição
        if (flag50) {
        flag50 = false;
        }

    } else if (nivelPercentual <= 50) {
        String mensagem = "Nível em " + String(nivelPercentual, 1) + "%. \nPossível pressão baixa do SEMASA e/ou consumo alto.";
        Blynk.virtualWrite(V27, mensagem);
        Blynk.setProperty(V27, "color", "#FF0000"); // Vermelho
        
        // Envia mensagem de alerta apenas na transição
        if (!flag50) {
            flag50 = true;
        }
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------
void blynkVirtualWrite(){
      // ALERTA SOBRE A BÓIA
      if (distancia <= 23 && boiaNormal && (millis() - lastMessageTime >= messageInterval)) {
        lastMessageTime = millis();
        boiaNormal = false; // Define que já foi enviado o alerta
        Blynk.virtualWrite(V12, "RISCO DE TRANSBORDAR! \nNível acima do limite. Possível problema na bóia.");
        Blynk.setProperty(V12, "color", "#FF0000"); // Vermelho
      }
      if (distancia >= 30 && !boiaNormal) {
        boiaNormal = true; // Impede o envio repetido enquanto a distância for maior que 33
        Blynk.virtualWrite(V12, "Bóia operando normalmente.");
        Blynk.setProperty(V12, "color", "#0000FF"); // Azul
    }
}

//--------------------------------------------------------------------------------------------------------------------------------------------------
