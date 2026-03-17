# DirectXFoundation

`DirectXFoundation` は、`DxLib` ベースで 2D ゲームを組み立てるための基盤プロジェクトです。  
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
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "D:\DollyFrame_jam\DirectXFoundation.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m:1
```

出力先:

- `build/Debug/DirectXFoundation.exe`
- `build/Release/DirectXFoundation.exe`

## 現在のサンプル内容

- `TitleScene` から起動する
- `TitleScene` から `GameScene` へ遷移できる
- `GameScene` から `ResultScene` へ遷移できる
- `ResultScene` から `TitleScene` か `GameScene` へ戻れる
- `GameScene` に `HP`、`Goal`、写真撮影と貼り付けの基本ルールが入っている
- `enemy` とダメージ無敵時間が入っている
- `player` と `target` の 2 つのプレハブを生成する
- `player` は入力で動く
- `target` は Lua スクリプトで揺れる
- `player` が `target` に接触すると色、音、ログが連動する
- Lua が一定間隔で音のイベントを発行する
- 写真を撮ってポラロイドとして貼り付けられる
- 貼り付けた写真は回転でき、10 秒でフェードアウトする
- CSV タイルと撮影した写真の両方で坂タイルを扱える

### 操作

- `A / D` または `Arrow Keys`: 移動
- `W / Space / Up`: ジャンプ
- `Left Shift / Right Shift`: 回避
- `Right Click`: カメラモード
- `Left Click`: 撮影 / 配置確定
- `C`: フィルター切り替え
- `1 / 2 / 3 / 4 / 5`: `None / Hot / Cold / Invert / Sepia`
- `E`: 写真配置モード
- `Q`: 配置レイヤ切り替え
- `F`: 左右反転
- `B`: ブリッジ切り替え
- `Z / X`: 配置中の写真を連続回転
- `Enter`: タイトルからゲーム開始
- `R`: シーン再読み込み
- `T`: タイトルへ戻る
- `GameScene` で `goal` に触れる: クリア
- `GameScene` で `hazard` に触れる: HP が減少
- `GameScene` で `enemy` に触れる: HP が減少
- `ResultScene` で `Enter`: タイトルへ戻る
- `ResultScene` で `R`: リトライ
- `Gamepad Left Stick`: 移動
- `Gamepad A`: ジャンプ
- `Gamepad Triggers`: 配置中の写真回転

## リポジトリの見方

### 主要ディレクトリ

- `main.cpp`
  エントリポイントです。`Application` を起動します。
- `core/`
  アプリ基盤、シーン管理、入力、音、ログ、ECS の土台です。
- `gameplay/`
  ゲーム固有のコンポーネント、プレハブ生成、フィルター処理、`photo_system` などです。
- `physics/`
  タイル、衝突、物理ワールドです。
- `rendering/`
  `DxLib` / 描画ラッパ / シェーダ管理 / テクスチャ管理です。
- `scenes/`
  `TitleScene`、`ResultScene`、`DemoScene`、`ShaderShowcaseScene` です。
- `scenes/game/`
  `GameScene` 本体です。更新、描画、衝突、内部定数を分割しています。
- `shaders/src/`
  `.hlsl` のソースです。
- `shaders/bin/`
  ビルドで生成される `.cso` の出力先です。

### よく触るファイル

- `core/application.h` / `core/application.cpp`
  アプリ全体の初期化、メインループ、描画、イベント消費です。
- `core/scene.h`
  すべてのシーンの抽象基底です。
- `core/scene_manager.h` / `core/scene_manager.cpp`
  現在シーンの所有と更新です。
- `core/scene_registry.h` / `core/scene_registry.cpp`
  シーン ID からシーンインスタンスを生成します。
- `scenes/title_scene.h` / `scenes/title_scene.cpp`
  タイトルシーンです。
- `scenes/game/game_scene.h`
  `GameScene` の状態と宣言です。
- `scenes/game/game_scene.cpp`
  初期化、全体更新、デバッグ UI です。
- `scenes/game/game_scene_gameplay.cpp`
  プレイヤー、敵、ワールド相互作用です。
- `scenes/game/game_scene_render.cpp`
  背景、UI、エンティティ描画です。
- `scenes/game/game_scene_collision.cpp`
  地形判定、重なり判定、配置判定です。
- `gameplay/photo_filter_rules.h` / `gameplay/photo_filter_rules.cpp`
  写真フィルターの名称、順序、効果本体です。
- `gameplay/photo_system.h` / `gameplay/photo_system.cpp`
  写真の撮影、配置、コピー生成、プレビュー描画です。
- `gameplay/prefab_factory.h` / `gameplay/prefab_factory.cpp`
  `assets/prefabs.json` を読み、エンティティを生成します。
- `rendering/shader.h` / `rendering/shader.cpp`
  2D スプライト描画用シェーダのセットアップです。
- `rendering/sprite.h` / `rendering/sprite.cpp`
  スプライト描画ラッパです。

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

`third_party` 配下はライブラリ本体なので、通常の改修対象は `core/` `gameplay/` `physics/` `rendering/` `scenes/` 配下です。

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
- `6`: 右上がり坂
- `7`: 右下がり坂

`6` と `7` は通常タイルだけでなく、写真で撮って貼った `PhotoBox` 側にも保存されます。見た目も当たり判定も坂として扱います。

## ドキュメント一覧

- [docs/architecture.md](docs/architecture.md)
  クラス構成、更新順、イベントの流れ、責務分離
- [docs/data-formats.md](docs/data-formats.md)
  `manifest.json`、`prefabs.json`、Lua API の説明
- [docs/extension-guide.md](docs/extension-guide.md)
  新しいシーン、コンポーネント、プレハブ、イベントの追加手順
- [docs/middleware.md](docs/middleware.md)
  導入済みミドルウェアの役割と接続先
- [docs/programming-guide.md](docs/programming-guide.md)
  現在の実装に合わせたプログラミング説明書
- [docs/team-programming-guide.md](docs/team-programming-guide.md)
  チーム制作メンバー向けのプログラム作業ガイド
- [docs/git-and-artifacts-guide.md](docs/git-and-artifacts-guide.md)
  Git 運用と生成物の扱い方
- [docs/build-setup-guide.md](docs/build-setup-guide.md)
  チーム向けビルド環境セットアップ手順
- [docs/filter-spec.md](docs/filter-spec.md)
  `Hot / Cold / Invert / Sepia` の仕様書
- [docs/level-editing-guide.md](docs/level-editing-guide.md)
  ステージ編集と配置作業のルール
- [docs/task-recipes.md](docs/task-recipes.md)
  敵追加、ギミック追加、フィルター追加の実装レシピ集
- [docs/glossary.md](docs/glossary.md)
  用語集
- [docs/troubleshooting.md](docs/troubleshooting.md)
  よくある詰まりどころと対処
- [docs/coding-conventions.md](docs/coding-conventions.md)
  このプロジェクト向けのコーディング規約メモ
- [docs/scene-reference.md](docs/scene-reference.md)
  シーン別の入口と担当範囲
- [docs/shader-guide.md](docs/shader-guide.md)
  シェーダ担当向けガイド
- [docs/ui-guide.md](docs/ui-guide.md)
  UI担当向けガイド
- [docs/refactor-candidates.md](docs/refactor-candidates.md)
  今後の改善候補一覧

## どこから触るべきか

用途別の開始地点は以下です。

- 新しいシーンを作る
  `core/scene.h`、`core/scene_registry.h`、`core/application.cpp`
- プレイヤーや敵を増やす
  `gameplay/components.h`、`gameplay/prefab_factory.cpp`、`assets/prefabs.json`
- データを調整したい
  `assets/manifest.json`、`assets/prefabs.json`
- スクリプトで試したい
  `gameplay/script_engine.cpp`、`assets/demo_scene.lua`
- 音を増やす
  `core/audio.cpp`、`core/event_bus.h`

## 補足

- 現在の基盤は 2D 前提です。
- 3D へ進める場合は、カメラ、メッシュ、マテリアル、深度有効描画、シーン定数の再設計が必要です。
- `third_party` は依存ライブラリです。通常の保守対象としては扱わない前提で構成しています。
