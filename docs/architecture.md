# Architecture

このドキュメントは、`DirectXFoundation` の実行構造と責務分離を説明します。

## 関連ドキュメント

- `docs/programming-guide.md`
  現在の実装に合わせた読み方
- `docs/team-programming-guide.md`
  チーム制作向けの作業ガイド

## 全体像

フレームの流れは概ね以下です。

1. `main.cpp` が `Application` を起動する
2. `Application` が Win32 / DirectX / ミドルウェアを初期化する
3. `SceneRegistry` から初期シーンを生成する
4. 毎フレーム `SceneManager::Update()` を呼ぶ
5. シーンが `EventBus` にイベントを積む
6. `Application` がイベントを消費し、音再生やシーン遷移を実行する
7. `SceneManager::Draw()` と `ImGui` を描画する

## クラス責務

### `Application`

責務:

- アプリ全体のライフサイクル管理
- Win32 メッセージ処理
- DirectX とミドルウェアの初期化と破棄
- 現在シーンの更新と描画
- `EventBus` の最終消費

`Application` はゲーム内容を持たない層です。  
シーン固有の分岐を極力持たず、登録済みシーン ID とイベント種別だけを見る構造を維持します。

### `Scene`

責務:

- 1 つのゲーム進行単位を表現する
- そのシーンに属するエンティティ群を管理する
- `EventBus` を所有し、シーン内イベントを受け渡す

`Scene` は抽象基底で、派生側が `GetSceneId()` を返します。

現状で最もゲームロジックが大きいのは `GameScene` です。  
`GameScene` はさらに次の単位へ分割されています。

- `game_scene_photo_state.h`
- `game_scene_state.h`
- `game_scene.cpp`
- `game_scene_gameplay.cpp`
- `game_scene_photo_tray_system.h`
- `game_scene_combat_system.h`
- `game_scene_player_system.h`
- `game_scene_player_movement_system.h`
- `game_scene_player_visual_system.h`
- `game_scene_world_interaction_system.h`
- `game_scene_render.cpp`
- `game_scene_collision.cpp`
- `game_scene_internal.h`

この構成を崩さず、

- 状態所有
- ルール
- 描画
- 当たり判定

を分けて保つのが今の保守上かなり重要です。

現在の `GameScene` は、フラットに大量のメンバを持つ形から次の state struct へ整理し始めています。

- `m_flow`
  シーン進行、カメラ、スロー、プレビュー時間
- `m_player`
  プレイヤー運動、接地、回避、見た目補間
- `m_debug`
  デバッグ表示、チューニング UI、ホットリロード
- `m_photo`
  撮影、保存、配置、コピーグループ

この方針により、機能追加時の競合点を `game_scene.h` から減らしていきます。
さらに、入力寄りの写真スロット選択、player 入力、player 移動解決、player 見た目、world 接触、combat 更新は helper header へ逃がし、`game_scene_gameplay.cpp` を orchestration 寄りに保ちます。

プレイヤー回避の tuning は `game_scene_internal.h` の値を `assets/tuning.json` 経由で読む構成です。

- `dodge_speed`
- `dodge_distance`
- `dodge_invincibility`
- `dodge_cooldown`

`dodge_invincibility` は回避中の無敵、`damageCooldown.seconds` は被弾後の再被弾防止で、責務を分けています。
調整 UI 自体は `DrawTuningPanel()` で ImGui ウィンドウとして描画しています。

### `SceneManager`

責務:

- 現在のシーンの所有
- `OnExit()` と `OnEnter()` の呼び分け
- `Update()`、`Draw()`、`DrawDebugUI()` の委譲

### `SceneRegistry`

責務:

- シーン ID とファクトリ関数の対応を持つ
- シーン生成のハードコードを `Application` から外す

たとえば `"demo"` -> `std::make_unique<DemoScene>()` のような登録を行います。

### `Entity`

責務:

- 複数コンポーネントの所有
- コンポーネントの更新と描画の中継

このプロジェクトの ECS は軽量版です。  
完全なデータ指向 ECS ではなく、ゲーム制作で扱いやすいコンポーネント集合の形にしています。

### `Component`

責務:

- エンティティへ機能を追加する
- 必要に応じて `Update()`、`Draw()`、`DrawDebugUI()` を持つ

## 現在のコンポーネント

### `TransformComponent`

- 位置
- サイズ
- 回転
- 拡大率

### `TintComponent`

- 描画色

### `TagComponent`

- エンティティ識別用文字列

### `SpriteRenderComponent`

- スプライト描画

### `PlayerControllerComponent`

- 入力による移動、回転、拡縮
- 音再生要求イベントの発行

### `RigidBodyComponent`

- `Box2D` ボディ所有
- ECS Transform と物理ボディの同期

### `BoxColliderComponent`

- `Box2D` 形状所有
- センサーと物理設定

### `EnemyComponent`

- 敵の有効状態
- 撃破状態
- 接触ダメージ

### `EnemyMoverComponent`

- 敵の簡易移動
- 凍結
- 時間巻き戻し

### `GimmickComponent`

- ギミック種別
- 有効/無効
- ワンショット消費

### `PhotoFilterComponent`

- フィルター種別
- 出力ロール
- 出力レイヤ
- tint

## イベント駆動の流れ

### 目的

ゲームロジック層とミドルウェア層を直接結びつけないために、`EventBus` を挟んでいます。

### 発行側

- `PhysicsWorld`
  接触イベントを発行する
- `PlayerControllerComponent`
  音再生要求を発行する
- `DemoScene`
  シーン切り替え要求を発行する
- `Lua`
  `request_sound()` などを通してイベントを発行する

### 消費側

- `Application`
  音、ログ、シーン切り替えを処理する

### 現在のイベント種別

- `ContactBegin`
- `ContactEnd`
- `PlaySoundRequest`
- `SceneChangeRequested`
- `LogMessage`

## 物理と ECS の同期

`PhysicsWorld` は `Box2D` ワールドを管理します。  
同期は以下の順です。

1. エンティティの `Transform` を物理へ反映
2. `Box2D` を `Step()`
3. 物理ボディの結果をエンティティへ反映
4. 接触イベントを `EventBus` へ発行

この構成により、プレイヤー制御、接触、演出を分離できます。

## スクリプトの位置づけ

`ScriptEngine` は `sol2` を使って Lua を実行します。  
現在は以下の役割です。

- スクリプトファイルのロード
- 数値変数のやり取り
- `update(dt)` の呼び出し
- EventBus 向け API の公開

Lua はエンジンの代替ではなく、ロジックや演出の実験レイヤとして使っています。

## データ駆動の位置づけ

### `assets/manifest.json`

- テクスチャ生成設定

### `assets/prefabs.json`

- エンティティ構成定義

これにより、ゲーム内容の量産で毎回 C++ を触る必要を減らしています。

## 実装上の境界

このプロジェクトでは以下を意識しています。

- `Application` はゲーム固有ロジックを持たない
- `Scene` はゲーム進行を持つ
- `Component` は個別機能を持つ
- `EventBus` はシステム間の橋渡しだけをする
- `JSON` と `Lua` は量産と反復調整のために使う

この境界を崩すと、規模が大きくなったときに保守しづらくなります。

特に今の `GameScene` では、写真フィルターが

- 撮られた元オブジェクト
- 配置されたコピー

の両方へ作用します。  
そのため、フィルター仕様を触るときは `render` と `gameplay` の両方を確認する前提で考えてください。
