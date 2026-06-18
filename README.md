# DirectXFoundation

`DirectXFoundation` は、DxLib をベースにした Windows 向け 2D ゲームプロジェクトです。
シーン管理、入力、描画、音声、軽量 ECS、物理、写真撮影/貼り付け、Prefab、Lua、JSON/CSV データをひとつのゲーム基盤としてまとめています。

## プロジェクト構成

このリポジトリには Visual Studio 2022 用のソリューションが含まれています。

- `DirectXFoundation.sln`
  メインのソリューションです。
- `DirectXFoundation.vcxproj`
  ゲーム本体です。
- `TuningTool.vcxproj`
  JSON ベースの調整データを扱う補助ツールです。

対応構成は `Debug|x64` と `Release|x64` です。

## 必要環境

- Windows
- Visual Studio 2022
- MSVC v143
- Windows SDK
- Git

## ビルド

Visual Studio でビルドする場合:

1. `DirectXFoundation.sln` を開く
2. `Debug | x64` または `Release | x64` を選ぶ
3. ソリューションをビルドする

コマンドラインでビルドする場合:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" DirectXFoundation.sln /t:Build /m:1 /p:Configuration=Debug /p:Platform=x64 /p:UseMultiToolTask=false /p:CL_MPCount=1
```

主な出力先:

- `build/Debug/DirectXFoundation.exe`
- `build/Release/DirectXFoundation.exe`

## ディレクトリ

- `core/`
  アプリケーション、シーン管理、入力、音声、ログ、イベント、リソース管理、軽量 ECS の基盤です。
- `gameplay/`
  ゲーム固有のコンポーネント、Prefab 生成、写真システム、フィルター、Lua スクリプト連携を扱います。
- `physics/`
  衝突、タイルマップ、Box2D 連携、画像輪郭処理を扱います。
- `rendering/`
  DxLib/DirectX まわりの描画、スプライト、テクスチャ、シェーダ、ImGui レイヤーを扱います。
- `scenes/`
  `TitleScene`、`GameScene`、`ResultScene`、`DemoScene`、`ShaderShowcaseScene` などのシーン実装です。
- `scenes/game/`
  `GameScene` の詳細実装です。入力、描画、UI、衝突、敵、ギミック、商人、マップ編集、写真操作などが domain 単位で分割されています。
- `assets/`
  画像、音声、エフェクト、マップ CSV、Lua、JSON 設定を置きます。
- `shaders/`
  HLSL シェーダを置きます。
- `third_party/`
  外部ライブラリを置きます。
- `docs/`
  設計、ビルド、データ形式、拡張、チーム作業用のドキュメントを置きます。
- `tools/`
  DDS/PNG 変換などの補助ツールを置きます。
- `exports/`
  書き出し成果物を置きます。

## エントリポイント

`main.cpp` の `WinMain` がエントリポイントです。

```cpp
Application app;
return app.Run(hInstance, nCmdShow);
```

アプリケーション本体の初期化、メインループ、シーン更新、描画は `core/application.*` が担当します。

## 主なシーン

- `TitleScene`
  タイトル画面です。
- `GameScene`
  メインのゲームシーンです。現在もっとも大きい実装で、複数の domain ファイルに分割されています。
- `ResultScene`
  リザルト画面です。
- `DemoScene`
  Lua や基本機能の確認用シーンです。
- `ShaderShowcaseScene`
  シェーダや描画効果の確認用シーンです。

## データファイル

- `assets/manifest.json`
  テクスチャなどのアセット ID を定義します。
- `assets/prefabs.json`
  エンティティ生成用の Prefab を定義します。
- `assets/input_bindings.json`
  キーボード/ゲームパッド入力のアクション割り当てを定義します。
- `assets/tuning.json`
  移動、回避、ゲームプレイ調整値を定義します。
- `assets/demo_scene.lua`
  Lua スクリプトのサンプルです。
- `assets/maps/`
  CSV ベースのステージ、マップ、遷移データを置きます。

## 入力

入力は `assets/input_bindings.json` でアクション名に割り当てられています。
代表的な操作は次の通りです。

- `Enter` / `Space`: 決定、ゲーム開始
- `Esc`: キャンセル
- `A` / `D` / 矢印キー: 移動
- `W` / `Space` / `Up`: ジャンプ
- `Shift`: 回避
- `Right Click`: カメラ保持
- `Left Click`: 撮影、配置確定
- `E`: 配置モード保持
- `Q`: 攻撃貼り付け、または配置レイヤー切り替え
- `C`: フィルター切り替え
- `1` - `5`: フィルター直接選択
- `F1`: チューニングパネル切り替え
- `F2`: ポストプロセス切り替え
- `F3`: 衝突デバッグ表示切り替え
- `R`: シーン再読み込み
- `T`: タイトルへ戻る

## 外部ライブラリ

主な依存ライブラリは次の通りです。

- DxLib
- ImGui
- Tracy
- spdlog
- nlohmann/json
- Lua 5.4
- sol2
- Box2D
- DirectXTex

Debug ビルドでは `DxLib_vs2015_x64_MTd.lib` 系、Release ビルドでは `DxLib_vs2015_x64_MT.lib` 系をリンクします。
DxLib のライブラリは `dxlib_support_libs/` に配置されています。

## よく触るファイル

- `core/application.cpp`
  アプリ全体の初期化、メインループ、描画、イベント処理です。
- `core/input.cpp`
  キーボード/マウス/ゲームパッド入力の集約です。
- `core/scene_manager.cpp`
  シーンの更新、描画、遷移処理です。
- `core/scene_registry.cpp`
  シーン ID とシーン生成処理の登録です。
- `scenes/game/game_scene.h`
  `GameScene` の状態と主要メンバーです。
- `scenes/game/game_scene_setup.cpp`
  ステージ初期化、Prefab 配置、チューニング入力です。
- `scenes/game/game_scene_gameplay.cpp`
  プレイヤー、バッテリー、写真操作などの中核更新です。
- `scenes/game/game_scene_render.cpp`
  ゲーム内エンティティとワールド描画です。
- `scenes/game/game_scene_render_ui.cpp`
  HUD、写真プレビュー、調整 UI などの描画です。
- `gameplay/prefab_factory.cpp`
  `assets/prefabs.json` からエンティティを生成します。
- `gameplay/photo_capture_system.cpp`
  写真撮影処理です。
- `gameplay/photo_paste_system.cpp`
  写真貼り付け処理です。
- `physics/tile_map.cpp`
  CSV マップとタイル衝突です。
- `rendering/shader.cpp`
  シェーダの読み込みと適用です。

## 作業時の注意

- `third_party/` と `dxlib_support_libs/` は基本的に外部依存として扱います。
- 新しい `.cpp` / `.h` を追加した場合は、`.vcxproj` と `.vcxproj.filters` への登録も確認してください。
- 実行時ログは `foundation.log` に出ます。
- `assets/` の JSON/CSV を変更した場合は、ゲーム起動後の読み込み結果も確認してください。
- README や docs に古い記述がある場合は、実装と `.vcxproj` の状態を優先して確認してください。
