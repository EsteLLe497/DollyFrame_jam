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

このプロジェクトでは `prefabs.json` があっても、すべてのゲームオブジェクトが prefab から置かれているわけではありません。  
特に `GameScene` の主要配置物は、まだ `game_scene.cpp` で直接生成しているものがあります。

つまり、

- JSON を変えれば自動で全部変わる

という前提ではありません。  
配置やルールに関わる変更では、`GameScene` 側のコード確認も必要です。

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
