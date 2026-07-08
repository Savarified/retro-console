#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Bluepad32.h>
#include "textures.h"
#include "terrain.h"

#include "FS.h"
#include "SD_MMC.h"

struct Texture;
class Player;

struct SaveHeader {
  float playerX;
  float playerY;
};

#define TFT_CS 8
#define TFT_DC 9
#define TFT_RST 10
#define TFT_BLK 46

#define SCREEN_W 160
#define SCREEN_H 128

#define SD_CLK_PIN 39
#define SD_CMD_PIN 38
#define SD_D0_PIN  40

float anim_speed = 0.6;

float camera_x = 0;
const float CAMERA_LEFT_BOUND = 50;
const float CAMERA_RIGHT_BOUND = 100;

float crosshair_x = 0;
float crosshair_y = 0;
float crosshair_speed = 1;
int current_block_index = 1;

const int TILE_SIZE = 8;
const int PLAYER_W = 32;
const int PLAYER_H = 32;

extern uint8_t tiles[level_height][level_width];

uint8_t charToTile(char c) {
  switch (c) {
    case '#': return 1;
    case '%': return 2;
    case ']': return 3;
    case 'B': return 4;
    default:  return 0;
  }
}

bool isSolid(int tx, int ty);

class Player {
public:
    float x, y;
    float dx, dy;
    int animState;
    unsigned long lastFrameTime;
    bool facing;

    //hitbox
    static const int SPRITE_W = 32;
    static const int SPRITE_H = 32;

    static const int HITBOX_W = 12;
    static const int HITBOX_H = 16;

    static const int HITBOX_OFFSET_X = (SPRITE_W - HITBOX_W) / 2;
    static const int HITBOX_OFFSET_Y = 8;

    static constexpr float COLLISION_EPSILON = 0.001f;

    const float riseGravity = 0.35f;
    const float fallGravity = 0.35f;
    const float jumpPower = -5.0f;
    const float maxFallSpeed = 10.0f;

    bool grounded = false;

    Player(float startX = 0, float startY = 0)
    {
        x = startX;
        y = startY;

        dx = 0;
        dy = 0;

        animState = 0;
        lastFrameTime = 0;
        facing = true;
        grounded = false;
    }

    float left()   { return x + HITBOX_OFFSET_X; }
    float right()  { return x + HITBOX_OFFSET_X + HITBOX_W; }
    float top()    { return y + HITBOX_OFFSET_Y; }
    float bottom() { return y + HITBOX_OFFSET_Y + HITBOX_H; }

    int tileLeft()   { return (int)floorf(left() / TILE_SIZE); }
    int tileTop()    { return (int)floorf(top() / TILE_SIZE); }

    int tileRight()  { return (int)floorf((right()  - COLLISION_EPSILON) / TILE_SIZE); }
    int tileBottom() { return (int)floorf((bottom() - COLLISION_EPSILON) / TILE_SIZE); }

    void update()
    {
        bool wasGrounded = grounded;
        grounded = false;

        x += dx;

        int l = tileLeft();
        int r = tileRight();
        int t = tileTop();
        int b = tileBottom();

        if (dx > 0)
        {
            if (isSolid(r, t) || isSolid(r, b))
            {
                x = r * TILE_SIZE - HITBOX_OFFSET_X - HITBOX_W;
            }
        }
        else if (dx < 0)
        {
            if (isSolid(l, t) || isSolid(l, b))
            {
                x = (l + 1) * TILE_SIZE - HITBOX_OFFSET_X;
            }
        }

        //gravity
        dy += (dy < 0) ? riseGravity : fallGravity;
        if (dy > maxFallSpeed) dy = maxFallSpeed;

        y += dy;

        l = tileLeft();
        r = tileRight();
        t = tileTop();
        b = tileBottom();

        bool hitGround = false;

        //falling
        if (dy > 0)
        {
            if (isSolid(l, b) || isSolid(r, b))
            {
                y = b * TILE_SIZE - HITBOX_OFFSET_Y - HITBOX_H;
                dy = 0;
                hitGround = true;
            }
        }

        else if (dy < 0)
        {
            if (t >= 0 && (isSolid(l, t) || isSolid(r, t)))
            {
                y = (t + 1) * TILE_SIZE - HITBOX_OFFSET_Y;
                dy = 0;
            }
        }

        if (hitGround)
        {
            grounded = true;
        }
        else if (wasGrounded && dy == 0)
        {
          //flickering fix
            grounded = true;
        }
    }

    void jump()
    {
        if (grounded)
        {
            dy = jumpPower;
            grounded = false;
        }
    }

    void updateAnim(float speed = 80)
    {
        if (dx > 0) facing = false;
        if (dx < 0) facing = true;

        if (dx != 0)
        {
            unsigned long now = millis();
            if (now - lastFrameTime >= speed)
            {
                animState = (animState + 1) % 8;
                lastFrameTime = now;
            }
        }
        else
        {
            animState = 0;
        }
    }
};

float walk_speed = 2;
float deadzone = 100;
Player player = (0, 50);

struct Texture {
  const uint16_t *data;
  int width;
  int height;
};
Texture walk1_tex = { walk1, 32, 32 };
Texture walk2_tex = { walk2, 32, 32 };
Texture walk3_tex = { walk3, 32, 32 };
Texture walk4_tex = { walk4, 32, 32 };

Texture walk5_tex = { walk5, 32, 32 };
Texture walk6_tex = { walk6, 32, 32 };
Texture walk7_tex = { walk7, 32, 32 };
Texture walk8_tex = { walk8, 32, 32 };

Texture walk_cycle[8] = {
  walk1_tex, walk2_tex, walk3_tex, walk4_tex,
  walk5_tex, walk6_tex, walk7_tex, walk8_tex
};
Texture crosshair_tex = {crosshair, 8, 8};

Texture grass_tex = {grass, 8, 8 };
Texture dirt_tex = {dirt, 8, 8 };
Texture stone_tex = {stone, 8, 8 };
Texture lightwood_plank_tex = {lightwood_plank, 8, 8 };
Texture darkwood_plank_tex = {darkwood_plank, 8, 8 };
Texture oak_log_tex = {oak_log, 8, 8 };
Texture light_tex = {light, 8, 8 };
Texture stone_brick_tex = {stone_brick, 8, 8 };
Texture brick_tex = {brick, 8, 8 };


void updateCamera()
{
    float screenX = player.x - camera_x;

    if (screenX > CAMERA_RIGHT_BOUND)
    {
        camera_x += screenX - CAMERA_RIGHT_BOUND;
    }

    if (screenX < CAMERA_LEFT_BOUND)
    {
        camera_x += screenX - CAMERA_LEFT_BOUND;
    }
    if (camera_x < 0) camera_x = 0;
}

uint8_t tiles[level_height][level_width];

void init_level(){
  for(int y=0;y<level_height;y++)
  {
    for(int x=0;x<level_width;x++)
    {
      switch(level[y][x])
      {
          case '.': tiles[y][x]=0; break;
          case '#': tiles[y][x]=1; break;
          case '%': tiles[y][x]=2; break;
          case ']': tiles[y][x]=3; break;
          case 'B': tiles[y][x]=4; break;
      }
    }
  }
}

bool saveLevel(const char *path) {
  SD_MMC.remove(path);
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) return false;
  SaveHeader header = { player.x, player.y };
  f.write((uint8_t *)&header, sizeof(header));
  f.write((uint8_t *)tiles, sizeof(tiles));
  f.close();
  return true;
}

void loadFromTerrain() {
  for (int y = 0; y < level_height; y++) {
    for (int x = 0; x < level_width; x++) {
      tiles[y][x] = charToTile(level[y][x]);
    }
  }
}

bool loadLevel(const char *path) {
  File f = SD_MMC.open(path);
  if (!f || f.size() != sizeof(SaveHeader) + sizeof(tiles)) {
    if (f) f.close();
    loadFromTerrain();
    player.x = 0;
    player.y = 0;
    return false;
  }
  SaveHeader header;
  f.read((uint8_t *)&header, sizeof(header));
  f.read((uint8_t *)tiles, sizeof(tiles));
  f.close();
  player.x = header.playerX;
  player.y = header.playerY;
  return true;
}

bool isSolid(int tx, int ty)
{
    if (tx < 0 || tx >= level_width) return true;
    if (ty < 0 || ty >= level_height) return false;

    return tiles[ty][tx] != 0;
}

int crosshairTileX() {
  return (int)((crosshair_x + camera_x + 4) / TILE_SIZE);
}
int crosshairTileY() {
  return (int)((crosshair_y + 4) / TILE_SIZE);
}

void destroyTile() {
  int tx = crosshairTileX();
  int ty = crosshairTileY();
  if (tx >= 0 && tx < level_width && ty >= 0 && ty < level_height) {
    tiles[ty][tx] = 0;
  }
}

void buildTile() {
  int tx = crosshairTileX();
  int ty = crosshairTileY();
  if (tx >= 0 && tx < level_width && ty >= 0 && ty < level_height) {
    if (tiles[ty][tx] != 0){
      return;
    }
    tiles[ty][tx] = current_block_index;
  }
}

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(SCREEN_W, SCREEN_H); //16-bit buffer

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

void drawTexture(GFXcanvas16 &canvas, const Texture &tex, int x, int y) {
  canvas.drawRGBBitmap(x, y, tex.data, tex.width, tex.height);
}

void drawTextureScaledFlippedTransparent(GFXcanvas16 &canvas, Texture &tex, int x, int y, int scale, uint16_t transparentColor, bool flipX) {
  canvas.startWrite(); //batches the writes, skips overhead

  for (int ty = 0; ty < tex.height; ty++) {
    const uint16_t *rowData = &tex.data[ty * tex.width];
    int srcX   = flipX ? tex.width - 1 : 0;
    int srcStep = flipX ? -1 : 1;
    int drawY = y + ty * scale;

    for (int tx = 0; tx < tex.width; tx++, srcX += srcStep) {
      uint16_t px = rowData[srcX];
      if (px == transparentColor) continue;

      int drawX = x + tx * scale;
      if (scale == 1) {
        canvas.writePixel(drawX, drawY, px);
      } else {
        canvas.writeFillRect(drawX, drawY, scale, scale, px);
      }
    }
  }

  canvas.endWrite();
}

void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      myControllers[i] = ctl;
      return;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      myControllers[i] = nullptr;
      return;
    }
  }
}

uint32_t lastButtons = 0;
uint32_t lastMiscButtons = 0;
int lastL2 = 0;
int lastR2 = 0;
const int TRIGGER_THRESHOLD = 100;

void checkButtonPress(ControllerPtr ctl) {
  uint32_t currentButtons = ctl->buttons();
  uint32_t currentMisc = ctl->miscButtons();
  int currentL2 = ctl->brake();
  int currentR2 = ctl->throttle();

  bool new_press_a = (currentButtons & BUTTON_A) && !(lastButtons & BUTTON_A);
  bool new_press_b = (currentButtons & BUTTON_B) && !(lastButtons & BUTTON_B);
  bool new_press_menu = (currentMisc & MISC_BUTTON_HOME) && !(lastMiscButtons & MISC_BUTTON_HOME);
  bool new_press_l2 = (currentL2 > TRIGGER_THRESHOLD) && (lastL2 <= TRIGGER_THRESHOLD);
  bool new_press_r2 = (currentR2 > TRIGGER_THRESHOLD) && (lastR2 <= TRIGGER_THRESHOLD);
  bool new_press_lb = (currentButtons & BUTTON_SHOULDER_L) && !(lastButtons & BUTTON_SHOULDER_L);
  bool new_press_rb = (currentButtons & BUTTON_SHOULDER_R) && !(lastButtons & BUTTON_SHOULDER_R);

  if (new_press_a) {
    player.jump();
  }

  if (new_press_b) {
  }

  if (new_press_lb) {
    current_block_index -= 1;
    if(current_block_index < 0){
      current_block_index = 9;
    }
  }

  if (new_press_rb) {
    current_block_index += 1;
    if (current_block_index > 9){
      current_block_index = 0;
    }
  }

  if (new_press_menu) {
    saveLevel("/level1.bin");
  }

  if (currentL2 > TRIGGER_THRESHOLD) {
    buildTile();
  }

  if (currentR2 > TRIGGER_THRESHOLD) {
    destroyTile();
  }

  lastButtons = currentButtons;
  lastMiscButtons = currentMisc;
  lastL2 = currentL2;
  lastR2 = currentR2;
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  tft.fillScreen(0x0000);

  canvas.setTextColor(0xFFFF);
  canvas.setCursor(10, 10);
  canvas.print("Boot complete!");
  pushCanvas();

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();

  SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN);
  SD_MMC.begin("/sdcard", true);

  loadLevel("/level1.bin");
}

void updateCrosshair(int rx, int ry){
  if (rx < -deadzone) {
      crosshair_x -= crosshair_speed;
    }
    else if (rx > deadzone) {
      crosshair_x += crosshair_speed;
    }

    if (ry < -deadzone) {
      crosshair_y -= crosshair_speed;
    }
    else if (ry > deadzone) {
      crosshair_y += crosshair_speed;
    }

    if (crosshair_x > (SCREEN_W - 8)){
      crosshair_x -= crosshair_speed;
    }
    if (crosshair_x < 0){
      crosshair_x += crosshair_speed;
    }
    if(crosshair_y > (SCREEN_H - 8)){
      crosshair_y -= crosshair_speed;
    }
    if(crosshair_y < 0){
      crosshair_y += crosshair_speed;
    }
}

void pushCanvas() {
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), SCREEN_W, SCREEN_H);
}

Texture indexToTex(int n){
  Texture block_tex = grass_tex;
  switch(n)
  {
    case 0:
      break;
    case 1:
      block_tex = grass_tex;
      break;
    case 2:
      block_tex = dirt_tex;
      break;
    case 3:
      block_tex = stone_tex;
      break;
    case 4:
      block_tex = lightwood_plank_tex;
      break;
    case 5:
      block_tex = darkwood_plank_tex;
      break;
    case 6:
      block_tex = oak_log_tex;
      break;
    case 7:
      block_tex = light_tex;
      break;
    case 8:
      block_tex = stone_brick_tex;
      break;
    case 9:
      block_tex = brick_tex;
      break;
  }
  return block_tex;
}

void render() {
  // visible column range
  int firstCol = (int)(camera_x / TILE_SIZE) - 1;      //-1 for partial tile at left edge
  int lastCol  = (int)((camera_x + SCREEN_W) / TILE_SIZE) + 1; //+1 for partial tile at right edge

  if (firstCol < 0) firstCol = 0;
  if (lastCol >= level_width) lastCol = level_width - 1;

  for (int y = 0; y < level_height; y++)
  {
    for (int x = firstCol; x <= lastCol; x++)
    {
        if (tiles[y][x] == 0) continue;
        Texture block_tex = indexToTex(tiles[y][x]);
        drawTexture(canvas, block_tex, (x * 8) - camera_x, y * 8);
    }
  }
}

void drawPlayer(GFXcanvas16 &canvas, Player &p, int scale, uint16_t keyColor, float camX)
{
  Texture &currentFrame = walk_cycle[p.animState];
  bool flip = (p.facing == 1);

  drawTextureScaledFlippedTransparent(
    canvas,
    currentFrame,
    (int)(p.x - camX),
    (int)p.y,
    scale,
    keyColor,
    flip
  );
}

void loop() {
  BP32.update();

  ControllerPtr ctl = nullptr;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] && myControllers[i]->isConnected()) {
      ctl = myControllers[i];
      break;
    }
  }

  canvas.fillScreen(0x263f); //clear buffer

  canvas.setCursor(10, 10);
  canvas.setTextColor(0xFFFF);

  if (ctl) {
    checkButtonPress(ctl);

    int lx = ctl->axisX();
    int ly = ctl->axisY();

    int rx = ctl->axisRX();
    int ry = ctl->axisRY();

    if (lx < -deadzone){
      player.dx = -walk_speed;
    }
    else if (lx > deadzone){
      player.dx = walk_speed;
    }
    else {
      player.dx = 0;
    }

    
    int dotX = SCREEN_W / 2 + map(lx, -512, 512, -40, 40);
    int dotY = SCREEN_H / 2 + map(ly, -512, 512, -30, 30);

    updateCrosshair(rx, ry);

    render();

    drawPlayer(canvas, player, 1, 0xf818, camera_x);
    player.update();
    player.updateAnim();
    drawTexture(canvas, indexToTex(current_block_index), 4, 4);
    drawTextureScaledFlippedTransparent(canvas, crosshair_tex, crosshair_x, crosshair_y, 1, 0xf818, false);
    updateCamera();
  } else {
    canvas.print("Waiting for controller...");
  }

  pushCanvas();

  delay(16);
}
