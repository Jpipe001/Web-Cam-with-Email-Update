
// Code From: https://github.com/mobizt/ESP-Mail-Client/tree/master/examples/SMTP/Send_Camera_Image/OV2640

// MODIFIFED TO SUIT Gmail Server, other Email servers are available, see Example code.
// This sends Email with Embedded/Inline Image from ESP32-CAM direct from the OV2640 camera
// or TTGO camera.
// Added Sleep with Wake up with TIMER or MOTION trigger with PIR sensor.
// Note: Uncomment Camera Pins as required for ESP32-CAM or TTGO camera.
/*
   This example shows how to send Email with inline images from OV2640 camera.
   The html and text version messages will be sent.
   Created by K. Suwatchai (Mobizt)
   Email: suwatchai@outlook.com
   Github: https://github.com/mobizt/ESP-Mail-Client
   Copyright (c) 2022 mobizt
*/

// Include Required Libraries
#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include "esp_camera.h"


/*
  // CONFIGURE CAMERA PINS
  // Pin definitions for My ESP32-CAM
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22
  const int PIR_pin = 13;
*/

// CONFIGURE CAMERA PINS
// Pin definitions for CAMERA_MODEL_TTGO_WHITE (Mine)
#define CAMERA_MODEL_TTGO_WHITE_BME280
#define PWDN_GPIO_NUM     26
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     32
#define SIOD_GPIO_NUM     13
#define SIOC_GPIO_NUM     12
#define Y9_GPIO_NUM       39
#define Y8_GPIO_NUM       36
#define Y7_GPIO_NUM       23
#define Y6_GPIO_NUM       18
#define Y5_GPIO_NUM       15
#define Y4_GPIO_NUM       4
#define Y3_GPIO_NUM       14
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM    27
#define HREF_GPIO_NUM     25
#define PCLK_GPIO_NUM     19
const int PIR_pin = 33;



// NETWORK CREDENTIALS
const char*        ssid = "NETWORK NAME";
const char*    password = "PASSWORD";

// CONFIGURE MAIL CLIENT  SET UP HERE  >>>>>
// To send Emails using Gmail on port 465 (SSL), you need to create an app password: https://support.google.com/accounts/answer/185833
#define SMTP_HOST          "smtp.gmail.com"
#define SMTP_PORT          465
#define AUTHOR_EMAIL       "your email@gmail.com"
#define AUTHOR_PASSWORD    "xxxx xxxx xxxx xxxx"
#define RECIPIENT_EMAIL    "email@gmail.com"

SMTPSession smtp;  // The SMTP Session object used for Email sending

void smtpCallback(SMTP_Status status);   //Callback to get the Email sending status

const char* Reason;               // WAKE UP Reason
RTC_DATA_ATTR int bootCount;      // Save in RTC Memory
RTC_DATA_ATTR time_t ts;          // Save in RTC Memory

String Filename() {
  return String(__FILE__).substring(String(__FILE__).lastIndexOf("\\") + 1);
}


void setup()  {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nProgram ~ " + Filename());
  Serial.printf("Starting ...\n");

  // CONFIGURE CAMERA SETTINGS
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  //config.pixel_format = CAMERA_PF_JPEG;
  config.frame_size = FRAMESIZE_SVGA;     //  (800x600)
  config.jpeg_quality = 12; // 10-63 lower number means higher quality
  config.fb_count = 1;

  // INITIALIZE CAMERA
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)    {
    if (err == 0x105)  Serial.printf("Check Camera Pins Configuration !!\n");
    Serial.printf("Camera init failed with error 0x%x", err);
    while (1)      delay(1000);           // Stay here
  }
  Serial.printf(" ~ Camera Initialised OK\n");

  // CONNECT to WiFi network:
  Serial.printf(" ~ WiFi Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.printf(".");
  }
  Serial.printf(" ~ Connected OK\n");

  // PRINT the Signal Strength:
  long rssi = WiFi.RSSI() + 100;
  Serial.printf(" ~ Signal Strength = %d", rssi);
  if (rssi > 50)  Serial.printf(" (>50 - Good)\n");  else   Serial.printf(" (Could be Better)\n");

  smtp.debug(0);   //  Basic debug = 1   No debug = 0 (Removes Some Info Messages)

  smtp.callback(smtpCallback);  //  Set the callback function to get the sending results

  ESP_Mail_Session session;     // Declare the session config data

  /* Set the NTP config time */
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.printf("Time not Available/n");
    session.time.ntp_server = F("pool.ntp.org,time.nist.gov");
    session.time.gmt_offset = -5;           // Winter Time Diff -5 Hrs (New York Time)
    session.time.day_light_offset = +1;     // Summer Time +1 Hr
  }

  /* Set the session config */
  session.server.host_name =           SMTP_HOST;
  session.server.port =                SMTP_PORT;
  session.login.email =                AUTHOR_EMAIL;
  session.login.password =             AUTHOR_PASSWORD;
  //session.login.user_domain =        F("mydomain.net");
  session.login.user_domain =          "";
  SMTP_Message message;            //  Declare the fllowing message class
  message.sender.name =                F("ESP Mail");
  message.sender.email =               AUTHOR_EMAIL;
  message.subject =                    F("Camera Image");
  message.addRecipient(F("user0"),     (RECIPIENT_EMAIL));
  //message.addRecipient(F("user1"), F("change_this@your_mail_dot_com"));  // for Additional Recipients

  //message.html.content = F("<span style=\"color:#ff0000;\">The camera image.</span><br/><br/><img src=\"cid:image-001\" alt=\"esp32 cam image\"  width=\"800\" height=\"600\">");
  String Html = "<font color='Purple'>The camera image" + String(bootCount) + ".</font><br><img src='cid:image-001' alt='esp32 cam image' width='800' height='600'>";
  message.html.content = F(Html.c_str());

  /* Enable the chunked data transfer with pipelining for large message if server supported */
  message.enable.chunking = true;

  /** The content transfer encoding e.g.
     enc_7bit or "7bit" (not encoded)
     enc_qp or "quoted-printable" (encoded) <- not supported for message from blob and file
     enc_base64 or "base64" (encoded)
     enc_binary or "binary" (not encoded)
     enc_8bit or "8bit" (not encoded)
     The default value is "7bit" */
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  /** The HTML text message character set e.g.
     us-ascii
     utf-8
     utf-7
     The default value is utf-8  */
  message.html.charSet = F("utf-8");

  SMTP_Attachment att;

  /** Set the inline image info e.g.
     file name, MIME type, file path, file storage type,
     transfer encoding and content encoding  */
  att.descr.filename = F("camera.jpg");
  att.descr.mime = F("image/jpg");

  Serial.printf("\n ~ Capture the Image\n\n");
  camera_fb_t  * fb = esp_camera_fb_get();          // *** CAPTURE IMAGE ***

  att.blob.data = fb->buf;  // Payload
  //att.blob.data = esp_camera_fb_get();

  att.blob.size = fb->len;  // Length
  //att.blob.size = cam.getSize();

  att.descr.content_id = F("image-001"); // The content id (cid) of camera.jpg image in the src tag

  /* Need to be base64 transfer encoding for inline image */
  att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;

  /* Add inline image to the message */
  message.addInlineImage(att);

  /* Connect to server with the session config */
  if (!smtp.connect(&session))
    return;

  /* Start sending the Email and close the session */
  if (!MailClient.sendMail(&smtp, &message, true))
    Serial.printf("Error sending Email: %s\n", smtp.errorReason());

  // to clear sending result log
  smtp.sendingResult.clear();

  //ESP_MAIL_PRINTF("Free Heap: %d\n", MailClient.getFreeHeap());

  // ********************************************
  // ***  TIMER WAKE UP  or  EXTERNAL WAKE UP  ***
  int Timer = 0;   //  ***  in Seconds,  Set to 0 for EXTERNAL WAKE UP  ***
  // ********************************************

  Print_Wakeup_Reason();    //Print the Wakeup Reason

  Serial.printf("Waking Up from %s : Boot Count = %d\n", Reason, bootCount);
  if (Timer == 0)  Serial.printf("Mode is in MOTION Sense!\n"); else Serial.printf("Mode is in TIMER Sense! : Timer Set to: %d Seconds\n, Timer");

  // ***  TIMER WAKE UP
  if (Timer > 0) esp_sleep_enable_timer_wakeup(Timer * 1000000);
  // ***  EXTERNAL WAKE UP
  // PIR_pin = GPIO_NUM_13, 1 = High, 0 = Low Input for ESP32
  // PIR_pin = GPIO_NUM_33, 1 = High, 0 = Low Input for TTGO

  if (Timer == 0) {
    while (digitalRead(PIR_pin) == 1)  {  // Wait for GPIO_NUM_PIR to Reset
      delay(100);    // Wait a little bit
    }
    // esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 1);  // for ESP32
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_pin, HIGH);
  }
  bootCount = bootCount + 1;         // Increment Boot Counter
  Serial.printf("Going to Sleep now!\n");
  Serial.flush();

  esp_deep_sleep_start();
}


void loop()  {
  // Nothing to do here!!
}


//Callback function to get the Email sending status
void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());  // Print the current status
  if (status.success())           // Print the sending result
  {
    // ESP_MAIL_PRINTF used in the examples is for format printing via debug Serial port
    // that works for all supported Arduino platform SDKs e.g. AVR, SAMD, ESP32 and ESP8266.
    // In ESP32 and ESP32, you can use Serial.printf directly.

    //Serial.printf("----------------\n");
    //ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    //ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    Serial.printf("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);

      // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
      // your device time was synched with NTP server.
      // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
      // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF(" Email No: %d\n", i + 1);
      ESP_MAIL_PRINTF("   Status: %s\n", result.completed ? "Success" : "Failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_mon + 1, dt.tm_mday, dt.tm_year + 1900, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("  Subject: %s\n", result.subject.c_str());
    }
    Serial.printf("----------------\n");

    // You need to clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
  }
}


void Print_Wakeup_Reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  Serial.printf("\n");
  switch (wakeup_reason)  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.printf("Wakeup caused by EXTERNAL Signal using RTC_IO\n");
      Reason = "~ EXT Input";
      break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.printf("Wakeup caused by EXTERNAL Signal using RTC_CNTL\n");
      Reason = "~ EXT Input";
      break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.printf("Wakeup caused by TIMER\n");
      Reason = "~ TIMER";
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.printf("Wakeup caused by Touchpad\n"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program\n"); break;
    default : Serial.printf("Wakeup was NOT Caused by Deep Sleep\n");
      Reason = "~ START Up";
      break;
  }
}
