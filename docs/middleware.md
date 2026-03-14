# Middleware

このドキュメントは、導入済みミドルウェアと接続先をまとめます。

## 一覧

- `ImGui`
- `XInput`
- `XAudio2`
- `Tracy`
- `nlohmann/json`
- `spdlog`
- `Lua 5.4`
- `sol2`
- `Box2D`

## `ImGui`

役割:

- デバッグ UI
- 基盤情報の表示
- シーン状態の可視化

接続先:

- `imgui_layer.h` / `imgui_layer.cpp`
- `application.cpp`
- `demo_scene.cpp`

## `XInput`

役割:

- ゲームパッド入力

接続先:

- `input.h` / `input.cpp`
- `PlayerControllerComponent`

## `XAudio2`

役割:

- 効果音再生

接続先:

- `audio.h` / `audio.cpp`
- `Application::ProcessSceneEvents()`

備考:

- 現在はサンプル用の簡易トーン生成です
- 本格的な音源管理はまだ未実装です

## `Tracy`

役割:

- CPU フレーム計測
- スコープ計測

接続先:

- `application.cpp`
- `demo_scene.cpp`

備考:

- `ZoneScoped`
- `FrameMark`

がすでに入っています。

## `nlohmann/json`

役割:

- アセット定義の読み込み
- プレハブ定義の読み込み

接続先:

- `asset_manifest.cpp`
- `prefab_factory.cpp`

## `spdlog`

役割:

- ログ出力
- 初期化、ロード、接触、シーン遷移の記録

接続先:

- `logger.h` / `logger.cpp`
- 各システムの `Logger::Info()` / `Warn()` / `Error()`

## `Lua 5.4` + `sol2`

役割:

- スクリプトロジック
- データ調整
- EventBus への要求発行

接続先:

- `script_engine.h` / `script_engine.cpp`
- `assets/demo_scene.lua`

公開中 API:

- `log_message`
- `request_sound`
- `request_scene_change`

## `Box2D`

役割:

- 2D 物理
- 接触判定
- 動的ボディ管理

接続先:

- `physics_world.h` / `physics_world.cpp`
- `RigidBodyComponent`
- `BoxColliderComponent`

備考:

- ECS と `Box2D` の同期は `PhysicsWorld` が担当します
- 接触は `ContactBegin` / `ContactEnd` として EventBus へ流します

## 導入方針

このプロジェクトでは、ミドルウェアを直接ゲームロジックから呼ぶのではなく、薄いラッパとイベントを挟んで使う方針です。

理由:

- 差し替えやすい
- テストしやすい
- シーンやコンポーネントを疎結合に保てる
- 基盤とゲーム内容の境界を維持できる

## 将来的な拡張候補

- 画像ファイルローダの本格化
- 音源ファイルのストリーミング再生
- Lua API の整理
- Tracy の GPU 計測追加
