/*
 * Stack-chan-tester
 * 
 * @author TakaoAkaki
 * Copyright (c) 2021-2024 Takao Akaki. All right reserved
 */

/*
 * This file contains code under multiple licenses:
 *
 * - Portions based on code by Takao Akaki (c) 2021-2024, originally published under the MIT License.
 *   See https://github.com/mongonta0716/stack-chan-tester
 *
 * - Portions based on code from M5CoreS3_FaceDetect, licensed under the GNU Lesser General Public License v2.1.
 *   See https://github.com/ronron-gh/M5CoreS3_FaceDetect
 *
 * - Additional code written by Kazumasa Hirai, licensed under the MIT License.
 *
 * Please refer to the LICENSE files for full license texts.
*/
// ------------------------
// ヘッダファイルのinclude
// 
#include <Arduino.h>                         // Arduinoフレームワークを使用する場合は必ず必要
#include <SD.h>                              // SDカードを使うためのライブラリです。
#include <Update.h>                          // 定義しないとエラーが出るため追加。
#include <Ticker.h>                          // 定義しないとエラーが出るため追加。
#include <M5StackUpdater.h>                  // M5Stack SDUpdaterライブラリ
#include <M5Unified.h>                       // M5Unifiedライブラリ
#include <Stackchan_system_config.h>         // stack-chanの初期設定ファイルを扱うライブラリ
#include <Stackchan_servo.h>                 // stack-chanのサーボを動かすためのライブラリ
#include <Avatar.h>                          // 顔を表示するためのライブラリ https://github.com/meganetaaan/m5stack-avatar
#include "formatString.hpp"                  // 文字列に変数の値を組み込むために使うライブラリ https://gist.github.com/GOB52/e158b689273569357b04736b78f050d6

#include <esp_camera.h>
#include <fb_gfx.h>
#include <vector>
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
// ヘッダファイルのinclude end 
// ================================== End

// ---------------------------------------------
// グローバル変数の定義エリア
// プログラム全体で利用する変数やクラスを決める
int servo_offset_x = 0;  // X軸サーボのオフセット（サーボの初期位置からの+-で設定）
int servo_offset_y = 0;  // Y軸サーボのオフセット（サーボの初期位置からの+-で設定）

using namespace m5avatar;     // (Avatar.h)avatarのnamespaceを使う宣言（こうするとm5avatar::???と書かなくて済む。)
Avatar avatar;                // (Avatar.h)avatarのクラスを定義
ColorPalette *cps[2];

#define SDU_APP_PATH "/stackchan_tester.bin"  // (SDUpdater.h)SDUpdaterで使用する変数
#define TFCARD_CS_PIN 4                       // SDカードスロットのCSPIN番号

StackchanSERVO servo;                         // (Stackchan_servo.h) サーボを扱うためのクラス
StackchanSystemConfig system_config;          // (Stackchan_system_config.h) プログラム内で使用するパラメータをYAMLから読み込むクラスを定義

uint32_t mouth_wait = 2000;       // 通常時のセリフ入れ替え時間（msec）
uint32_t last_mouth_millis = 0;   // セリフを入れ替えた時間
bool core_port_a = false;         // Core1のPortAを使っているかどうか

const char* lyrics[] = { "BtnA:MoveTo90  ", "BtnB:FaceDetect  ", "BtnC:RandomMode  ", "BtnALong:AdjustMode"};  // 通常モード時に表示するセリフ
const int lyrics_size = sizeof(lyrics) / sizeof(char*);  // セリフの数
int lyrics_idx = 0;                                      // 表示するセリフ数用の変数

int latest_x = 0;
int latest_y = 0;

#define TWO_STAGE 1 /*<! 1: detect by two-stage which is more accurate but slower(with keypoints). */
                    /*<! 0: detect by one-stage which is less accurate but faster(without keypoints). */

#define LCD_WIDTH (320)
#define LCD_HEIGHT  (240)
#define LCD_BUF_SIZE (LCD_WIDTH*LCD_HEIGHT*2)

// ================================== End

static camera_config_t camera_config = {
    .pin_pwdn     = -1,
    .pin_reset    = -1,
    .pin_xclk     = 2,
    .pin_sscb_sda = 12,
    .pin_sscb_scl = 11,

    .pin_d7 = 47,
    .pin_d6 = 48,
    .pin_d5 = 16,
    .pin_d4 = 15,
    .pin_d3 = 42,
    .pin_d2 = 41,
    .pin_d1 = 40,
    .pin_d0 = 39,

    .pin_vsync = 46,
    .pin_href  = 38,
    .pin_pclk  = 45,

    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565,
    //.pixel_format = PIXFORMAT_JPEG,
    .frame_size   = FRAMESIZE_QVGA,   // QVGA(320x240)
    .jpeg_quality = 0,
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

// ------------------------------------------------
// Stack-chan-testerの各モードを定義するエリア
// adjustOffset(): オフセットを調べるモード
// moveRandom()  : ランダムでStack-chanが動くモード
// testServo()   : サーボをテストするときのモード

// オフセットの設定モード(loop()でBtnAが押されると実行されます。)
void adjustOffset() {
  // サーボのオフセットを調整するモード
  // 関数内のみで使用する変数の初期化
  servo_offset_x = 0;
  servo_offset_y = 0;
  servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 2000);  // サーボを中央にセット
  bool adjustX = true; // X軸とY軸のモードを切り替えるためのフラグ
  for (;;) { // 無限ループ(BtnBが長押しされるまで繰り返します。)
    M5.update();
    if (M5.BtnA.wasPressed()) {
      // オフセットを減らす
      if (adjustX) {
        servo_offset_x--;
      } else {
        servo_offset_y--;
      }
    }
    if (M5.BtnB.pressedFor(2000)) {
      // 調整モードを終了(loop()へ戻る)
      break;
    }
    if (M5.BtnB.wasPressed()) {
      // 調整モードのXとYを切り替え
      adjustX = !adjustX;
    }
    if (M5.BtnC.wasPressed()) {
      // オフセットを増やす
      if (adjustX) {
        servo_offset_x++;
      } else {
        servo_offset_y++;
      }
    }
    // オフセットを反映した位置にサーボを移動
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree + servo_offset_x, system_config.getServoInfo(AXIS_Y)->start_degree + servo_offset_y, 1000);

    std::string s;

    if (adjustX) {
      // X軸の値を設定する。
      s = formatString("%s:%d:BtnB:X/Y", "X", servo_offset_x); // 吹き出し用のStringを作成
    } else {
      // Y軸の値を設定する。
      s = formatString("%s:%d:BtnB:X/Y", "Y", servo_offset_y); // 吹き出し用のStringを作成
    }
    // 
    avatar.setSpeechText(s.c_str()); // 作成したStringをChar型に変換して吹き出しに表示する。

  }
}

// ランダムモード(loop()でBtnCが押されると実行されます。)
void moveRandom() {
  for (;;) { // 無限ループ（BtnCが押されるまでランダムモードを繰り返します。
    // ランダムモード
    int x = random(system_config.getServoInfo(AXIS_X)->lower_limit + 45, system_config.getServoInfo(AXIS_X)->upper_limit - 45);  // 可動範囲の下限+45〜上限-45 でランダム
    int y = random(system_config.getServoInfo(AXIS_Y)->lower_limit, system_config.getServoInfo(AXIS_Y)->upper_limit);            // 可動範囲の下限〜上限 でランダム
    M5.update();
    if (M5.BtnC.wasPressed()) {
      // ランダムモードを抜ける処理。(loop()に戻ります。)
      break;
    }
    uint16_t base_delay_time = 0;
    if (system_config.getServoType() == ServoType::SCS || system_config.getServoType() == ServoType::DYN_XL330) {
      base_delay_time = 500;
    }
    int delay_time = random(10);
    servo.moveXY(x, y, 1000 + (100 + base_delay_time) * delay_time);
    delay(2000 + 500 * delay_time);
    if (!core_port_a) {
      // Basic/M5Stack Fireの場合はバッテリー情報が取得できないので表示しない
      avatar.setBatteryStatus(M5.Power.isCharging(), M5.Power.getBatteryLevel());
    }
    //avatar.setSpeechText("Stop BtnC");
    avatar.setSpeechText("");
  }
}
void testServo() {
  // サーボのテストの動き
  for (int i=0; i<2; i++) { // 同じ動きを2回繰り返す。
    avatar.setSpeechText("X center -> left  ");
    servo.moveX(system_config.getServoInfo(AXIS_X)->lower_limit, 1000); // 初期位置から左へ向く
    avatar.setSpeechText("X left -> right  ");
    servo.moveX(system_config.getServoInfo(AXIS_X)->upper_limit, 3000); // 左へ向いたあと右へ向く
    avatar.setSpeechText("X right -> center  ");
    servo.moveX(system_config.getServoInfo(AXIS_X)->start_degree, 1000); // 前を向く
    avatar.setSpeechText("Y center -> lower  ");
    servo.moveY(system_config.getServoInfo(AXIS_Y)->lower_limit, 1000); // 上を向く
    avatar.setSpeechText("Y lower -> upper  ");
    servo.moveY(system_config.getServoInfo(AXIS_Y)->upper_limit, 1000); // 下を向く
    avatar.setSpeechText("Initial Pos.");
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000); // 初期位置に戻る（正面を向く)
  }
}

// ぼっちちゃんのむむむを再現するために遊びで作った関数。
// 使わないでください。
void mumumuServo() {
  for (int i=0; i<30; i++) {
    servo.moveX(120, 250);
    servo.moveX(240, 250);
  }
}
// ============================================== End

esp_err_t camera_init(){

    //initialize the camera
    M5.In_I2C.release();
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        //Serial.println("Camera Init Failed");
        M5.Display.println("Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

void check_heap_free_size(void){
  Serial.printf("===============================================================\n");
  Serial.printf("Mem Test\n");
  Serial.printf("===============================================================\n");
  Serial.printf("esp_get_free_heap_size()                              : %6d\n", esp_get_free_heap_size() );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_DMA)               : %6d\n", heap_caps_get_free_size(MALLOC_CAP_DMA) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_SPIRAM)            : %6d\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_INTERNAL)          : %6d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_DEFAULT)           : %6d\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT) );

}

#define FACE_COLOR_WHITE  0x00FFFFFF
#define FACE_COLOR_BLACK  0x00000000
#define FACE_COLOR_RED    0x000000FF
#define FACE_COLOR_GREEN  0x0000FF00
#define FACE_COLOR_BLUE   0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN   (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)

static void draw_face_boxes(fb_data_t *fb, std::list<dl::detect::result_t> *results, int face_id)
{
    int x, y, w, h, c_x, c_y, dx, dy, new_x, new_y;
    uint32_t color = FACE_COLOR_YELLOW;
    if (face_id < 0)
    {
        color = FACE_COLOR_RED;
    }
    else if (face_id > 0)
    {
        color = FACE_COLOR_GREEN;
    }
    if(fb->bytes_per_pixel == 2){
        //color = ((color >> 8) & 0xF800) | ((color >> 3) & 0x07E0) | (color & 0x001F);
        color = ((color >> 16) & 0x001F) | ((color >> 3) & 0x07E0) | ((color << 8) & 0xF800);
    }
    int i = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results->begin(); prediction != results->end(); prediction++, i++)
    {
        // rectangle box
        x = (int)prediction->box[0];
        y = (int)prediction->box[1];

        // yが負の数のときにfb_gfx_drawFastHLine()でメモリ破壊してリセットする不具合の対策
        if(y < 0){
           y = 0;
        }

        w = (int)prediction->box[2] - x + 1;
        h = (int)prediction->box[3] - y + 1;

        //Serial.printf("x:%d y:%d w:%d h:%d\n", x, y, w, h);

        if((x + w) > fb->width){
            w = fb->width - x;
        }
        if((y + h) > fb->height){
            h = fb->height - y;
        }

        c_x = x + (w / 2);
        c_y = y + (h / 2);

        //dx = (((c_x - 160) * 23) / 320);
        //dy = (((c_y - 120) * 23) / 320);

        if(c_x - 160 > 5){
          dx = 1;
        }else if(c_x - 160 < -5){
          dx = -1;
        }else{
          dx = 0;
        }

        if(c_y - 120 > 5){
          dy = 1;
        }else if(c_y - 120 < -5){
          dy = -1;
        }else{
          dy = 0;
        }

        new_x = latest_x + dx;
        new_y = latest_y + dy;

        if(new_x > system_config.getServoInfo(AXIS_X)->upper_limit){
          new_x = system_config.getServoInfo(AXIS_X)->upper_limit;
        }else if (new_x < system_config.getServoInfo(AXIS_X)->lower_limit){
          new_x = system_config.getServoInfo(AXIS_X)->lower_limit;
        }

        if(new_y > system_config.getServoInfo(AXIS_Y)->upper_limit){
          new_y = system_config.getServoInfo(AXIS_Y)->upper_limit;
        }else if (new_y < system_config.getServoInfo(AXIS_Y)->lower_limit){
          new_y = system_config.getServoInfo(AXIS_Y)->lower_limit;
        }

        //Serial.printf("x:%d y:%d w:%d h:%d c_x:%d c_y:%d dx:%d dy:%d new_x:%d new_y:%d\n", x, y, w, h, c_x, c_y, dx, dy, new_x, new_y);
        servo.moveXY(new_x, new_y, 10);
        //servo.moveXY(new_x, system_config.getServoInfo(AXIS_Y)->start_degree, 10);

        latest_x = new_x;
        latest_y = new_y;

        //fb_gfx_fillRect(fb, x+10, y+10, w-20, h-20, FACE_COLOR_RED);  //モザイク
        //fb_gfx_drawFastHLine(fb, x, y, w, color);
        //fb_gfx_drawFastHLine(fb, x, y + h - 1, w, color);
        //fb_gfx_drawFastVLine(fb, x, y, h, color);
        //fb_gfx_drawFastVLine(fb, x + w - 1, y, h, color);

#if TWO_STAGE
        // landmarks (left eye, mouth left, nose, right eye, mouth right)
        int x0, y0, j;
        for (j = 0; j < 10; j+=2) {
            x0 = (int)prediction->keypoint[j];
            y0 = (int)prediction->keypoint[j+1];
            //fb_gfx_fillRect(fb, x0, y0, 3, 3, color);
        }
#endif
    }
}

esp_err_t camera_capture_and_face_detect(){
  //Serial.println("camera_capture_and_face_detect");
  //acquire a frame
  M5.In_I2C.release();
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    //Serial.println("Camera Capture Failed");
    M5.Display.println("Camera Capture Failed");
    return ESP_FAIL;
  }


  int face_id = 0;
  //Serial.println("HumanFaceDetect_start");

#if TWO_STAGE
  HumanFaceDetectMSR01 s1(0.1F, 0.5F, 10, 0.2F);
  HumanFaceDetectMNP01 s2(0.5F, 0.3F, 5);
  std::list<dl::detect::result_t> &candidates = s1.infer((uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3});
  std::list<dl::detect::result_t> &results = s2.infer((uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3}, candidates);
#else
  HumanFaceDetectMSR01 s1(0.3F, 0.5F, 10, 0.2F);
  std::list<dl::detect::result_t> &results = s1.infer((uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3});
#endif
  //Serial.println("HumanFaceDetect_end");
  if (results.size() > 0) {
      //Serial.printf("Face detected : %d\n", results.size());

      fb_data_t rfb;
      rfb.width = fb->width;
      rfb.height = fb->height;
      rfb.data = fb->buf;
      rfb.bytes_per_pixel = 2;
      rfb.format = FB_RGB565;

      draw_face_boxes(&rfb, &results, face_id);
      //Serial.println("draw_face_boxes_end");
  }


  //replace this with your own function
  //process_image(fb->width, fb->height, fb->format, fb->buf, fb->len);
  //M5.Display.startWrite();
  //M5.Display.setAddrWindow(0, 0, LCD_WIDTH, LCD_HEIGHT);
  //M5.Display.writePixels((uint16_t*)fb->buf, int(fb->len / 2));
  //M5.Display.endWrite();
  
  //Serial.println("M5_Display_end");
  //Serial.println("<heap size before fb return>");  
  //check_heap_free_size();

  //return the frame buffer back to the driver for reuse
  esp_camera_fb_return(fb);
  //Serial.println("esp_camera_fb_return");

  //Serial.println("<heap size after fb return>");  
  //check_heap_free_size();

  return ESP_OK;
}

void camera_capture_and_face_detect_loop(){
  //Serial.println("loop_start");
  servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 2000);  // サーボを初期位置へ変更
  latest_x = system_config.getServoInfo(AXIS_X)->start_degree;
  latest_y = system_config.getServoInfo(AXIS_Y)->start_degree;
  for (;;) {
    M5.update();
    if (M5.BtnA.wasPressed()) {
      // ランダムモードを抜ける処理。(loop()に戻ります。)
      //Serial.println("break");
      break;
    }
    camera_capture_and_face_detect();
  }
  //Serial.println("loop_end");
}

// ------------------------------------------------------------------
// Arduinoフレームワークで一番最初に実行される関数の定義
// void setup()とvoid loop()は必ず必要です。
// void setup()は、最初に1回だけ実行します。
void setup() {
  auto cfg = M5.config();       // 設定用の情報を抽出
  cfg.serial_baudrate = 115200;
  //cfg.output_power = true;
  M5.begin(cfg);                // M5Stackをcfgの設定で初期化
  M5.setTouchButtonHeight(40); // タッチボタンの画面下部から40pxに設定
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);    // M5Unifiedのログ初期化（画面には表示しない。)
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);     // M5Unifiedのログ初期化（シリアルモニターにESP_LOG_INFOのレベルのみ表示する)
  M5.Log.setEnableColor(m5::log_target_serial, false);         // M5Unifiedのログ初期化（ログをカラー化しない。）
  M5_LOGI("Hello World");                                      // logにHello Worldと表示
  SD.begin(GPIO_NUM_4, SPI, 25000000);                         // SDカードの初期化
  delay(2000);                                                 // SDカードの初期化を少し待ちます。
 
  system_config.loadConfig(SD, "");                            // SDカードから初期設定ファイルを読み込む
  if (M5.getBoard() == m5::board_t::board_M5Stack) {           // Core1かどうかの判断
    if (system_config.getServoInfo(AXIS_X)->pin == 22) {       // サーボのGPIOが22であるか確認（本当は21も確認してもいいかもしれないが省略）
      // M5Stack Coreの場合、Port.Aを使う場合は内部I2CをOffにする必要がある。バッテリー表示は不可。
      M5_LOGI("I2CRelease");              // ログに出力
      avatar.setBatteryIcon(false);       // avatarのバッテリーアイコンを表示しないモードに設定
      M5.In_I2C.release();                // I2Cを使わないように設定(IMUやIP5306との通信をしないようにします。)※設定しないとサーボの動きがおかしくなります。
      core_port_a = true;                 // Core1でポートAを使用しているフラグをtrueに設定
    }
  } else {
    avatar.setBatteryIcon(true);          // Core2以降の場合は、バッテリーアイコンを表示する。
  }
  // servoの初期化
  M5_LOGI("attach servo"); // ログへ出力
  // サーボの初期化を行います。（このとき、初期位置（正面）を向きます。）
  servo.begin(system_config.getServoInfo(AXIS_X)->pin, system_config.getServoInfo(AXIS_X)->start_degree,
              system_config.getServoInfo(AXIS_X)->offset,
              system_config.getServoInfo(AXIS_Y)->pin, system_config.getServoInfo(AXIS_Y)->start_degree,
              system_config.getServoInfo(AXIS_Y)->offset,
              (ServoType)system_config.getServoType());

  M5.Power.setExtOutput(!system_config.getUseTakaoBase());       // 設定ファイルのTakaoBaseがtrueの場合は、Groveポートの5V出力をONにする。

  M5_LOGI("ServoType: %d\n", system_config.getServoType());      // サーボのタイプをログに出力
  //USBSerial.println("HelloWorldUSBSerial");
  avatar.init();                   // avatarを初期化して実行開始します。(このときに顔が表示されます。)
  cps[0] = new ColorPalette();
  cps[0]->set(COLOR_PRIMARY, TFT_BLACK);
  cps[0]->set(COLOR_BACKGROUND, TFT_WHITE);
  avatar.setColorPalette(*cps[0]);

  last_mouth_millis = millis();    // loop内で使用するのですが、処理を止めずにタイマーを実行するための変数です。一定時間で口を開くのとセリフを切り替えるのに利用します。
  //moveRandom();
  //testServo();

  camera_init();
}


// メインのloop処理。（必ず定義が必要。）
// 基本的にはずっと繰り返し実行されます。
// stack-chan-testerの場合は、ボタンを押して、各モードの関数が実行されると一時停止します。
void loop() {
  M5.update();  // M5Stackのボタン状態を更新します。
  if (M5.BtnA.pressedFor(2000)) {                // ボタンAを2秒長押しを検出したら。
    adjustOffset();                              // サーボのオフセットを調整するモードへ
  } else if (M5.BtnA.wasPressed()) {             // ボタンAが押された場合
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 2000);  // サーボを初期位置へ変更
  }
  
  if (M5.BtnB.wasSingleClicked()) {               // ボタンBが1回クリックされたとき
    //testServo();                                  // サーボのテストを実行します。
    camera_capture_and_face_detect_loop();
  } else if (M5.BtnB.wasDoubleClicked()) {        // ボタンBをダブルクリックしたとき
    // Groveポートの出力を切り替えます。
    if (M5.Power.getExtOutput() == true) { 
      M5.Power.setExtOutput(false);
      avatar.setSpeechText("ExtOutput Off");
    } else {
      M5.Power.setExtOutput(true);
      avatar.setSpeechText("ExtOutput On");
    }
    delay(2000);                  // 2秒待ちます。(吹き出しをしばらく表示させるため)
    avatar.setSpeechText("");     // 吹き出しの表示を消します。
  } 
  if (M5.BtnC.pressedFor(5000)) {                                    // ボタンCを5秒長押しした場合(この処理はあまり使わないでしょう。)
    M5_LOGI("Will copy this sketch to filesystem");                  // ログへ出力
    if (saveSketchToFS( SD, SDU_APP_PATH, TFCARD_CS_PIN )) {         // SDUpdater用のbinファイルをSDカードに書き込みます。
      M5_LOGI("Copy Successful!");
    } else {
      M5_LOGI("Copy failed!");
    }
  } else if (M5.BtnC.wasPressed()) {     // ボタンCが押された場合
    //mumumuServo();                     // 左右に高速で首を振ります。（サーボが壊れるのであまり使わないでください。）コメントなので実行されません。
    moveRandom();                        // ランダムモードになります。
  }

  if ((millis() - last_mouth_millis) > mouth_wait) {             // 口を開けるタイミングを待ちます。時間を図ってmouth_waitで設定した時間が経つと繰り返します。
    const char* l = lyrics[lyrics_idx++ % lyrics_size];          // セリフを取り出します。
    avatar.setSpeechText(l);                                     // 吹き出しにセリフを表示します。
    avatar.setMouthOpenRatio(0.7);                               // アバターの口を70%開きます。(70%=0.7)
    delay(200);                                                  // 200ミリ秒口を開いたまま
    avatar.setMouthOpenRatio(0.0);                               // 口を閉じます。(0%=0.0)
    last_mouth_millis = millis();                                // 実行時間を更新。（この時点からまたmouth_wait分待ちます。）
    if (!core_port_a) {                                                              // Core1でポートA「以外」のとき（!はnot）
      avatar.setBatteryStatus(M5.Power.isCharging(), M5.Power.getBatteryLevel());    // バッテリー表示を更新します。
    }
  }

}
