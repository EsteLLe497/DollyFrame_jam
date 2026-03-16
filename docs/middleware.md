# Middleware

このドキュメントは、導入済みミドルウェアと接続先をまとめます。

## 関連ドキュメント

- `docs/programming-guide.md`
- `docs/team-programming-guide.md`

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
- `game_scene.cpp`

補足:

現在は `GameScene` のデバッグ情報も `ImGui` へ出しています。  
ただしプレイヤーが常時見る UI は `game_scene_render.cpp` 側のゲーム描画で出しています。

## `XInput`

役割:

- ゲームパッド入力

接続先:

- `input.h` / `input.cpp`
- `PlayerControllerComponent`

補足:

キーボードと同じく `input.cpp` 側の明示マップで管理しています。  
使うキーを増やすときはここを更新してください。

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
- `game_scene.cpp`
- `game_scene_gameplay.cpp`

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

## `DxLib`

役割:

- ウィンドウと描画の実行基盤
- 入力取得
- 2D 描画
- シェーダ適用

接続先:

- `application.cpp`
- `input.cpp`
- `shader.cpp`
- `sprite.cpp`
- `game_scene_render.cpp`

補足:

現在の実行は実質 `DxLib` 前提です。  
README の古い説明にある `DirectX 11` 単体基盤として読むとズレるので注意してください。

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
- 生成物と依存バイナリの Git 管理方針整理
