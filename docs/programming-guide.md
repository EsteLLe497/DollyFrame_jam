# Programming Guide

このドキュメントは、`DollyFrame_jam` を触る人向けの実装説明書です。  
目的は「どこに何があるか」「どう改修するか」を短時間で掴めるようにすることです。

## 1. 全体像

主な流れは次です。

1. `main.cpp` から `Application` を起動
2. `Application` が入力、シーン更新、描画、イベント処理を実行
3. `SceneManager` が現在シーンを保持
4. `GameScene` がプレイヤー、写真、敵、ギミックをオーケストレーション

## 2. 主要ファイル

### 基盤

- `main.cpp`
- `core/application.*`
- `core/input.*`
- `core/scene_manager.*`
- `core/scene_registry.*`

### ゲームプレイ

- `gameplay/components.*`  
  汎用、写真、敵、ギミックの主要コンポーネントを集約
- `gameplay/photo_filter_rules.*`  
  フィルター順、ラベル、適用ルール本体
- `gameplay/photo_system.*`  
  写真処理ファサード
- `gameplay/photo_capture_system.*`  
  キャプチャ本体
- `gameplay/photo_paste_system.*`  
  ペースト本体
- `gameplay/photo_shared.*`  
  Capture/Paste 共通処理
- `gameplay/prefab_factory.*`

### `GameScene`

- `scenes/game/game_scene.cpp`  
  `GameScene` コンストラクタのみ
- `scenes/game/game_scene_entry_domain.cpp`  
  `GetSceneId` / `Update` / `Draw` / `GetEventBus`
- `scenes/game/game_scene_lifecycle_domain.cpp`  
  `OnEnter` / `OnExit`
- `scenes/game/game_scene_facade_domain.cpp`  
  フレーム進行のフェーズ制御
- `scenes/game/game_scene_flow_domain.cpp`  
  `UpdatePitRestartFlow` / `UpdateStageTransitionFlow` / `UpdateFrameTimers`
- `scenes/game/game_scene_photo_control_domain.cpp`  
  `UpdatePhotoModes` / カメラ操作 / フィルター入力
- `scenes/game/game_scene_control_domain.cpp`  
  全体ショートカット / `OnCancelAction` / tuning ホットリロード
- `scenes/game/game_scene_gameplay_pipeline_domain.cpp`  
  `RunGameplayFrame` と更新パイプライン
- `scenes/game/game_scene_gameplay.cpp`  
  Player / Battery / Photo 実装の中核
- `scenes/game/game_scene_enemy_domain.cpp`  
  敵ドメイン更新
- `scenes/game/game_scene_gimmick_domain.cpp`  
  ギミックドメイン更新
- `scenes/game/game_scene_marker_spawn_domain.cpp`  
  マーカー駆動の再生成
- `scenes/game/game_scene_map_editor_domain.cpp`  
  マップエディタ入力と保存系
- `scenes/game/game_scene_menu_domain.cpp`  
  Escape メニュー入力と描画
- `scenes/game/game_scene_effects_domain.cpp`  
  エフェクト更新
- `scenes/game/game_scene_debug_domain.cpp`  
  デバッグUI
- `scenes/game/game_scene_entity_query_domain.cpp`  
  エンティティ検索クエリ
- `scenes/game/game_scene_world_interaction_system.h`  
  接触・被弾判定ヘルパ
- `scenes/game/game_scene_render.cpp`
- `scenes/game/game_scene_render_ui.cpp`
- `scenes/game/game_scene_collision.cpp`
- `scenes/game/game_scene_setup.cpp`

## 3. 役割分担の考え方

`GameScene` は「実装集中ファイル」ではなく「呼び出し専用オーケストレータ」に寄せます。  
担当境界は次の 6 ドメイン固定です。

- Player 系
- Capture モード系
- Paste モード系
- フィルター系
- Enemy 系
- ステージギミック系

詳細は `docs/domain-ownership-guide.md` を参照してください。

## 4. 同じ `.cpp` に寄せるべきもの

- フィルタールール本体: `gameplay/photo_filter_rules.cpp`
- Capture/Paste 共通の変換・生成ルール: `gameplay/photo_shared.cpp`
- Enemy/Gimmick 共通の接触ダメージ判定: 共有ヘルパ化して 1 箇所管理

## 4.1 直近で固定した実装ルール

- 写真配置ルール
  - 判定入口は `scenes/game/game_scene_collision.cpp` の `IsPhotoPlacementValid`
  - グループ定義は `PhotoPlacementRuleGroup` + 禁止対象ビットマスクで管理
  - 既定挙動: グループ1は Enemy 禁止、グループ2は Floor + Enemy 禁止
- ペースト描画順
  - `scenes/game/game_scene_render.cpp` の `DrawPastedEntitiesFront` で管理
  - `PhotoPasteOrderComponent` の昇順描画で「後から貼ったものほど前面」を実現
- `GameScene::Update` の責務
  - `scenes/game/game_scene_entry_domain.cpp` は入口オーケストレーションに集中
  - 詳細処理は `facade / flow / photo_control / gameplay_pipeline / control` に委譲

## 5. 分離を優先するもの

- `game_scene_gameplay.cpp` の混在ロジック  
  `Player/Enemy/Gimmick` へ分離
- `photo_system.cpp` の混在ロジック  
  `photo_capture_system.cpp` / `photo_paste_system.cpp` / `photo_shared.cpp` へ分離
- `game_scene_gameplay.cpp` の処理塊  
  `player / battery / photo / world` ごとの関数群へ段階分割

## 6. よくある改修の入口

- フィルター効果を変える: `gameplay/photo_filter_rules.cpp`
- 撮影体験を変える: `gameplay/photo_capture_system.cpp`
- 配置体験を変える: `gameplay/photo_paste_system.cpp`
- 共通の写真生成ルールを変える: `gameplay/photo_shared.cpp`
- 敵の挙動を変える: `scenes/game/game_scene_enemy_domain.cpp`
- ギミック挙動を変える: `scenes/game/game_scene_gimmick_domain.cpp`
- マップマーカー再生成を変える: `scenes/game/game_scene_marker_spawn_domain.cpp`
- エディタ操作を変える: `scenes/game/game_scene_map_editor_domain.cpp`
- HUD/オーバーレイを変える: `scenes/game/game_scene_render_ui.cpp`

## 7. ビルド

メイン実行ファイル:

- `DirectXFoundation.vcxproj`

コマンドライン例:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "D:\DollyFrame_jam\DirectXFoundation.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m:1
```

注意:

- `dxlib_support_libs/` が無いとリンクエラーになります
- 新規 `.cpp/.h` 追加時は `.vcxproj` 登録漏れに注意します
