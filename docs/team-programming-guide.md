# Team Programming Guide

このドキュメントは、チーム制作メンバー向けのプログラム作業ガイドです。  
目的は「誰がどこを触ると安全か」「どう進めると衝突しにくいか」を共有することです。

## 1. 基本方針

- 1 ブランチ 1 テーマ
- 1 PR 1 ドメイン
- `GameScene` は呼び出し専用オーケストレータとして扱う

## 2. 担当境界（固定）

- Player 系
- Capture モード系
- Paste モード系
- フィルター系
- Enemy 系
- ステージギミック系

担当詳細は `docs/domain-ownership-guide.md` を参照。

## 3. よく触るファイル

- `scenes/game/game_scene.cpp`
- `scenes/game/game_scene_entry_domain.cpp`
- `scenes/game/game_scene_lifecycle_domain.cpp`
- `scenes/game/game_scene_facade_domain.cpp`
- `scenes/game/game_scene_flow_domain.cpp`
- `scenes/game/game_scene_photo_control_domain.cpp`
- `scenes/game/game_scene_control_domain.cpp`
- `scenes/game/game_scene_gameplay_pipeline_domain.cpp`
- `scenes/game/game_scene_gameplay.cpp`
- `scenes/game/game_scene_enemy_domain.cpp`
- `scenes/game/game_scene_gimmick_domain.cpp`
- `scenes/game/game_scene_map_editor_domain.cpp`
- `scenes/game/game_scene_marker_spawn_domain.cpp`
- `scenes/game/game_scene_menu_domain.cpp`
- `scenes/game/game_scene_render.cpp`
- `scenes/game/game_scene_render_ui.cpp`
- `gameplay/components.*`
- `gameplay/photo_filter_rules.*`
- `gameplay/photo_capture_system.*`
- `gameplay/photo_paste_system.*`
- `gameplay/photo_shared.*`

## 4. 作業の切り方

良い例:

- `enemy-contact-damage-tuning`
- `capture-cursor-feel`
- `filter-sepia-rule`

避ける例:

- 敵挙動変更 + UI 配色変更 + `.vcxproj` 追加を同時に行う

## 5. 競合しやすい場所

- `game_scene.cpp`
- `game_scene_entry_domain.cpp`
- `game_scene_control_domain.cpp`
- `game_scene_gameplay.cpp`
- `components.*`
- `DirectXFoundation.vcxproj`

回避策:

1. 先に担当境界を宣言
2. 中央ファイル変更を最小化
3. ファイル追加 PR と機能 PR を分ける

## 6. コミット前チェック

- ビルドが通るか
- 新規ファイルの `.vcxproj` 反映漏れがないか
- 生成物を混ぜていないか
- docs 更新が必要なら同じコミットで反映したか

## 7. 相談した方がいい変更

- 入力方式の変更
- `DirectXFoundation.vcxproj` の大きな変更
- `GameScene` 更新順の変更
- 写真システムの基本仕様変更
