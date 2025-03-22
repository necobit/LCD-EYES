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
constexpr int PIN_SCL = 1;       // SCLK(SPI Clock) (SCL)
constexpr int PIN_SDA = 3;       // MOSI (SDA)
constexpr int PIN_RST = 5;       // Reset
constexpr int PIN_DC = 7;        // Data/Command
constexpr int PIN_BLK = 44;      // Backlight
constexpr int PIN_CS = 43;       // Chip Select
constexpr int PIN_WINKER_R = 9;  // ウィンカー右
constexpr int PIN_WINKER_L = 11; // ウィンカー左
constexpr int PIN_TOUCH1 = 12;   // タッチ1
constexpr int PIN_TOUCH2 = 42;   // タッチ2
constexpr int PIN_TOUCH3 = 46;   // タッチ3
constexpr int PIN_HEAD = 14;     // ヘッドライト
constexpr int PIN_BRAKE = 41;    // ブレーキライト

// 目の設定
constexpr int EYE_RADIUS = 50;                       // 目の半径
constexpr int EYE_SPACING = 190;                     // 目の間隔
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
constexpr int BLINK_INTERVAL = 3100;                 // 瞬きの間隔（ミリ秒）
constexpr int BLINK_DURATION = 200;                  // 瞬きの持続時間（ミリ秒）

// 色の設定
// ライブラリの定義済み色定数を使用
constexpr uint32_t SQUARE_EYE_COLOR = TFT_WHITE; // 白色

// モード切替の定数
constexpr int NORMAL_EYE_DURATION = 9000;    // 通常の目モードの持続時間（ミリ秒）
constexpr int SLOT_MACHINE_DURATION = 10000; // スロットマシンモードの持続時間（ミリ秒）
constexpr int SLEEP_MODE_DURATION = 10000;   // おやすみモードの持続時間（ミリ秒）

// 目のモード
enum EyeMode
{
  NORMAL_EYE = 0,   // 通常の目（四角い目）
  SLOT_MACHINE = 1, // スロットマシンモード
  SLEEP_MODE = 2,   // おやすみモード
  EYE_MODE_COUNT    // モードの数
};

// スロットマシンの状態
enum SlotState
{
  SLOT_START,    // 開始状態
  SLOT_SPINNING, // 回転中
  SLOT_RESULT,   // 結果表示中
  SLOT_END       // 終了状態
};

// おやすみモードの状態
enum SleepState
{
  SLEEP_START,   // 開始状態
  SLEEP_NORMAL,  // 通常の目を表示
  SLEEP_CLOSING, // 目を閉じている途中
  SLEEP_DIMMING, // 画面を暗くしている途中
  SLEEP_COMPLETE // 完全に暗くなった状態
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
  unsigned long modeStartTime;  // モード開始時間
  SlotState slotState;          // スロットマシンの状態
  unsigned long slotStartTime;  // スロット開始時間
  int slotNumber;               // スロットの結果の数字
  SleepState sleepState;        // おやすみモードの状態
  unsigned long sleepStartTime; // おやすみモード開始時間
  int brightness;               // 画面の明るさ（おやすみモード用）
};

// ウィンカー制御用の変数
unsigned long lastWinkerToggleTime = 0;    // 最後にウィンカーの状態を切り替えた時間
bool winkerState = false;                  // ウィンカーの現在の状態
constexpr int WINKER_BLINK_INTERVAL = 500; // ウィンカー点滅間隔（ミリ秒）

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
void drawNormalEyes(EyePosition leftPupil, EyePosition rightPupil);
void drawSlotMachine();
void drawSleepMode();
void updateWinkers(); // ウィンカー制御用の関数
void updateMode();    // モード更新用の関数

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
  case NORMAL_EYE:
    drawNormalEyes(leftPupil, rightPupil);
    break;
  case SLOT_MACHINE:
    drawSlotMachine();
    break;
  case SLEEP_MODE:
    drawSleepMode();
    break;
  default:
    drawNormalEyes(leftPupil, rightPupil);
    break;
  }

  // 現在の位置を前回の位置として保存
  eyeState.prevLeftEye = leftPupil;
  eyeState.prevRightEye = rightPupil;
  eyeState.initialized = true;
}

// 通常の目（四角い目）を描画する関数
void drawNormalEyes(EyePosition leftPupil, EyePosition rightPupil)
{
  // 背景を黒で塗りつぶし
  eyesSprite.fillScreen(TFT_BLACK);

  // 瞬き中かどうかを確認
  unsigned long currentTime = millis();
  bool drawBlink = eyeState.isBlinking && (currentTime - eyeState.blinkStartTime < BLINK_DURATION);

  if (!drawBlink)
  {
    // 左右の目の白目部分を描画（四角形）
    int leftEyeX = DISPLAY_CENTER_X - EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2 + leftPupil.x;
    int rightEyeX = DISPLAY_CENTER_X + EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2 + rightPupil.x;
    int eyeY = DISPLAY_CENTER_Y - SQUARE_EYE_HEIGHT / 2 + leftPupil.y;

    // 角丸四角形で目を描画
    eyesSprite.fillRoundRect(leftEyeX, eyeY, SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, SQUARE_EYE_COLOR);
    eyesSprite.fillRoundRect(rightEyeX, eyeY, SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, SQUARE_EYE_COLOR);
  }
  else
  {
    // 瞬き中は太い線を描画（3ピクセル）
    int leftStartX = DISPLAY_CENTER_X - EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2 + leftPupil.x;
    int leftEndX = leftStartX + SQUARE_EYE_WIDTH;
    int rightStartX = DISPLAY_CENTER_X + EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2 + rightPupil.x;
    int rightEndX = rightStartX + SQUARE_EYE_WIDTH;
    int lineY = DISPLAY_CENTER_Y + leftPupil.y;

    // 画面からはみ出さないように制限
    leftStartX = constrain(leftStartX, 0, DISPLAY_WIDTH - 1);
    leftEndX = constrain(leftEndX, 0, DISPLAY_WIDTH - 1);
    rightStartX = constrain(rightStartX, 0, DISPLAY_WIDTH - 1);
    rightEndX = constrain(rightEndX, 0, DISPLAY_WIDTH - 1);

    // 3ピクセルの太さの線を描画（中央と上下に1ピクセルずつ）
    for (int i = -1; i <= 1; i++)
    {
      int y = lineY + i;
      if (y >= 0 && y < DISPLAY_HEIGHT)
      {
        eyesSprite.drawLine(leftStartX, y, leftEndX, y, SQUARE_EYE_COLOR);
        eyesSprite.drawLine(rightStartX, y, rightEndX, y, SQUARE_EYE_COLOR);
      }
    }
  }

  // スプライトを画面に転送
  eyesSprite.pushSprite(&ExtDisplay, 0, 0);
}

// スロットマシンモードを描画する関数
void drawSlotMachine()
{
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - eyeState.slotStartTime;

  // 背景を黒で塗りつぶし
  eyesSprite.fillScreen(TFT_BLACK);

  // スロットマシンの状態に応じて描画
  switch (eyeState.slotState)
  {
  case SLOT_START:
    // 開始状態：通常の目から開始し、下に流れていく
    if (elapsedTime < 1500) // 1.5秒かけて目を下に流す
    {
      // 進行度（0.0～1.0）
      float progress = elapsedTime / 1500.0f;

      // 左右の目の白目部分を描画（四角形）- 下に流れていく
      int leftEyeX = DISPLAY_CENTER_X - EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
      int rightEyeX = DISPLAY_CENTER_X + EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
      int eyeY = DISPLAY_CENTER_Y - SQUARE_EYE_HEIGHT / 2 + (int)(DISPLAY_HEIGHT * progress); // 下に移動

      // 画面内にある場合のみ描画
      if (eyeY < DISPLAY_HEIGHT)
      {
        // 角丸四角形で目を描画
        eyesSprite.fillRoundRect(leftEyeX, eyeY, SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, SQUARE_EYE_COLOR);
        eyesSprite.fillRoundRect(rightEyeX, eyeY, SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, SQUARE_EYE_COLOR);
      }

      // 同時に数字が上から流れてくる（まだ画面外）
      eyesSprite.setTextSize(10); // より大きなサイズに
      eyesSprite.setTextColor(TFT_WHITE);

      // 左目（10の位）
      for (int i = 0; i < 4; i++)
      {
        int digit = (1 + i) % 10; // 9から始まる
        // 画面上部から流れてくる（まだ見えない）- 目と同じ速度で移動
        int y = -300 + (int)(progress * DISPLAY_HEIGHT) + i * 80;
        if (y > -80 && y < DISPLAY_HEIGHT)
        {
          eyesSprite.setCursor(DISPLAY_CENTER_X - EYE_SPACING / 2 - 25, y);
          eyesSprite.printf("%d", digit);
        }
      }

      // 右目（1の位）
      for (int i = 0; i < 4; i++)
      {
        int digit = (1 + i) % 10; // 9から始まる
        // 画面上部から流れてくる（まだ見えない）- 目と同じ速度で移動
        int y = -300 + (int)(progress * DISPLAY_HEIGHT) + i * 80;
        if (y > -80 && y < DISPLAY_HEIGHT)
        {
          eyesSprite.setCursor(DISPLAY_CENTER_X + EYE_SPACING / 2 - 25, y);
          eyesSprite.printf("%d", digit);
        }
      }
    }
    else
    {
      // 次の状態へ
      eyeState.slotState = SLOT_SPINNING;
      eyeState.slotStartTime = currentTime;
      // スロットの回転時間は3000ms
      eyeState.slotNumber = 3000;
    }
    break;

  case SLOT_SPINNING:
    // 回転中：3000msの数字を回転させる
    if (elapsedTime < eyeState.slotNumber)
    {
      // ドラムリールのような表現（下から上に数字が流れる）
      eyesSprite.setTextSize(10); // より大きなサイズに
      eyesSprite.setTextColor(TFT_WHITE);

      // 左目（10の位）のドラムリール
      int leftDigit = (elapsedTime / 200) % 10; // 200msごとに切り替え（ゆっくり）
      for (int i = -2; i <= 2; i++)
      {
        int digit = (leftDigit + i + 10) % 10; // 循環させる

        // 滑らかに移動（200msのサイクルを60フレームに分割）
        float cycleProgress = (elapsedTime % 200) / 200.0f;
        int y = DISPLAY_CENTER_Y - 35 + i * 80 - (int)(cycleProgress * 80);

        // 画面内に表示される場合のみ描画
        if (y > -80 && y < DISPLAY_HEIGHT)
        {
          eyesSprite.setCursor(DISPLAY_CENTER_X - EYE_SPACING / 2 - 30, y);
          eyesSprite.printf("%d", digit);
        }
      }

      // 右目（1の位）のドラムリール
      int rightDigit = (elapsedTime / 150) % 10; // 150msごとに切り替え（左より速く）
      for (int i = -2; i <= 2; i++)
      {
        int digit = (rightDigit + i + 10) % 10; // 循環させる

        // 滑らかに移動（150msのサイクルを60フレームに分割）
        float cycleProgress = (elapsedTime % 150) / 150.0f;
        int y = DISPLAY_CENTER_Y - 35 + i * 80 - (int)(cycleProgress * 80);

        // 画面内に表示される場合のみ描画
        if (y > -80 && y < DISPLAY_HEIGHT)
        {
          eyesSprite.setCursor(DISPLAY_CENTER_X + EYE_SPACING / 2 - 30, y);
          eyesSprite.printf("%d", digit);
        }
      }
    }
    else
    {
      // 回転終了、結果を決定
      eyeState.slotNumber = random(1, 21); // 01から20までのランダムな数字
      eyeState.slotState = SLOT_RESULT;
      eyeState.slotStartTime = currentTime;
    }
    break;

  case SLOT_RESULT:
    // 結果表示：3秒間結果を表示
    if (elapsedTime < 3000)
    {
      eyesSprite.setTextSize(10); // より大きなサイズに
      eyesSprite.setTextColor(TFT_WHITE);

      // 結果の数字を取得
      int tens = eyeState.slotNumber / 10; // 10の位
      int ones = eyeState.slotNumber % 10; // 1の位

      // 左目に10の位を表示
      eyesSprite.setCursor(DISPLAY_CENTER_X - EYE_SPACING / 2 - 30, DISPLAY_CENTER_Y - 35);
      eyesSprite.printf("%d", tens);

      // 右目に1の位を表示
      eyesSprite.setCursor(DISPLAY_CENTER_X + EYE_SPACING / 2 - 30, DISPLAY_CENTER_Y - 35);
      eyesSprite.printf("%d", ones);
    }
    else
    {
      // 結果表示終了、終了状態へ
      eyeState.slotState = SLOT_END;
      eyeState.slotStartTime = currentTime;
    }
    break;

  case SLOT_END:
    // 終了状態：数字が上に消えて目が中央に表示される
    if (elapsedTime < 1500)
    {
      // 進行度（0.0～1.0）
      float progress = elapsedTime / 1500.0f;

      if (progress < 0.5f)
      { // 最初の50%の時間は数字が上に流れる
        // 数字が上に流れていく
        eyesSprite.setTextSize(10);
        eyesSprite.setTextColor(TFT_WHITE);

        // 左目（10の位）の数字が上に流れる
        int tens = eyeState.slotNumber / 10;
        int leftY = DISPLAY_CENTER_Y - 35 - (int)((progress / 0.5f) * DISPLAY_HEIGHT);
        if (leftY > -80 && leftY < DISPLAY_HEIGHT)
        {
          eyesSprite.setCursor(DISPLAY_CENTER_X - EYE_SPACING / 2 - 30, leftY);
          eyesSprite.printf("%d", tens);
        }

        // 右目（1の位）の数字が上に流れる
        int ones = eyeState.slotNumber % 10;
        int rightY = DISPLAY_CENTER_Y - 35 - (int)((progress / 0.5f) * DISPLAY_HEIGHT);
        if (rightY > -80 && rightY < DISPLAY_HEIGHT)
        {
          eyesSprite.setCursor(DISPLAY_CENTER_X + EYE_SPACING / 2 - 30, rightY);
          eyesSprite.printf("%d", ones);
        }
      }
      // 後半で目が上から流れてきて中央で止まる
      else if (progress >= 0.5f)
      {                                               // 後半50%で目が現れる（オーバーラップなし）
        float eyeProgress = (progress - 0.5f) / 0.5f; // 0.0～1.0に正規化

        // 目の位置を計算（上から中央に移動し、中央で停止）
        int eyeY;
        if (eyeProgress < 0.8f)
        { // 最初の80%で上から中央に移動
          eyeY = -SQUARE_EYE_HEIGHT + (int)((DISPLAY_CENTER_Y - SQUARE_EYE_HEIGHT / 2 + SQUARE_EYE_HEIGHT) * eyeProgress / 0.8f);
        }
        else
        {
          // 残りの20%は中央で停止
          eyeY = DISPLAY_CENTER_Y - SQUARE_EYE_HEIGHT / 2;
        }

        int leftEyeX = DISPLAY_CENTER_X - EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
        int rightEyeX = DISPLAY_CENTER_X + EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;

        // 画面内にある場合のみ描画
        if (eyeY > -SQUARE_EYE_HEIGHT && eyeY < DISPLAY_HEIGHT)
        {
          eyesSprite.fillRoundRect(leftEyeX, eyeY, SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, SQUARE_EYE_COLOR);
          eyesSprite.fillRoundRect(rightEyeX, eyeY, SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, SQUARE_EYE_COLOR);
        }
      }
    }
    else
    {
      // 終了後は通常の目を中央に表示したまま待機
      int leftEyeX = DISPLAY_CENTER_X - EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
      int rightEyeX = DISPLAY_CENTER_X + EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
      int eyeY = DISPLAY_CENTER_Y - SQUARE_EYE_HEIGHT / 2;

      eyesSprite.fillRoundRect(leftEyeX, eyeY, SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, SQUARE_EYE_COLOR);
      eyesSprite.fillRoundRect(rightEyeX, eyeY, SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, SQUARE_EYE_COLOR);
    }
    break;
  }

  // スプライトを画面に転送
  eyesSprite.pushSprite(&ExtDisplay, 0, 0);
}

// おやすみモードを描画する関数
void drawSleepMode()
{
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - eyeState.sleepStartTime;

  // 背景を黒で塗りつぶし
  eyesSprite.fillScreen(TFT_BLACK);

  // 線を描画するための変数を事前に宣言
  int leftStartX, leftEndX, rightStartX, rightEndX, lineY;

  // おやすみモードの状態に応じて描画
  switch (eyeState.sleepState)
  {
  case SLEEP_START:
    // 開始状態：通常の目から開始
    eyeState.sleepState = SLEEP_NORMAL;
    eyeState.sleepStartTime = currentTime;
    eyeState.brightness = 200; // 初期の明るさ
    break;

  case SLEEP_NORMAL:
    // 通常の四角い目を3秒間表示
    // 左右の目の白目部分を描画（四角形）
    {
      int leftEyeX = DISPLAY_CENTER_X - EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
      int rightEyeX = DISPLAY_CENTER_X + EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
      int eyeY = DISPLAY_CENTER_Y - SQUARE_EYE_HEIGHT / 2;

      // 角丸四角形で目を描画
      eyesSprite.fillRoundRect(leftEyeX, eyeY, SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, SQUARE_EYE_COLOR);
      eyesSprite.fillRoundRect(rightEyeX, eyeY, SQUARE_EYE_WIDTH, SQUARE_EYE_HEIGHT, SQUARE_EYE_RADIUS, SQUARE_EYE_COLOR);
    }

    // 3秒後に次の状態へ
    if (elapsedTime > 3000)
    {
      eyeState.sleepState = SLEEP_CLOSING;
      eyeState.sleepStartTime = currentTime;
    }
    break;

  case SLEEP_CLOSING:
    // 目を閉じる：瞬きと同じ表現
    // 左右の目の線を描画
    leftStartX = DISPLAY_CENTER_X - EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
    leftEndX = leftStartX + SQUARE_EYE_WIDTH;
    rightStartX = DISPLAY_CENTER_X + EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
    rightEndX = rightStartX + SQUARE_EYE_WIDTH;
    lineY = DISPLAY_CENTER_Y;

    // 画面からはみ出さないように制限
    leftStartX = constrain(leftStartX, 0, DISPLAY_WIDTH - 1);
    leftEndX = constrain(leftEndX, 0, DISPLAY_WIDTH - 1);
    rightStartX = constrain(rightStartX, 0, DISPLAY_WIDTH - 1);
    rightEndX = constrain(rightEndX, 0, DISPLAY_WIDTH - 1);

    // 3ピクセルの太さの線を描画（中央と上下に1ピクセルずつ）
    for (int i = -1; i <= 1; i++)
    {
      int y = lineY + i;
      if (y >= 0 && y < DISPLAY_HEIGHT)
      {
        eyesSprite.drawLine(leftStartX, y, leftEndX, y, SQUARE_EYE_COLOR);
        eyesSprite.drawLine(rightStartX, y, rightEndX, y, SQUARE_EYE_COLOR);
      }
    }

    // 0.5秒後に次の状態へ
    if (elapsedTime > 500)
    {
      eyeState.sleepState = SLEEP_DIMMING;
      eyeState.sleepStartTime = currentTime;
    }
    break;

  case SLEEP_DIMMING:
    // 画面を徐々に暗くする（2秒かけて）
    if (elapsedTime < 2000)
    {
      // 左右の目の線を描画（そのまま表示）
      leftStartX = DISPLAY_CENTER_X - EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
      leftEndX = leftStartX + SQUARE_EYE_WIDTH;
      rightStartX = DISPLAY_CENTER_X + EYE_SPACING / 2 - SQUARE_EYE_WIDTH / 2;
      rightEndX = rightStartX + SQUARE_EYE_WIDTH;
      lineY = DISPLAY_CENTER_Y;

      leftStartX = constrain(leftStartX, 0, DISPLAY_WIDTH - 1);
      leftEndX = constrain(leftEndX, 0, DISPLAY_WIDTH - 1);
      rightStartX = constrain(rightStartX, 0, DISPLAY_WIDTH - 1);
      rightEndX = constrain(rightEndX, 0, DISPLAY_WIDTH - 1);

      // 3ピクセルの太さの線を描画（中央と上下に1ピクセルずつ）
      for (int i = -1; i <= 1; i++)
      {
        int y = lineY + i;
        if (y >= 0 && y < DISPLAY_HEIGHT)
        {
          eyesSprite.drawLine(leftStartX, y, leftEndX, y, SQUARE_EYE_COLOR);
          eyesSprite.drawLine(rightStartX, y, rightEndX, y, SQUARE_EYE_COLOR);
        }
      }

      // 明るさを徐々に下げる
      eyeState.brightness = 200 - (int)(200.0 * elapsedTime / 2000.0);
      ExtDisplay.setBrightness(eyeState.brightness);
    }
    else
    {
      // 完全に暗くなったら次の状態へ
      eyeState.sleepState = SLEEP_COMPLETE;
      eyeState.sleepStartTime = currentTime;
      ExtDisplay.setBrightness(0); // 完全に暗く
    }
    break;

  case SLEEP_COMPLETE:
    // 完全に暗くなった状態（何も表示しない）
    // 次のモード切替まで待機
    break;
  }

  // スプライトを画面に転送
  eyesSprite.pushSprite(&ExtDisplay, 0, 0);
}

// モードを更新する関数
void updateMode()
{
  unsigned long currentTime = millis();

  // モード開始時間が設定されていない場合は初期化
  if (eyeState.modeStartTime == 0)
  {
    eyeState.modeStartTime = currentTime;
    return;
  }

  // 現在のモードに応じた持続時間を取得
  unsigned long modeDuration;
  switch (eyeState.mode)
  {
  case NORMAL_EYE:
    modeDuration = NORMAL_EYE_DURATION;
    break;
  case SLOT_MACHINE:
    modeDuration = SLOT_MACHINE_DURATION;
    break;
  case SLEEP_MODE:
    modeDuration = SLEEP_MODE_DURATION;
    break;
  default:
    modeDuration = NORMAL_EYE_DURATION;
    break;
  }

  // 設定された時間ごとにモードを切り替え
  if (currentTime - eyeState.modeStartTime >= modeDuration)
  {
    // 次のモードに切り替え
    eyeState.mode = (EyeMode)((eyeState.mode + 1) % EYE_MODE_COUNT);
    eyeState.modeStartTime = currentTime;

    // モード固有の初期化
    switch (eyeState.mode)
    {
    case NORMAL_EYE:
      // 通常モードに戻る時は明るさを元に戻す
      ExtDisplay.setBrightness(200);
      break;

    case SLOT_MACHINE:
      // スロットマシンモードの初期化
      eyeState.slotState = SLOT_START;
      eyeState.slotStartTime = currentTime;
      break;

    case SLEEP_MODE:
      // おやすみモードの初期化
      eyeState.sleepState = SLEEP_START;
      eyeState.sleepStartTime = currentTime;
      break;
    }
  }
}

// 目の位置を更新する関数
void updateEyePosition()
{
  unsigned long currentTime = millis();

  // モードの更新
  updateMode();

  // 通常モードの場合のみ瞬きと目の動きを更新
  if (eyeState.mode == NORMAL_EYE)
  {
    // 瞬き処理
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
        int maxMove = SQUARE_EYE_WIDTH / 4; // 移動範囲を制限

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
      // 瞬き中は常に再描画
      drawEyes(eyeState.leftEye, eyeState.rightEye);
    }
  }
  else
  {
    // 他のモードの場合は常に再描画
    drawEyes(eyeState.leftEye, eyeState.rightEye);
  }
}

// ウィンカーを制御する関数
void updateWinkers()
{
  // 各タッチセンサーの状態を読み取る
  bool touch1Detected = digitalRead(PIN_TOUCH1) == HIGH; // ウィンカー用
  bool touch2Detected = digitalRead(PIN_TOUCH2) == HIGH; // ブレーキライト用
  bool touch3Detected = digitalRead(PIN_TOUCH3) == HIGH; // ヘッドライト用

  // 現在の時間を取得
  unsigned long currentTime = millis();

  // タッチ1（ウィンカー）の処理
  if (touch1Detected)
  {
    // タッチが検出された場合、ウィンカーを点滅させる
    if (currentTime - lastWinkerToggleTime >= WINKER_BLINK_INTERVAL)
    {
      // 点滅間隔が経過したら状態を切り替え
      winkerState = !winkerState;
      digitalWrite(PIN_WINKER_R, winkerState ? HIGH : LOW);
      digitalWrite(PIN_WINKER_L, winkerState ? HIGH : LOW);
      lastWinkerToggleTime = currentTime;
    }
  }
  else
  {
    // タッチが検出されていない場合、ウィンカーをOFFにする
    if (winkerState)
    {
      digitalWrite(PIN_WINKER_R, LOW);
      digitalWrite(PIN_WINKER_L, LOW);
      winkerState = false;
    }
  }

  // タッチ2（ブレーキライト）の処理
  digitalWrite(PIN_BRAKE, touch2Detected ? LOW : HIGH);

  // タッチ3（ヘッドライト）の処理
  digitalWrite(PIN_HEAD, touch3Detected ? LOW : HIGH);
}

void setup()
{
  M5.begin();
  // Serial.begin();
  ExtDisplay.init();             // 外部ディスプレイを初期化
  ExtDisplay.setBrightness(200); // バックライトの明るさ(0-255)

  // ピンの初期化
  pinMode(PIN_WINKER_R, OUTPUT);
  pinMode(PIN_WINKER_L, OUTPUT);
  pinMode(PIN_HEAD, OUTPUT);
  pinMode(PIN_BRAKE, OUTPUT);
  pinMode(PIN_TOUCH1, INPUT);
  pinMode(PIN_TOUCH2, INPUT);
  pinMode(PIN_TOUCH3, INPUT);

  // 出力ピンの初期状態を設定
  digitalWrite(PIN_WINKER_R, LOW);
  digitalWrite(PIN_WINKER_L, LOW);
  digitalWrite(PIN_HEAD, HIGH);  // 初期状態はHIGH
  digitalWrite(PIN_BRAKE, HIGH); // 初期状態はHIGH

  // 目の初期状態を設定
  eyeState.leftEye = {0, 0};
  eyeState.rightEye = {0, 0};
  eyeState.prevLeftEye = {0, 0};
  eyeState.prevRightEye = {0, 0};
  eyeState.isMoving = false;
  eyeState.initialized = false;
  eyeState.nextMoveTime = millis() + random(MOVE_INTERVAL_MIN, MOVE_INTERVAL_MAX + 1);
  eyeState.mode = NORMAL_EYE; // 初期モードは通常の目
  eyeState.isBlinking = false;
  eyeState.nextBlinkTime = millis() + BLINK_INTERVAL;
  eyeState.lookingAtCenter = true; // 初期状態はセンターを見ている
  eyeState.modeStartTime = 0;      // モード開始時間（updateMode()で初期化される）
  eyeState.slotState = SLOT_START;
  eyeState.sleepState = SLEEP_START;

  // 初期描画
  drawInitialEyes();
}

void loop()
{
  M5.update();
  updateEyePosition();
  updateWinkers(); // ウィンカー制御を更新
  delay(16);       // 約60FPS
}
