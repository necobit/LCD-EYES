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
constexpr int EYE_RADIUS = 50;                       // 目の半径
constexpr int EYE_SPACING = 210;                     // 目の間隔
constexpr int PUPIL_RADIUS = 25;                     // 瞳の半径
constexpr int SQUARE_EYE_WIDTH = 60;                 // 四角い目の幅
constexpr int SQUARE_EYE_HEIGHT = 120;               // 四角い目の高さ
constexpr int SQUARE_EYE_RADIUS = 5;                 // 四角い目の角の丸み
constexpr int DISPLAY_WIDTH = 320;                   // ディスプレイの幅
constexpr int DISPLAY_HEIGHT = 240;                  // ディスプレイの高さ
constexpr int DISPLAY_CENTER_X = DISPLAY_WIDTH / 2;  // ディスプレイの中心X
constexpr int DISPLAY_CENTER_Y = DISPLAY_HEIGHT / 2; // ディスプレイの中心Y
constexpr int MOVE_INTERVAL_MIN = 2000;              // 目の動きの最小間隔（ミリ秒）
constexpr int MOVE_INTERVAL_MAX = 5000;              // 目の動きの最大間隔（ミリ秒）
constexpr int MOVE_DURATION = 200;                   // 目の動きの持続時間（ミリ秒）
constexpr int BLINK_INTERVAL = 3000;                 // 瞬きの間隔（ミリ秒）
constexpr int BLINK_DURATION = 200;                  // 瞬きの持続時間（ミリ秒）

// 色の設定
// ライブラリの定義済み色定数を使用
constexpr uint32_t SQUARE_EYE_COLOR = TFT_WHITE; // 白色

// 目のモード
enum EyeMode
{
  ROUND_EYE = 0,  // 丸い目
  SQUARE_EYE = 1, // 四角い目
  EYE_MODE_COUNT  // モードの数
};

// 目の位置情報
struct EyePosition
{
  int x;
  int y;
};

// 目の状態
struct EyeState
{
  EyePosition leftEye;          // 現在の左目の位置
  EyePosition rightEye;         // 現在の右目の位置
  EyePosition prevLeftEye;      // 前回の左目の位置
  EyePosition prevRightEye;     // 前回の右目の位置
  unsigned long nextMoveTime;   // 次の動きの時間
  bool isMoving;                // 動き中フラグ
  unsigned long moveStartTime;  // 動きの開始時間
  EyePosition targetLeft;       // 目標の左目位置
  EyePosition targetRight;      // 目標の右目位置
  EyePosition originalLeft;     // 動き開始時の左目位置
  EyePosition originalRight;    // 動き開始時の右目位置
  bool initialized;             // 初期化済みフラグ
  EyeMode mode;                 // 目のモード
  unsigned long nextBlinkTime;  // 次の瞬きの時間
  bool isBlinking;              // 瞬き中フラグ
  unsigned long blinkStartTime; // 瞬きの開始時間
  bool lookingAtCenter;         // センターを見ているかどうか
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
void drawRoundEyes(EyePosition leftPupil, EyePosition rightPupil);
void drawSquareEyes(EyePosition leftPupil, EyePosition rightPupil);

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
  // 目のモードに応じて描画関数を呼び出す
  switch (eyeState.mode)
  {
  case ROUND_EYE:
    drawRoundEyes(leftPupil, rightPupil);
    break;
  case SQUARE_EYE:
    drawSquareEyes(leftPupil, rightPupil);
    break;
  default:
    drawRoundEyes(leftPupil, rightPupil);
    break;
  }

  // 現在の位置を前回の位置として保存
  eyeState.prevLeftEye = leftPupil;
  eyeState.prevRightEye = rightPupil;
  eyeState.initialized = true;
}

// 丸い目を描画する関数
void drawRoundEyes(EyePosition leftPupil, EyePosition rightPupil)
{
  // 背景を黒で塗りつぶし
  eyesSprite.fillScreen(TFT_BLACK);

  // 瞬き中かどうかを確認
  unsigned long currentTime = millis();
  bool drawBlink = eyeState.isBlinking && (currentTime - eyeState.blinkStartTime < BLINK_DURATION);

  if (!drawBlink)
  {
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
  }
  else
  {
    // 瞬き中は細い楕円を描画
    // 左目の瞬き
    for (int i = -2; i <= 2; i++)
    {
      eyesSprite.drawFastHLine(
          DISPLAY_CENTER_X - EYE_SPACING / 2 - EYE_RADIUS,
          DISPLAY_CENTER_Y + i,
          EYE_RADIUS * 2,
          TFT_WHITE);
    }

    // 右目の瞬き
    for (int i = -2; i <= 2; i++)
    {
      eyesSprite.drawFastHLine(
          DISPLAY_CENTER_X + EYE_SPACING / 2 - EYE_RADIUS,
          DISPLAY_CENTER_Y + i,
          EYE_RADIUS * 2,
          TFT_WHITE);
    }
  }

  // スプライトを画面に転送
  eyesSprite.pushSprite(&ExtDisplay, 0, 0);
}

// 四角い目を描画する関数
void drawSquareEyes(EyePosition leftPupil, EyePosition rightPupil)
{
  // 背景を黒で塗りつぶし
  eyesSprite.fillScreen(TFT_BLACK);

  // 瞬き中かどうかを確認
  unsigned long currentTime = millis();
  bool drawBlink = eyeState.isBlinking && (currentTime - eyeState.blinkStartTime < BLINK_DURATION);

  if (!drawBlink)
  {
    // 左右の目の四角い白目部分を描画（角が丸い四角形）
    // 画面からはみ出さないように位置を調整
    int leftX = DISPLAY_CENTER_X - EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2 + leftPupil.x;
    int leftY = DISPLAY_CENTER_Y - SQUARE_EYE_HEIGHT / 2 + leftPupil.y;
    int rightX = DISPLAY_CENTER_X + EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2 + rightPupil.x;
    int rightY = DISPLAY_CENTER_Y - SQUARE_EYE_HEIGHT / 2 + rightPupil.y;

    // 画面からはみ出さないように調整
    leftX = constrain(leftX, 0, DISPLAY_WIDTH - SQUARE_EYE_WIDTH);
    leftY = constrain(leftY, 0, DISPLAY_HEIGHT - SQUARE_EYE_HEIGHT);
    rightX = constrain(rightX, 0, DISPLAY_WIDTH - SQUARE_EYE_WIDTH);
    rightY = constrain(rightY, 0, DISPLAY_HEIGHT - SQUARE_EYE_HEIGHT);

    eyesSprite.fillRoundRect(
        leftX, leftY,
        SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, 0xAFFF);

    eyesSprite.fillRoundRect(
        rightX, rightY,
        SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, 0xAFFF);
  }
  else
  {
    // 瞬き中は線を描画
    int lineY = DISPLAY_CENTER_Y + leftPupil.y;
    // 画面からはみ出さないように調整
    lineY = constrain(lineY, 0, DISPLAY_HEIGHT - 1);

    int leftStartX = DISPLAY_CENTER_X - EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2 + leftPupil.x;
    int leftEndX = DISPLAY_CENTER_X - EYE_SPACING / 2 + SQUARE_EYE_WIDTH / 2 + leftPupil.x;
    int rightStartX = DISPLAY_CENTER_X + EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2 + rightPupil.x;
    int rightEndX = DISPLAY_CENTER_X + EYE_SPACING / 2 + SQUARE_EYE_WIDTH / 2 + rightPupil.x;

    // 画面からはみ出さないように調整
    leftStartX = constrain(leftStartX, 0, DISPLAY_WIDTH - 1);
    leftEndX = constrain(leftEndX, 0, DISPLAY_WIDTH - 1);
    rightStartX = constrain(rightStartX, 0, DISPLAY_WIDTH - 1);
    rightEndX = constrain(rightEndX, 0, DISPLAY_WIDTH - 1);

    eyesSprite.drawLine(leftStartX, lineY, leftEndX, lineY, SQUARE_EYE_COLOR);
    eyesSprite.drawLine(rightStartX, lineY, rightEndX, lineY, SQUARE_EYE_COLOR);
  }

  // スプライトを画面に転送
  eyesSprite.pushSprite(&ExtDisplay, 0, 0);
}

// 目の位置を更新する関数
void updateEyePosition()
{
  unsigned long currentTime = millis();

  // ボタン押下で目のモード切替
  if (M5.BtnA.wasPressed())
  {
    eyeState.mode = (EyeMode)((eyeState.mode + 1) % EYE_MODE_COUNT);
    // モード変更時に目を再描画
    drawEyes(eyeState.leftEye, eyeState.rightEye);
  }

  // 瞬き処理（両方のモードで共通）
  // 瞬きの開始判定
  if (!eyeState.isBlinking && currentTime >= eyeState.nextBlinkTime)
  {
    eyeState.isBlinking = true;
    eyeState.blinkStartTime = currentTime;
    // 瞬きの持続時間は固定で0.2秒、次の瞬きは3秒後
    eyeState.nextBlinkTime = currentTime + BLINK_DURATION + BLINK_INTERVAL;
  }

  // 瞬きの終了判定（0.2秒で確実に終了）
  if (eyeState.isBlinking && (currentTime - eyeState.blinkStartTime >= BLINK_DURATION))
  {
    eyeState.isBlinking = false;
    // 瞬きが終わったら強制的に再描画して元の目に戻す
    drawEyes(eyeState.leftEye, eyeState.rightEye);
  }

  // 動きの開始判定
  if (!eyeState.isMoving && currentTime >= eyeState.nextMoveTime)
  {
    eyeState.isMoving = true;
    eyeState.moveStartTime = currentTime;

    // 元の位置を保存
    eyeState.originalLeft = eyeState.leftEye;
    eyeState.originalRight = eyeState.rightEye;

    // センターを見ているかどうかで目標位置を決定
    if (eyeState.lookingAtCenter)
    {
      // センターを見ている場合は、ランダムな位置に移動
      int maxMove;
      if (eyeState.mode == ROUND_EYE)
      {
        maxMove = EYE_RADIUS - PUPIL_RADIUS;
      }
      else
      {
        // 四角い目の場合は、画面からはみ出さないように移動範囲を制限
        maxMove = SQUARE_EYE_WIDTH / 4; // 移動範囲を小さくする
      }

      eyeState.targetLeft.x = random(-maxMove, maxMove + 1);
      eyeState.targetLeft.y = random(-maxMove, maxMove + 1);
      eyeState.targetRight.x = eyeState.targetLeft.x; // 両目を同じ方向に動かす
      eyeState.targetRight.y = eyeState.targetLeft.y;

      // 次はセンターに戻る
      eyeState.lookingAtCenter = false;
    }
    else
    {
      // センターを見ていない場合は、センターに戻る
      eyeState.targetLeft.x = 0;
      eyeState.targetLeft.y = 0;
      eyeState.targetRight.x = 0;
      eyeState.targetRight.y = 0;

      // 次はランダムな位置に移動
      eyeState.lookingAtCenter = true;
    }

    // 次の動きの時間を設定（3秒後）
    eyeState.nextMoveTime = currentTime + MOVE_DURATION + 3000;
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
  else if (eyeState.isBlinking)
  {
    // 瞬き中は常に再描画（両方のモードで共通）
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
  eyeState.mode = SQUARE_EYE; // 初期モードは丸い目
  eyeState.isBlinking = false;
  eyeState.nextBlinkTime = millis() + BLINK_INTERVAL;
  eyeState.lookingAtCenter = true; // 初期状態はセンターを見ている

  // 初期描画
  drawInitialEyes();
}

void loop()
{
  M5.update();
  updateEyePosition();
  delay(16); // 約60FPS
}
