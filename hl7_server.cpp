#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "time.h"
#include "uart_register.h"

// Replace with your network credentials
const char* ssid     = "*******";
const char* password = "*******";
const char* host = "*********";//serverın ip adresi 1
const int httpPort = 80;//serverın port bilgisi

boolean led=false;

String etco2="0";
String fico2="0";
String etco2rr="0"; 

uint8_t buf_idx = 0;   
uint8_t fifo_len = 4;

bool interrupt_flag=0;
String timestamp;
 
String dizi_ayirma(){
    fifo_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT; //rf fifo uzunluğunu oku
	char a[fifo_len];
	String str;
	buf_idx = 0;
	while (buf_idx < fifo_len) {
		char read=READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
		if (read=='-')
		{
			for (int i=0;i<fifo_len-buf_idx;i++)
			{
				a[buf_idx] ='\0';
				buf_idx++;
			}
			buf_idx=fifo_len;
			}else{
			a[buf_idx] =read;
		buf_idx++;}
	}
	a[fifo_len] = '\0';
	str=a;
	return str;
}

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

String getTime() {
  timeClient.update();
   time_t epochTime = timeClient.getEpochTime();
   struct tm *ptm = gmtime ((time_t *)&epochTime);
   String yil=String(ptm->tm_year+1900);
   String ay=String(ptm->tm_mon+1);
   String gun=String(ptm->tm_mday);
   String saat=String(timeClient.getHours());
   String dakika=String(timeClient.getMinutes());
   String saniye=String(timeClient.getSeconds());
   if ((ptm->tm_mon+1)<10){ay="0"+ay;}//hl7 ay gun ve tarihleri iki basamaklı olarak algılar bir basamaklı ise sonuna 0 eklenir
   if ((ptm->tm_mday)<10){gun="0"+gun;}
   if (timeClient.getHours()<10){saat="0"+saat;}
   if (timeClient.getMinutes()<10){dakika="0"+dakika;}
   if (timeClient.getSeconds()<10){saniye="0"+saniye;}
  String now =(yil+ay+gun+saat+dakika+saniye);
  return now;
}

void uart0_rx_intr_handler(void *para){
  uint8_t RcvChar;
  uint8_t uart_no = UART0;
  uint32_t uart_intr_status = READ_PERI_REG(UART_INT_ST(uart_no));//get uart intr status
  while(uart_intr_status != 0x0) {
    if (UART_FRM_ERR_INT_ST == (uart_intr_status & UART_FRM_ERR_INT_ST)){ // if it is caused by a frm_err interrupt
      WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_FRM_ERR_INT_CLR);
      Serial.println("caused by a frm_err interrupt");
    } else if (UART_RXFIFO_FULL_INT_ST == (uart_intr_status & UART_RXFIFO_FULL_INT_ST)) { //if it is caused by a fifo_full interrupt
      Serial.println("caused by a fifo_full interrupt");
      fifo_len = (READ_PERI_REG(UART_STATUS(uart_no)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT; //read rf fifo length
      char r[fifo_len];
      buf_idx = 0;
      while (buf_idx < fifo_len) {
        r[buf_idx] = READ_PERI_REG(UART_FIFO(uart_no)) & 0xFF;
        buf_idx++;
      }
      r[fifo_len] = '\0';
      WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_FULL_INT_CLR); //clear full interrupt state
    } else if (UART_RXFIFO_TOUT_INT_ST == (uart_intr_status & UART_RXFIFO_TOUT_INT_ST)) { //if it is caused by a time_out interrupt
      //Serial.println("caused by a time_out interrupt");
      interrupt_flag=1;//interrupt aktif
     fifo_len = (READ_PERI_REG(UART_STATUS(uart_no)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT; //read rf fifo length
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////stm32 den gelen verileri firebase için paketlere ayırma
   etco2=dizi_ayirma();
	 fico2=dizi_ayirma();//veri paketi uartda(266-85-257) şeklinde geliyor
   etco2rr=dizi_ayirma();
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////stm32 den gelen verileri firebase için paketlere ayırma	 
      WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_TOUT_INT_CLR); //clear full interrupt state
    } else if (UART_TXFIFO_EMPTY_INT_ST == (uart_intr_status & UART_TXFIFO_EMPTY_INT_ST)) { //if it is caused by a tx_empty interrupt
      Serial.println("caused by a tx_empty interrupt");
      WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_TXFIFO_EMPTY_INT_CLR);
      CLEAR_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_TXFIFO_EMPTY_INT_ENA);
    } else {

    }
    uart_intr_status = READ_PERI_REG(UART_INT_ST(uart_no)); //update interrupt status
  }
}

static void install_uart_tout(){
  ETS_UART_INTR_DISABLE();
  ETS_UART_INTR_ATTACH(uart0_rx_intr_handler, NULL);

  WRITE_PERI_REG(UART_CONF1(0), UART_RX_TOUT_EN |
    ((0x2 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S));

  WRITE_PERI_REG(UART_INT_CLR(0), 0xffff);
  SET_PERI_REG_MASK(UART_INT_ENA(0), UART_RXFIFO_TOUT_INT_ENA);
  CLEAR_PERI_REG_MASK(UART_INT_ENA(0), UART_RXFIFO_FULL_INT_ENA);

  ETS_UART_INTR_ENABLE();
}


void setup() {
  pinMode(2,OUTPUT);
  Serial.begin(115200);
  delay(10);
  install_uart_tout();
  //WiFi ağına bağlanarak başlıyoruz
  Serial.println();
  Serial.println();
  Serial.print("connecting to WiFi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

 // int value = 0;
  }
  
  void loop(){
  yield();
   if(interrupt_flag==1)//interrupt geldiğinde verileri gönder (firebase interrupt fonksiyonu içerisinde çalışmıyor o yüzden loopun içine yazdım.
   {                    //fakat uartdan gelen verileri interrupt halinde kaydediyor
     Serial.println("Gelen veri:"+etco2+"  "+fico2+"  "+etco2rr);
	   interrupt_flag=0;
   }

    //===============================================
    //    Sensörleri okumak için ayrılan alan
     int etco2=analogRead(A0);
    // fico2+=2;
    // etco2rr+=1;
    //===============================================
    //delay(5000);
    //++value;

    // Serial.print("connecting to ");
    // Serial.println(host);

    //TCP bağlantıları oluşturmak için WiFiClient sınıfını kullanın
    WiFiClient client;
   
    if(!client.connect(host,httpPort)){
      Serial.println("Baglanti basarisiz ");
      return;
    }
    //Şimdi istek için bir URI oluşturduk
    String url = "/hl7data/save.php?etco2="+String(etco2)+"&fico2="+String(fico2)+"&etco2rr="+String(etco2rr);

    // Serial.print("Requesting URL: ");
    // Serial.println(url);

    //Bu, isteği sunucuya gönderir
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  //  unsigned long timeout = millis();
  //   while (client.available() == 0){
  //     if(millis() - timeout > 5000){
  //     Serial.println(">>> client Timeout !");
  //     client.stop();
  //     return;
  //   }
   delay(1);//sn de 20 kere
 // digitalWrite(2,!led);
  }

  //Cevabın tüm satırlarını sunucudan oku ve seriye yazdır
  // while(client.available()){
  //   String line = client.readStringUntil('\r');
  //   Serial.print(line);
  // }
  // String line = client.readStringUntil('\r');
  //   Serial.print(line);

  // Serial.println();
  // Serial.println("closing connecting");
  // delay(50);//sn de 20 kere
  // digitalWrite(2,!led);
//  }
