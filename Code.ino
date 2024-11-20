#include <WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Thông tin kết nối WiFi
const char* ssid = "Nha Giau";
const char* password = "Meomeo1999@";

// Thông tin kết nối MQTT broker
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_topic_time = "web/topic";      
const char* mqtt_topic_led = "web/topic1";     

WiFiClient espClient;
PubSubClient client(espClient);

// Cài đặt NTPClient
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600); // Múi giờ Việt Nam là UTC+7

#define fire_pin 5  // Chân kết nối cảm biến lửa
#define vcc_fire 18
#define buzzer_pin 25       // Chân kết nối còi báo cháy
#define sensor_vcin 19  // Chân kết nối cảm biến vật cản
#define vcc_vcin 21
#define sensor_vcout 22
#define vcc_vcout 23
#define motor_in 32   // Chân kết nối điều khiển động cơ AC
#define motor_out 33

// Biến lưu trữ 
int startHour = 0, startMinute = 0, endHour = 0, endMinute = 0;
bool fireDetected = false;
bool mqttControl = false;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
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
}

// Hàm xử lý cho topic "web/topic"
void handleTimeTopic(String message) {
  // Phân tích chuỗi thời gian nhận được theo định dạng HH:MM-HH:MM
  int sep1 = message.indexOf(':');
  int sep2 = message.indexOf('-');
  int sep3 = message.indexOf(':', sep2);

  if (sep1 != -1 && sep2 != -1 && sep3 != -1) {
    startHour = message.substring(0, sep1).toInt();
    startMinute = message.substring(sep1 + 1, sep2).toInt();
    endHour = message.substring(sep2 + 1, sep3).toInt();
    endMinute = message.substring(sep3 + 1).toInt();

    Serial.print("Thời gian bật: ");
    Serial.print(startHour);
    Serial.print(":");
    Serial.print(startMinute);
    Serial.print(", Thời gian tắt: ");
    Serial.print(endHour);
    Serial.print(":");
    Serial.println(endMinute);
  } else {
    Serial.println("Sai định dạng thời gian nhận được.");
  }
}

// Hàm xử lý cho topic "web/topic1"
void handleLedTopic(String message) {
  if (message == "1") {
    mqttControl = true;
    Serial.println("Ấn 1.");
  } else if (message == "0") {
    mqttControl = false;
    Serial.println("Ấn 0.");
  } else {
    Serial.println("Message does not match any action.");
  }
}

// Hàm callback chính
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String message;

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println();

  // Chuyển topic nhận được thành String để xử lý
  String topicStr = String(topic);

  // Kiểm tra topic và gọi hàm xử lý tương ứng
  if (topicStr == "web/topic") {
    handleTimeTopic(message);
  } else if (topicStr == "web/topic1") {
    handleLedTopic(message);
  } else {
    Serial.println("Unknown topic received.");
  }
}

void sendvaluetomqtt(int value){
  String payload = String(value);
  client.publish("web/warning", payload.c_str());
  Serial.print("Message sent: ");
  Serial.println(payload);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(mqtt_topic_time); // Đăng ký topic thời gian
      client.subscribe(mqtt_topic_led);  // Đăng ký topic LED
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
} 

void setup() {
  pinMode(vcc_fire, OUTPUT);
  digitalWrite(vcc_fire, HIGH);

  pinMode(vcc_vcin, OUTPUT);
  digitalWrite(vcc_vcin, LOW);

  pinMode(vcc_vcout, OUTPUT);
  digitalWrite(vcc_vcout, LOW);

  pinMode(fire_pin, INPUT);
  //attachInterrupt(digitalPinToInterrupt(fire_pin), handleFireInterrupt, FALLING);  
  pinMode(buzzer_pin, OUTPUT);       
  digitalWrite(buzzer_pin, LOW); 

  pinMode(sensor_vcin, INPUT);
  pinMode(sensor_vcout, INPUT);

  pinMode(motor_in, OUTPUT);
  pinMode(motor_out, OUTPUT);
  digitalWrite(motor_in, LOW);
  digitalWrite(motor_out, LOW);

  Serial.begin(115200);
  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Cấu hình thời gian keep-alive
  client.setKeepAlive(60); // Gửi gói PINGREQ mỗi 60 giây

  // Khởi động NTP client
  timeClient.begin();
}

void fire() {
  int sensorValue_fire = digitalRead(fire_pin); // Đọc giá trị từ cảm biến lửa
  Serial.print("Giá trị cảm biến lửa: ");
  Serial.println(sensorValue_fire);

  sendvaluetomqtt(sensorValue_fire); // Gửi trạng thái cảm biến đến MQTT broker

  if (sensorValue_fire == 0) {  // Nếu phát hiện cháy
    if (!fireDetected) {        // Nếu trạng thái cháy chưa được ghi nhận
      fireDetected = true;      // Đặt trạng thái cháy
      Serial.println("Ngọn lửa được phát hiện! Kích hoạt còi báo cháy.");
    }
    digitalWrite(buzzer_pin, HIGH); // Bật còi báo động
    opendoor();                     // Mở cửa
  } else if (fireDetected) {        // Nếu hết cháy
    fireDetected = false;           // Reset trạng thái cháy
    Serial.println("Không còn ngọn lửa. Tắt còi báo cháy.");
    digitalWrite(buzzer_pin, LOW);  // Tắt còi báo động
    // Không đóng cửa tại đây, chuyển quyền điều khiển lại cho cảm biến vật cản
  }
}

void opendoor()
{
  digitalWrite(motor_out, LOW);
  digitalWrite(motor_in, HIGH);
  Serial.println("Mở cửa.");
  delay(1000);
}

void closedoor()
{
  digitalWrite(motor_in, LOW);
  delay(3000);
  digitalWrite(motor_out, HIGH);
  Serial.println("Đóng cửa.");
  delay(1000);
}

void loop() {
 //------------------
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Cập nhật thời gian hiện tại từ NTP
  timeClient.update();
 //----------------------
  fire();

//---------------------------------------

  if (!fireDetected) {
    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();

    // Kiểm tra trong khoảng thời gian cho phép mở cửa
    if ((currentHour > startHour || (currentHour == startHour && currentMinute >= startMinute)) &&
        (currentHour < endHour || (currentHour == endHour && currentMinute <= endMinute))) {
      
      // Bật nguồn cho cảm biến vật cản
      digitalWrite(vcc_vcin, HIGH);
      digitalWrite(vcc_vcout, HIGH);

      // Đọc giá trị từ cảm biến vật cản
      int sensorValue_in = digitalRead(sensor_vcin);
      int sensorValue_out = digitalRead(sensor_vcout);

      if ((sensorValue_in == LOW) || (sensorValue_out == LOW)) {
        opendoor(); // Mở cửa nếu phát hiện vật cản
      } else {
        closedoor(); // Đóng cửa nếu không có vật cản
      }
    } else {
      // Ngoài thời gian cho phép mở cửa
      digitalWrite(vcc_vcin, LOW);
      digitalWrite(vcc_vcout, LOW);
      
      if (mqttControl) {
        opendoor();
        delay(500); 
        Serial.println("Mở cửa thủ công.");           
      } else{
          closedoor();
          delay(500);
          Serial.println("Đang giờ đóng cửa.");
      }      
      delay(500);
    }
  } else {
    // Nếu có cháy, luôn mở cửa
    opendoor();
  }
  
}