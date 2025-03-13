// M5Unified(+M5GFX)で、AtomS3+外部ディスプレイ(ST7789)を使う
// 2024-10-14
// AtomS3: https://docs.m5stack.com/ja/core/AtomS3
// ST7789: https://www.amazon.co.jp/gp/product/B07QG93NPB/

// ST7789のみ使うならM5GFX.hでもよい
#include <M5Unified.h>
#include <lgfx/v1/panel/Panel_ST7789.hpp>

// 使用したピン
// 3.3V -> VCC
// G    -> GND
constexpr int PIN_SCL = 1;  // SCLK(SPI Clock) (SCL)
constexpr int PIN_SDA = 3;  // MOSI (SDA)
constexpr int PIN_RST = 5;  // Reset
constexpr int PIN_DC = 7;   // Data/Command
constexpr int PIN_BLK = 44; // Backlight
constexpr int PIN_CS = 43;  // Chip Select

// 目の設定
constexpr int EYE_RADIUS = 30;          // 目の半径
constexpr int EYE_SPACING = 80;         // 目の間隔
constexpr int PUPIL_RADIUS = 15;        // 瞳の半径
constexpr int DISPLAY_CENTER_X = 120;   // ディスプレイの中心X
constexpr int DISPLAY_CENTER_Y = 160;   // ディスプレイの中心Y
constexpr int MOVE_INTERVAL_MIN = 2000; // 目の動きの最小間隔（ミリ秒）
constexpr int MOVE_INTERVAL_MAX = 5000; // 目の動きの最大間隔（ミリ秒）
constexpr int MOVE_DURATION = 200;      // 目の動きの持続時間（ミリ秒）

// 目の位置情報
struct EyePosition
{
  int x;
  int y;
};

// 目の状態
struct EyeState
{
  EyePosition leftEye;
  EyePosition rightEye;
  EyePosition prevLeftEye;  // 前回の左目の位置
  EyePosition prevRightEye; // 前回の右目の位置
  unsigned long nextMoveTime;
  bool isMoving;
  unsigned long moveStartTime;
  EyePosition targetLeft;
  EyePosition targetRight;
  EyePosition originalLeft;
  EyePosition originalRight;
  bool initialized; // 初期化済みフラグ
};

// LovyanGFX: https://github.com/lovyan03/LovyanGFX
// LovyanGFXのHowToUse/2_user_setting/2_user_setting.inoのコードより
// https://github.com/lovyan03/LovyanGFX/blob/3608914/examples/HowToUse/2_user_setting/2_user_setting.ino
class LGFX_AtomS3_SPI_ST7789 : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789 _panel_instance; // 接続するパネルの型にあったインスタンスを用意
  lgfx::Bus_SPI _bus_instance;        // パネルを接続するバスの種類にあったインスタンスを用意
  lgfx::Light_PWM _light_instance;    // バックライト制御が可能な場合はインスタンスを用意(必要なければ削除)

public:
  LGFX_AtomS3_SPI_ST7789(void)
  {
    {                                    // バス制御の設定を行います。
      auto cfg = _bus_instance.config(); // バス設定用の構造体を取得

      // 使用するSPIを選択
      // ESP32-S2,C3 : SPI2_HOST or SPI3_HOST / ESP32 : VSPI_HOST or HSPI_HOST
      // (追記) AtomS3ではSPI2_HOST
      // ※ ESP-IDFバージョンアップに伴い、VSPI_HOST , HSPI_HOSTの記述は非推奨になるため、
      // エラーが出る場合は代わりにSPI2_HOST , SPI3_HOSTを使用してください。
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 3;       // SPI通信モードを設定(0-3)
      cfg.pin_sclk = PIN_SCL; // SPIのSCLK(SCL)ピン番号を設定
      cfg.pin_mosi = PIN_SDA; // SPIのMOSI(SDA)ピン番号を設定
      cfg.pin_miso = -1;      // SPIのMISOピン番号を設定 (-1 = disable)
      cfg.pin_dc = PIN_DC;    // SPIのD/C(Data/Command)ピン番号を設定 (-1 = disable)
      // SDカードと共通のSPIバスを使う場合、MISOは省略せず必ず設定してください。
      _bus_instance.config(cfg);              // 設定値をバスに反映します。
      _panel_instance.setBus(&_bus_instance); // バスをパネルにセットします。
    }

    {                                      // 表示パネル制御の設定を行います。
      auto cfg = _panel_instance.config(); // 表示パネル設定用の構造体を取得
      cfg.pin_cs = PIN_CS;                 // CSが接続されているピン番号   (-1 = disable)
      cfg.pin_rst = PIN_RST;               // RSTが接続されているピン番号  (-1 = disable)
      cfg.pin_busy = -1;                   // BUSYが接続されているピン番号 (-1 = disable)
      cfg.panel_width = 240;               // 実際に表示可能な幅
      cfg.panel_height = 320;              // 実際に表示可能な高さ
      cfg.invert = true;                   // パネルの明暗が反転してしまう場合 trueに設定
      cfg.offset_rotation = 3;
      _panel_instance.config(cfg);
    }

    {                                      // バックライト制御の設定を行います。（必要なければ削除）
      auto cfg = _light_instance.config(); // バックライト設定用の構造体を取得
      cfg.pin_bl = PIN_BLK;                // バックライトが接続されているピン番号
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance); // バックライトをパネルにセット
    }

    setPanel(&_panel_instance); // 使用するパネルをセット
  }
};

LGFX_AtomS3_SPI_ST7789 ExtDisplay; // インスタンスを作成
LGFX_Sprite eyesSprite;            // 目全体用のスプライト
EyeState eyeState;                 // 目の状態を管理する変数

// 関数プロトタイプ宣言
void drawEyes(EyePosition leftPupil, EyePosition rightPupil);
void updateEyePosition();

// 初期描画
void drawInitialEyes()
{
  // スプライトの初期化（ディスプレイと同じサイズ）
  eyesSprite.createSprite(ExtDisplay.width(), ExtDisplay.height());
  eyesSprite.fillScreen(TFT_BLACK);

  // 初期状態の目を描画
  drawEyes(eyeState.leftEye, eyeState.rightEye);
}

// 目を描画する関数（スプライト使用）
void drawEyes(EyePosition leftPupil, EyePosition rightPupil)
{
  // 背景を黒で塗りつぶし
  eyesSprite.fillScreen(TFT_BLACK);

  // 左右の目の白目部分を描画
  eyesSprite.fillCircle(DISPLAY_CENTER_X - EYE_SPACING / 2, DISPLAY_CENTER_Y, EYE_RADIUS, TFT_WHITE);
  eyesSprite.fillCircle(DISPLAY_CENTER_X + EYE_SPACING / 2, DISPLAY_CENTER_Y, EYE_RADIUS, TFT_WHITE);

  // 左右の瞳を描画
  eyesSprite.fillCircle(DISPLAY_CENTER_X - EYE_SPACING / 2 + leftPupil.x,
                        DISPLAY_CENTER_Y + leftPupil.y,
                        PUPIL_RADIUS, TFT_BLACK);

  eyesSprite.fillCircle(DISPLAY_CENTER_X + EYE_SPACING / 2 + rightPupil.x,
                        DISPLAY_CENTER_Y + rightPupil.y,
                        PUPIL_RADIUS, TFT_BLACK);

  // スプライトを画面に転送
  eyesSprite.pushSprite(&ExtDisplay, 0, 0);

  // 現在の位置を前回の位置として保存
  eyeState.prevLeftEye = leftPupil;
  eyeState.prevRightEye = rightPupil;
  eyeState.initialized = true;
}

// 目の位置を更新する関数
void updateEyePosition()
{
  unsigned long currentTime = millis();

  // 動きの開始判定
  if (!eyeState.isMoving && currentTime >= eyeState.nextMoveTime)
  {
    eyeState.isMoving = true;
    eyeState.moveStartTime = currentTime;

    // 元の位置を保存
    eyeState.originalLeft = eyeState.leftEye;
    eyeState.originalRight = eyeState.rightEye;

    // ランダムな目標位置を設定（瞳の動く範囲を制限）
    int maxMove = EYE_RADIUS - PUPIL_RADIUS;
    eyeState.targetLeft.x = random(-maxMove, maxMove + 1);
    eyeState.targetLeft.y = random(-maxMove, maxMove + 1);
    eyeState.targetRight.x = eyeState.targetLeft.x; // 両目を同じ方向に動かす
    eyeState.targetRight.y = eyeState.targetLeft.y;

    // 次の動きの時間を設定
    eyeState.nextMoveTime = currentTime + MOVE_DURATION + random(MOVE_INTERVAL_MIN, MOVE_INTERVAL_MAX + 1);
  }

  // 動きの処理
  if (eyeState.isMoving)
  {
    unsigned long elapsedTime = currentTime - eyeState.moveStartTime;

    if (elapsedTime >= MOVE_DURATION)
    {
      // 動きの終了
      eyeState.isMoving = false;
      eyeState.leftEye = eyeState.targetLeft;
      eyeState.rightEye = eyeState.targetRight;
    }
    else
    {
      // 動きの途中（線形補間）
      float progress = (float)elapsedTime / MOVE_DURATION;

      eyeState.leftEye.x = eyeState.originalLeft.x + (eyeState.targetLeft.x - eyeState.originalLeft.x) * progress;
      eyeState.leftEye.y = eyeState.originalLeft.y + (eyeState.targetLeft.y - eyeState.originalLeft.y) * progress;

      eyeState.rightEye.x = eyeState.originalRight.x + (eyeState.targetRight.x - eyeState.originalRight.x) * progress;
      eyeState.rightEye.y = eyeState.originalRight.y + (eyeState.targetRight.y - eyeState.originalRight.y) * progress;
    }

    // 目を更新
    drawEyes(eyeState.leftEye, eyeState.rightEye);
  }
}

void setup()
{
  M5.begin();
  Serial.begin();
  ExtDisplay.init();             // 外部ディスプレイを初期化
  ExtDisplay.setBrightness(200); // バックライトの明るさ(0-255)

  // 目の初期状態を設定
  eyeState.leftEye = {0, 0};
  eyeState.rightEye = {0, 0};
  eyeState.prevLeftEye = {0, 0};
  eyeState.prevRightEye = {0, 0};
  eyeState.isMoving = false;
  eyeState.initialized = false;
  eyeState.nextMoveTime = millis() + random(MOVE_INTERVAL_MIN, MOVE_INTERVAL_MAX + 1);

  // 初期描画
  drawInitialEyes();
}

void loop()
{
  M5.update();
  updateEyePosition();
  delay(16); // 約60FPS
}
