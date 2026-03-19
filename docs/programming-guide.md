# Programming Guide

このドキュメントは、`DollyFrame_jam` を触る人向けの実装説明書です。  
目的は「どこに何があるか」「どう改修するか」を短時間で掴めるようにすることです。

## 1. 全体像

このプロジェクトは、`DxLib` をベースにした 2D ゲームです。  
構成としては大きく次の 4 層に分かれています。

- アプリ基盤
- シーン管理
- ECS 風のゲームオブジェクト管理
- `GameScene` のゲームプレイ実装

主な流れはこうです。

1. `main.cpp` から `Application` を起動する
2. `Application` が入力、シーン更新、描画、イベント消費を回す
3. `SceneManager` が現在シーンを保持する
4. `GameScene` がゲームルール、撮影、配置、敵、ギミックを処理する

## 2. 主要ファイル

### エントリと基盤

- `main.cpp`
  エントリポイントです。
- `core/application.h` / `core/application.cpp`
  メインループ、シーン更新、描画、イベント処理を持ちます。
- `core/input.h` / `core/input.cpp`
  キーボード、マウス、ゲームパッド入力のラッパです。主要操作は `InputAction` と `InputAxis` に寄せています。
- `core/audio.h` / `core/audio.cpp`
  簡易音声再生です。
- `core/logger.h` / `core/logger.cpp`
  ログ出力です。

### シーン

- `core/scene.h`
  シーン基底クラスです。
- `core/scene_manager.*`
  現在シーンの所有と遷移処理です。
- `core/scene_registry.*`
  シーン ID とシーンクラスの対応表です。
- `scenes/title_scene.*`
  タイトル画面です。
- `scenes/game/game_scene.*`
  メインのゲームプレイです。
- `scenes/result_scene.*`
  結果画面です。

### ECS 風オブジェクト

- `core/entity.h` / `core/entity.cpp`
  コンポーネントを束ねるゲームオブジェクトです。
- `core/component.h` / `core/component.cpp`
  コンポーネント基底です。
- `gameplay/components.h` / `gameplay/components.cpp`
  実際のコンポーネント群です。

### データ

- `assets/maps/side_scroll_stage01.csv`
  ステージのタイル配置です。
- `assets/prefabs.json`
  プレハブ定義です。
- `assets/tuning.json`
  調整値です。

## 3. `GameScene` の分割

`GameScene` は 1 ファイルではなく、役割ごとに分かれています。

- `scenes/game/game_scene.h`
  状態とメソッド宣言
- `scenes/game/game_scene.cpp`
  初期化、シーン更新、デバッグ UI
- `scenes/game/game_scene_setup.cpp`
  ステージ初期化、prefab 配置、チューニング入出力
- `scenes/game/game_scene_gameplay.cpp`
  プレイヤー更新、撮影、配置、敵、相互作用
- `scenes/game/game_scene_render.cpp`
  エンティティ描画とワールド側の見た目
- `scenes/game/game_scene_render_ui.cpp`
  背景、HUD、撮影オーバーレイ、写真プレビュー、調整 UI
- `scenes/game/game_scene_collision.cpp`
  地形判定、重なり判定、補助関数
- `scenes/game/game_scene_internal.h`
  `inline` の共通補助関数と定数
- `gameplay/photo_system.cpp`
  写真の撮影、配置、コピー生成、配置プレビュー描画
- `gameplay/photo_filter_rules.cpp`
  フィルター順、名称、効果本体

改修時はまず「何を変えたいか」でファイルを分けて考えると速いです。

- ルールを変えたい: `scenes/game/game_scene_gameplay.cpp`
- 見た目を変えたい: `scenes/game/game_scene_render.cpp`
- HUD やオーバーレイを変えたい: `scenes/game/game_scene_render_ui.cpp`
- 当たり判定を変えたい: `scenes/game/game_scene_collision.cpp`
- 初期配置や prefab 構成を変えたい: `scenes/game/game_scene_setup.cpp` と `assets/prefabs.json`
- 写真まわりを変えたい: `gameplay/photo_system.cpp`
- 新しい状態を持たせたい: `scenes/game/game_scene.h`

## 4. 主なゲーム要素

### 入力

主要な入力は `core/input.h` の `InputAction` / `InputAxis` でまとめています。

- 離散入力: `Input_IsActionPressed()` / `Input_IsActionDown()`
- 軸入力: `Input_GetAxis()`

方針:

- シーン遷移や `GameScene` の主要操作はアクション経由で扱う
- `Input_IsKeyPressed()` / `Input_IsKeyDown()` は移行途中の補助として残す
- 新規実装では、まずアクション追加で表現できるかを確認する

### プレイヤー

`GameScene::UpdatePlayer()` で移動、ジャンプ、重力、地形接地、写真オブジェクトとの接触を処理しています。

### 写真システム

写真まわりは次の順です。

1. 右クリックでカメラモード
2. 左クリックで撮影
3. `E` で配置プレビュー
4. 左クリックで配置確定

配置中の操作:

- `Q`: 配置レイヤ切り替え
- `F`: 左右反転
- `B`: ブリッジ切り替え
- `Z / X`: 写真を連続回転

補足:

- 貼り付けた写真はポラロイド台紙つきで生成される
- 貼り付けた写真は 10 秒でフェードアウトする
- `PhotoFilterTheme::None` のときは撮影元の色をそのまま保持する
- 坂タイル `6 / 7` は写真に保存され、貼り付け後も坂として扱う
- 写真サイズを調整したいときは `scenes/game/game_scene_internal.h` と `assets/tuning.json` の `capture_width_tiles` / `capture_height_tiles` / `printed_photo_*` を見る

関連メソッド:

- `HandlePhotoCapture()`
- `HandlePhotoSpawn()`
- `ApplyPhotoFilterToPhotoBox()`
- `ApplyPhotoFilterToCapturedTarget()`

実装の中心:

- `gameplay/photo_system.cpp`
- `gameplay/photo_filter_rules.cpp`

### フィルター

現在のテーマ:

- `None`
- `Hot`
- `Cold`
- `Invert`
- `Sepia`

役割はこうです。

- `Hot`
  危険化、燃焼系
- `Cold`
  凍結、足場化系
- `Invert`
  敵対反転系
- `Sepia`
  時間巻き戻し系

注意点:

- フィルターは「貼ったコピー」にも作用します
- さらに「撮られた元オブジェクト」にも作用します

### スロー演出

集中補助として、撮影時と配置時にゲームがスローになります。

- 撮影開始時: `0.8` 秒
- 配置開始時: `1.2` 秒
- シャッターフラッシュは通常速度

実装は `scenes/game/game_scene.cpp` の `Update()` にあります。

### 時間制限

現在の `GameScene` では時間制限を使っていません。

- カウントダウンなし
- `TimeUp` 遷移なし
- pickup は時間回復ではなく通常取得扱い

### 敵

主に使っているコンポーネント:

- `EnemyComponent`
- `EnemyMoverComponent`

`EnemyMoverComponent` は浮遊移動を担当し、現在は `SetFrozen()` と `Rewind()` を持っています。  
つまり `Cold` や `Sepia` のようなフィルター作用を直接受けられます。

### ギミック

主に使っているコンポーネント:

- `GimmickComponent`
- `PhotoFilterComponent`

今の `GameScene` では、ワールド上のフィルター装置より「選択したフィルターで撮る」方が主軸です。

## 5. 改修するときの入口

### 新しいフィルターを追加したい

触る場所:

- `gameplay/components.h`
  `PhotoFilterTheme`
- `gameplay/photo_filter_rules.cpp`
  テーマ名表示、フィルター順、効果本体
- `scenes/game/game_scene.cpp`
  入力切り替え
- `scenes/game/game_scene_render.cpp`
  UI 色、プレビュー、オーバーレイ

### 新しい敵を追加したい

最小構成:

1. `components.*` に必要な状態を足す
2. `scenes/game/game_scene.cpp` の生成処理を増やす
3. 必要なら `gameplay/prefab_factory.*` と `assets/prefabs.json` を更新する

### 新しいギミックを追加したい

まず `GimmickComponent` で足りるか確認します。  
専用状態が必要ならコンポーネントを増やし、`HandleWorldInteractions()` に挙動を追加します。

### 背景や UI を変えたい

ワールド描画は `scenes/game/game_scene_render.cpp`、HUD やオーバーレイは `scenes/game/game_scene_render_ui.cpp` を見ます。

主な描画入口:

- `DrawBackdrop()`
- `DrawCaptureOverlay()`
- `DrawPhotoPlacementPreview()`
- `DrawDevelopedPhotoPreview()`
- `DrawEntity()`

## 6. データ駆動の範囲

このプロジェクトは完全なデータ駆動ではありません。  
現状は「一部がデータ駆動で、一部は `GameScene` ハードコード」です。

データ側:

- `prefabs.json`
- `side_scroll_stage01.csv`
- `tuning.json`

コード側:

- ステージ上の主要オブジェクト配置
- フィルターの細かい効果
- プレイヤー操作
- シーン進行

つまり、配置やルールを大きく変えるときは、データだけではなく `GameScene` も触る前提です。

### タイル ID メモ

`assets/maps/side_scroll_stage01.csv` の主な値:

- `0`: 空
- `1`: 地面
- `2`: 足場
- `3`: 装飾床
- `4`: 危険帯
- `5`: ゴール
- `6`: 右上がり坂
- `7`: 右下がり坂

`6` と `7` は通常マップでも、写真を撮って貼ったコピーでも坂として扱います。

## 7. ビルドについて

### メイン実行ファイル

- `DirectXFoundation.vcxproj`

### 調整ツール

- `TuningTool.vcxproj`

### コマンドライン例

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "D:\DollyFrame_jam\DirectXFoundation.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m:1
```

出力:

- `build/Debug/DirectXFoundation.exe`

注意:

- 現在は `dxlib_support_libs/` がリンクに必要です
- これが無いと `DxLib_vs2015_x64_MTd.lib` でリンクエラーになります

## 8. よくある触り方

### フィルターの見た目だけ変えたい

`scenes/game/game_scene_render.cpp` だけで足りることが多いです。

### フィルターの効果を変えたい

`gameplay/photo_filter_rules.cpp` の

- `ApplyPhotoFilterToPhotoBox()`
- `ApplyPhotoFilterToCapturedTarget()`

を見ます。

### 写真配置の感触を変えたい

`gameplay/photo_system.cpp` の配置処理を見ます。

### スロー時間を変えたい

`scenes/game/game_scene.cpp` と `scenes/game/game_scene_internal.h` の定数を見ます。

## 9. 実装上の注意

- `GameScene` は状態が多いので、変更前に `scenes/game/game_scene.h` を確認する
- 見た目変更とルール変更を同じ関数に混ぜすぎない
- `third_party/` は基本的に直接触らない
- 生成物や依存バイナリは Git 管理方針が揺れやすいので注意する

## 10. 迷ったときの読み順

初見ならこの順が速いです。

1. `scenes/game/game_scene.h`
2. `scenes/game/game_scene.cpp`
3. `scenes/game/game_scene_gameplay.cpp`
4. `gameplay/photo_system.cpp`
5. `scenes/game/game_scene_render.cpp`
6. `gameplay/components.h`
7. `scenes/game/game_scene_internal.h`
8. `assets/prefabs.json`
9. `assets/maps/side_scroll_stage01.csv`

この順で読むと、状態、更新、描画、部品、データの対応が掴みやすいです。
