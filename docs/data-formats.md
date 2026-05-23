# Data Formats

このドキュメントは、JSON と Lua の定義形式を説明します。

## 先に読んでおくとよいもの

- `docs/programming-guide.md`
- `docs/team-programming-guide.md`

## `assets/manifest.json`

役割:

- テクスチャ ID を構築する
- 現在はファイル読み込みではなく、生成テクスチャを定義する

### ルート構造

```json
{
  "textures": {
    "white": {
      "type": "solid",
      "width": 1,
      "height": 1,
      "rgba": "FFFFFFFF"
    },
    "player": {
      "type": "checkerboard",
      "width": 256,
      "height": 256,
      "rgbaA": "FF4EC9B0",
      "rgbaB": "FF1B3340",
      "cellSize": 32
    }
  }
}
```

### `type: solid`

使用キー:

- `width`
- `height`
- `rgba`

### `type: checkerboard`

使用キー:

- `width`
- `height`
- `rgbaA`
- `rgbaB`
- `cellSize`

### 色の形式

- 16 進数 8 桁
- 並びは `AARRGGBB`

例:

- `FFFFFFFF`: 白
- `FF000000`: 黒
- `FF4EC9B0`: 不透明の緑系

## `assets/prefabs.json`

役割:

- エンティティ構成を JSON で定義する

### ルート構造

```json
{
  "prefabs": {
    "player": {
      "tag": "Player",
      "texture": "player",
      "transform": {
        "x": 400.0,
        "y": 220.0,
        "width": 256.0,
        "height": 256.0,
        "rotation": 0.0,
        "scale": 1.0
      },
      "tint": {
        "r": 1.0,
        "g": 1.0,
        "b": 1.0,
        "a": 1.0
      },
      "rigidBody": {
        "enabled": true,
        "type": "dynamic",
        "fixedRotation": true,
        "gravityScale": 0.0
      },
      "collider": {
        "enabled": true,
        "density": 1.0,
        "friction": 0.2,
        "isSensor": false
      },
      "controller": {
        "player": true
      }
    }
  }
}
```

### 使用フィールド

#### 直下

- `tag`
- `texture`

#### `transform`

- `x`
- `y`
- `width`
- `height`
- `rotation`
- `scale`

#### `tint`

- `r`
- `g`
- `b`
- `a`

#### `rigidBody`

- `enabled`
- `type`
  値は `static` / `dynamic` / `kinematic`
- `fixedRotation`
- `gravityScale`

#### `collider`

- `enabled`
- `density`
- `friction`
- `isSensor`

#### `controller`

- `player`

### 注意点

- `texture` は `manifest.json` 側のキーと一致させる必要があります
- `controller.player = true` を付けると `PlayerControllerComponent` が追加されます
- `rigidBody.enabled = true` なのに `collider.enabled = false` でも生成自体はできます

### 現状の運用注意

このプロジェクトでは、配置物の構成と配置座標はまだ完全分離ではありません。  
ただし `GameScene` の主要配置物は `sandbox_*` prefab から生成する形へ移行しています。

つまり、

- prefab の構成変更は `prefabs.json`
- どの prefab をどこへ置くかは `game_scene_setup.cpp`

の両方を見る前提です。  
JSON だけでレベル全体が完結する構造ではまだありません。

### フィルターまわり

現在のフィルター本体はコード側が主です。  
今あるテーマは次です。

- `None`
- `Hot`
- `Cold`
- `Invert`
- `Sepia`

効果は主に `game_scene_gameplay.cpp` で処理しています。  
そのため、`prefabs.json` だけでフィルター仕様を完結させる構造ではありません。

## `assets/demo_scene.lua`

役割:

- サンプルシーンの簡単なロジックや演出を記述する

### 現在のグローバル変数

- `time`
- `target_x`
- `target_y`
- `sound_timer`
- `logged_startup`

### 現在の必須関数

```lua
function update(dt)
end
```

`ScriptEngine` は毎フレーム `update(dt)` を探して呼びます。  
存在しない場合は何もしません。

## Lua から使える API

現在は EventBus へ要求を積む API だけを公開しています。

### `log_message(message)`

ログメッセージを発行します。

```lua
log_message("Lua scene script active")
```

### `request_sound(cue_name)`

音再生要求イベントを発行します。

```lua
request_sound("test_tone")
```

### `request_scene_change(scene_id)`

シーン切り替え要求イベントを発行します。

```lua
request_scene_change("demo")
```

## フォールバック

JSON が読めない場合の挙動は以下です。

- `manifest.json` が無い
  ビルトインの `white`、`player`、`target` テクスチャを使う
- `prefabs.json` が無いか壊れている
  ビルトインの `player`、`target` プレハブを使う

このため、最低限のサンプルは外部データが壊れても起動できます。

## 補足

もし「データだけで調整したい」ときは、先に次のどちらなのかを分けてください。

- 見た目や数値の調整
- ルールの変更

前者は JSON で済むことがありますが、後者はほぼコード変更が必要です。

### `assets/tuning.json` の写真サイズ系キー

写真サイズの調整でよく触るもの:

- `capture_width_tiles`
- `capture_height_tiles`
- `printed_photo_padding_x`
- `printed_photo_padding_top`
- `printed_photo_footer_height`
- `printed_photo_min_width`
- `printed_photo_min_height`
- `printed_photo_matte_inset`

意味:

- `capture_*`
  撮影フレームのタイル数
- `printed_photo_*`
  貼り付け時のポラロイド台紙サイズ

## `assets/input_bindings.json`

役割:

- 入力アクションのキーボード / マウス / ゲームパッド割り当てを上書きする
- `core/input.cpp` のデフォルト定義に対して差分適用する

### ルート構造

```json
{
  "actions": {
    "Jump": {
      "pressed_keys": ["SPACE", "W", "UP"],
      "pressed_gamepad": ["GAMEPAD_SOUTH_PRESSED"]
    },
    "HoldCamera": {
      "down_keys": ["RBUTTON"],
      "down_gamepad": ["GAMEPAD_LEFT_TRIGGER_DOWN"],
      "down_falls_back_to_pressed": false
    }
  }
}
```

### `actions.<ActionName>` で使えるキー

- `down_keys`
  `Input_IsActionDown` 判定に使うキー配列
- `pressed_keys`
  `Input_IsActionPressed` 判定に使うキー配列
- `down_gamepad`
  `Input_IsActionDown` 判定に使うゲームパッド述語配列
- `pressed_gamepad`
  `Input_IsActionPressed` 判定に使うゲームパッド述語配列
- `down_falls_back_to_pressed`
  `true` のとき、`down_keys/down_gamepad` が無効なら `pressed_*` 判定へフォールバックする

### キー名 (`down_keys` / `pressed_keys`)

主な文字列:

- 単文字: `A`〜`Z`, `0`〜`9`
- 特殊: `ENTER`, `SPACE`, `ESCAPE`, `LEFT`, `RIGHT`, `UP`, `DOWN`
- 修飾: `SHIFT`, `LSHIFT`, `RSHIFT`
- マウス: `LBUTTON`, `RBUTTON`
- ファンクション: `F1`〜`F12`
- 無効化: `NONE`

数値 (0〜255) も指定できます。

### ゲームパッド述語名 (`down_gamepad` / `pressed_gamepad`)

- `GAMEPAD_SOUTH_PRESSED`
- `GAMEPAD_EAST_PRESSED`
- `GAMEPAD_NORTH_PRESSED`
- `GAMEPAD_BACK_PRESSED`
- `GAMEPAD_LEFT_TRIGGER_DOWN`
- `GAMEPAD_RIGHT_TRIGGER_DOWN`
- `GAMEPAD_RIGHT_TRIGGER_PRESSED`
- `GAMEPAD_LEFT_SHOULDER_DOWN`
- `GAMEPAD_RIGHT_SHOULDER_DOWN`
- `GAMEPAD_LEFT_SHOULDER_PRESSED`
- `GAMEPAD_RIGHT_SHOULDER_PRESSED`
- `NONE`

### 運用メモ

- ファイルが無い場合はデフォルト割り当てを使います。
- 未知の Action 名 / キー名 / 述語名は `foundation.log` に警告が出ます。
- 起動時に、最終的に有効になった全アクションのバインド一覧をログ出力します。
