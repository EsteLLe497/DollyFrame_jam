# DirectXFoundation

`DirectXFoundation` は、`DirectX 11` ベースで 2D ゲームを組み立てるための基盤プロジェクトです。  
このリポジトリには、描画、入力、音、デバッグ UI、シーン管理、簡易 ECS、物理、スクリプト、イベント駆動の接続までをまとめています。

## このプロジェクトでできること

- Win32 + Direct3D 11 のウィンドウアプリをそのまま起動できる
- スプライトを描画できる
- `ImGui` でデバッグ UI を表示できる
- キーボードと `XInput` ゲームパッドを扱える
- `XAudio2` で効果音を鳴らせる
- `Box2D` で 2D 物理と接触判定を扱える
- `Lua` からイベントを発行してゲームロジックを駆動できる
- JSON でアセットとプレハブを定義できる
- CSV でタイルマップを定義できる

## クイックスタート

### 必要環境

- Windows
- Visual Studio 2022
- MSVC v143
- Windows SDK

### ビルド

Visual Studio の場合:

1. `DirectXFoundation.vcxproj` を開く
2. `Debug | x64` または `Release | x64` を選ぶ
3. ビルドして実行する

コマンドラインの場合:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "D:\directX\DirectXFoundation.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m:1
```

出力先:

- `build/Debug/DirectXFoundation.exe`
- `build/Release/DirectXFoundation.exe`

## 現在のサンプル内容

- `TitleScene` から起動する
- `TitleScene` から `GameScene` へ遷移できる
- `GameScene` から `ResultScene` へ遷移できる
- `ResultScene` から `TitleScene` か `GameScene` へ戻れる
- `GameScene` に `HP`、`Goal`、`Timer` の基本ルールが入っている
- `enemy` とダメージ無敵時間が入っている
- `player` と `target` の 2 つのプレハブを生成する
- `player` は入力で動く
- `target` は Lua スクリプトで揺れる
- `player` が `target` に接触すると色、音、ログが連動する
- Lua が一定間隔で音のイベントを発行する

### 操作

- `Arrow Keys`: 移動
- `Q / E`: 回転
- `Z / X`: 拡大縮小
- `Space`: テスト音再生
- `Enter`: タイトルからゲーム開始
- `R`: シーン再読み込み
- `T`: タイトルへ戻る
- `GameScene` で `goal` に触れる: クリア
- `GameScene` で `hazard` に触れる: HP が減少
- `GameScene` で `enemy` に触れる: HP が減少
- `GameScene` で時間切れ: リザルトへ遷移
- `ResultScene` で `Enter`: タイトルへ戻る
- `ResultScene` で `R`: リトライ
- `Gamepad Left Stick`: 移動
- `Gamepad Triggers`: 回転
- `Gamepad A`: テスト音再生

## リポジトリの見方

### ルートの主要ファイル

- `main.cpp`
  エントリポイントです。`Application` を起動するだけに絞っています。
- `application.h` / `application.cpp`
  アプリ全体の初期化、メインループ、描画、ミドルウェア更新、イベント消費を担当します。
- `scene.h`
  すべてのシーンの抽象基底です。
- `scene_manager.h` / `scene_manager.cpp`
  現在シーンの所有と更新を担当します。
- `scene_registry.h` / `scene_registry.cpp`
  シーン ID からシーンインスタンスを生成します。
- `title_scene.h` / `title_scene.cpp`
  タイトルシーンです。ゲーム開始導線を持ちます。
- `game_scene.h` / `game_scene.cpp`
  現在のプレイ用シーンです。HP、Goal、Timer、Enemy を持ちます。
- `game_session.h` / `game_session.cpp`
  実行中ゲームの結果情報を `ResultScene` へ引き渡します。
- `result_scene.h` / `result_scene.cpp`
  リザルトシーンです。タイトル復帰とリトライ導線を持ちます。
- `demo_scene.h` / `demo_scene.cpp`
  技術確認用のサンドボックスシーンです。
- `entity.h` / `entity.cpp`
  コンポーネントの集合としてのゲームオブジェクトです。
- `component.h` / `component.cpp`
  コンポーネント基底です。
- `components.h` / `components.cpp`
  `Transform`、`SpriteRender`、`PlayerController`、`RigidBody` などの実装です。
- `event_bus.h` / `event_bus.cpp`
  シーン内イベントの発行と収集を行います。
- `physics_world.h` / `physics_world.cpp`
  `Box2D` ワールドの管理と ECS との同期を行います。
- `asset_manifest.h` / `asset_manifest.cpp`
  `assets/manifest.json` を読み、テクスチャ ID を構築します。
- `prefab_factory.h` / `prefab_factory.cpp`
  `assets/prefabs.json` を読み、エンティティを生成します。
- `tile_map.h` / `tile_map.cpp`
  CSV タイルマップの読み込みと描画を担当します。
- `script_engine.h` / `script_engine.cpp`
  `Lua` 実行と `EventBus` への公開 API を担当します。
- `resource_manager.h` / `resource_manager.cpp`
  テクスチャ生成とロードを担当します。
- `directX.h` / `directX.cpp`
  Direct3D 11 デバイス、スワップチェーン、RTV/DSV、共通ステートを初期化します。
- `shader.h` / `shader.cpp`
  2D スプライト描画用シェーダのセットアップです。
- `sprite.h` / `sprite.cpp`
  スプライト描画ラッパです。
- `imgui_layer.h` / `imgui_layer.cpp`
  `ImGui` の Win32 / DX11 バックエンド層です。
- `input.h` / `input.cpp`
  キーボードとゲームパッド入力のラッパです。
- `audio.h` / `audio.cpp`
  `XAudio2` の簡易ラッパです。
- `logger.h` / `logger.cpp`
  `spdlog` ベースのログ出力です。

### データファイル

- `assets/manifest.json`
  テクスチャ定義です。
- `assets/prefabs.json`
  プレハブ定義です。
- `assets/demo_scene.lua`
  サンプルシーンの Lua スクリプトです。
- `assets/maps/side_scroll_stage01.csv`
  CSV ベースのサンプルタイルマップです。

### 外部ライブラリ

- `third_party/imgui`
- `third_party/tracy`
- `third_party/spdlog`
- `third_party/json`
- `third_party/lua54`
- `third_party/sol2`
- `third_party/box2d`

`third_party` 配下はライブラリ本体なので、通常の改修対象はこの README で列挙したルートファイル群です。

## 設計の要点

### 1. `Application` は基盤統括

`Application` は以下を担当します。

- Win32 ウィンドウ作成
- DirectX 初期化
- ミドルウェア初期化
- フレーム更新
- シーン更新と描画
- シーンイベントの消費

### 2. `Scene` はゲーム進行の単位

各シーンは `Scene` を継承し、以下を実装します。

- `GetSceneId()`
- `OnEnter()`
- `Update()`
- `Draw()`
- `DrawDebugUI()`

### 3. `Entity` + `Component` で挙動を組む

現在の主なコンポーネント:

- `TransformComponent`
- `TintComponent`
- `TagComponent`
- `SpriteRenderComponent`
- `PlayerControllerComponent`
- `RigidBodyComponent`
- `BoxColliderComponent`

### 4. `EventBus` でシステム間を疎結合にする

主なイベント種別:

- `ContactBegin`
- `ContactEnd`
- `PlaySoundRequest`
- `SceneChangeRequested`
- `LogMessage`

シーンやコンポーネントはイベントを発行し、実際の音再生は `Application` 側が処理します。

### 5. データ駆動の入口は JSON と Lua

- `manifest.json`: テクスチャ定義
- `prefabs.json`: プレハブ定義
- `side_scroll_stage01.csv`: タイル ID 配列
- `demo_scene.lua`: シーンロジックの一部

### CSV タイルマップ

`assets/maps/side_scroll_stage01.csv` は 1 セル = 1 タイルの CSV です。現状のサンプルでは次の値を使っています。

- `0`: 空
- `1`: 地面
- `2`: 足場
- `3`: 装飾床
- `4`: 危険帯の目印
- `5`: ゴール目印

今は [game_scene.cpp](/D:/directX/game_scene.cpp) で背景として読み込んでいます。次に横スクロール本体へ進めるときは、この CSV をそのまま地形当たり判定へつなげられます。

## ドキュメント一覧

- [docs/architecture.md](docs/architecture.md)
  クラス構成、更新順、イベントの流れ、責務分離
- [docs/data-formats.md](docs/data-formats.md)
  `manifest.json`、`prefabs.json`、Lua API の説明
- [docs/extension-guide.md](docs/extension-guide.md)
  新しいシーン、コンポーネント、プレハブ、イベントの追加手順
- [docs/middleware.md](docs/middleware.md)
  導入済みミドルウェアの役割と接続先

## どこから触るべきか

用途別の開始地点は以下です。

- 新しいシーンを作る
  `scene.h`、`scene_registry.h`、`application.cpp`
- プレイヤーや敵を増やす
  `components.h`、`prefab_factory.cpp`、`assets/prefabs.json`
- データを調整したい
  `assets/manifest.json`、`assets/prefabs.json`
- スクリプトで試したい
  `script_engine.cpp`、`assets/demo_scene.lua`
- 音を増やす
  `audio.cpp`、`event_bus.h`

## 補足

- 現在の基盤は 2D 前提です。
- 3D へ進める場合は、カメラ、メッシュ、マテリアル、深度有効描画、シーン定数の再設計が必要です。
- `third_party` は依存ライブラリです。通常の保守対象としては扱わない前提で構成しています。
