#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FS.h>

// Conexão WIFI
const char* ssid = "?????????";
const char* password = "?????????";

// API do Telegram
const char* host = "api.telegram.org";
const int httpsPort = 443;
const char* token = "?????????:???????????????????????????????????";                                        
const char* chat_id = "?????????";

const char* boundry = "<delimitador_conteudo>";
const char* filenamePhoto = "/foto.jpg"; // Faça upload do arquivo no menu: Tools -> "ESP8266 Sketch Data Upload"

WiFiClientSecure client; // client HTTPS


void setup() {
  Serial.begin(115200);  

  // Inicia sistema de arquivos 
  initFileSystem();

  // Inicia conexão WIFI
  initWifiConnection();

  // Lista arquivos do diretório
  listFilesDirectory();

  // Envia foto para o Telegram
  sendPhotoToTelegram();
}

// Inicia sistema de arquivos
void initFileSystem()
{
  Serial.println();
  Serial.print("Abrindo sistema de arquivos da memoria FLASH...");
  if (SPIFFS.begin()) {
    Serial.println("OK");
  }
  else {
    Serial.println("Falha ao abrir sistema de arquivos da memoria FLASH!");
    return;
  }    
}

// Inicia conexão WIFI
void initWifiConnection()
{
  Serial.println();
  Serial.printf("Conectando na rede Wifi [%s]...\r\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());   
}

// Lista todos os arquivos do diretório
void listFilesDirectory()
{
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
      Serial.print(dir.fileName());
      Serial.print(": ");
      File f = dir.openFile("r");
      Serial.println(f.size());
      f.close();
  }
}

// Envia foto para o Telegram
void sendPhotoToTelegram()
{
    // Conecta no servidor do Telegram
    Serial.printf("Conectando %s:%d... ", host, httpsPort);

    // Tenta conectar via domínio 
    if (!client.connect(host, httpsPort)) {      
      Serial.println("Falha na conexao com o servidor!");      
      return;
    }

    // Envia dados para o Telegram
    sendDataToTelegram();

    // Recebe dados do Telegram
    receiveDataFromTelegram();
}

// Envia dados para o Telegram
void sendDataToTelegram()
{
    String start_request = "";
    String end_request = "";

    // Delimitador de conteúdo inicial "chat_id"
    start_request = start_request + "--" + boundry + "\r\n";
    start_request = start_request + "content-disposition: form-data; name=\"chat_id\"" + "\r\n";
    start_request = start_request + "\r\n";
    start_request = start_request + chat_id + "\r\n";

    // Delimitador de conteúdo inicial "photo"
    start_request = start_request + "--" + boundry + "\r\n";
    start_request = start_request + "content-disposition: form-data; name=\"photo\"; filename=\"foto.jpg\"\r\n";
    start_request = start_request + "Content-Type: image/jpeg\r\n";
    start_request = start_request + "\r\n";

    // Delimitador de conteúdo final
    end_request = end_request + "\r\n";
    end_request = end_request + "--" + boundry + "--" + "\r\n";

    // Calcula o tamanho do conteúdo incluindo os delimitadores de conteúdo
    File file = SPIFFS.open(filenamePhoto, "r");                    
    int contentLength = (int)file.size() + start_request.length() + end_request.length();    

    // Monta o cabeçalho HTTP
    String headers = String("POST /bot") + token + "/sendPhoto HTTP/1.1\r\n";
    headers = headers + "Host: " + host + "\r\n";
    headers = headers + "User-Agent: ESP8266" + String(ESP.getChipId()) + "\r\n";
    headers = headers + "Accept: */*\r\n";
    headers = headers + "Content-Type: multipart/form-data; boundary=" + boundry + "\r\n";
    headers = headers + "Content-Length: " + contentLength + "\r\n";
    headers = headers + "\r\n";
    headers = headers + "\r\n";

    Serial.println();
    Serial.println("Enviando dados para o Telegram...");        

    // Envia cabeçalho HTTP
    Serial.print(headers);        
    client.print(headers);
    client.flush();

    // Envia delimitador de conteúdo inicial
    Serial.print(start_request);
    client.print(start_request);
    client.flush();

    // Envia conteúdo do arquivo
    sendFile(&file);
    
    file.close();
    client.flush();

    // Envia delimitador de conteúdo final
    Serial.print(end_request);
    client.print(end_request);
    client.flush();
}

// Envia os dados do arquivo via stream
void sendFile(Stream* stream)
{
    size_t bytesReaded; // bytes lidos
    size_t bytesSent;   // bytes enviados
    do {
        uint8_t buff[1024]; // buffer
        bytesSent = 0;
        bytesReaded = stream->readBytes(buff, sizeof(buff));
        if (bytesReaded) {
            bytesSent = client.write(buff, bytesReaded);
            client.flush();
        }        
    } while ( (bytesSent == bytesReaded) && (bytesSent > 0) );
}

// Recebe dados do Telegram
void receiveDataFromTelegram()
{
    // Espera por tempo de resposta do Servidor
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Sem resposta do servidor!");
        client.stop();
        return;
      }
    }    
      
    // Recebe resposta do Telegram
    Serial.println();
    Serial.println("Recebendo dados do Telegram...");        

    // Recebe cabeçalho HTTP
    int responseContentLength = 0;
    while (client.available()) {
      
      // Carrega uma linha do cabeçalho
      String line = client.readStringUntil('\r');
      client.read(); // lê o caracter '\n'

      Serial.println(line);

      // Lê o tamanho do conteúdo da resposta
      if (line.startsWith("Content-Length:")) {
        int index = line.indexOf(':');
        responseContentLength = line.substring(index + 1).toInt();
      }
      
      // Verifica se é o fim do cabelho HTTP
      if (line.length() == 0)
        break;
    }
    
    // Recebe conteúdo da resposta (mensagem JSON)
    while (responseContentLength > 0)
    {
      char ch = client.read();      
      Serial.print(ch);
      responseContentLength--;
    }  

    // Fecha a conexão
    Serial.println();
    Serial.println("closing connection");    
}

void loop() {
}
