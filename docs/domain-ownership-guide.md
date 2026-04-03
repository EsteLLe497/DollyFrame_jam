# Domain Ownership Guide

このドキュメントは、担当者が「自分の領域だけ触ればよい」状態を保つための境界定義です。

## 固定ドメイン

- Player 系
- キャプチャーモード系
- ペーストモード系
- フィルター系
- エネミー系
- ステージギミック系

## ファイル責務

### Player 系

- `scenes/game/game_scene_player_system.h`
- `scenes/game/game_scene_player_movement_system.h`
- `scenes/game/game_scene_player_visual_system.h`

### キャプチャーモード系

- `gameplay/photo_capture_system.h`
- `gameplay/photo_capture_system.cpp`

### ペーストモード系

- `gameplay/photo_paste_system.h`
- `gameplay/photo_paste_system.cpp`

### フィルター系

- `gameplay/photo_filter_rules.h`
- `gameplay/photo_filter_rules.cpp`

### エネミー系

- `scenes/game/game_scene_enemy_domain.cpp`
- `gameplay/components.*`（Enemy 関連）

### ステージギミック系

- `scenes/game/game_scene_gimmick_domain.cpp`
- `gameplay/components.*`（Gimmick 関連）

## 共有化してよい場所

- Capture/Paste 共通ルール: `gameplay/photo_shared.*`
- Enemy/Gimmick 共通接触ダメージ: 共有ヘルパ（新規/既存）

## 2026-04-03 時点の反映済み整理

- `scenes/game/game_scene.cpp`
  - `Update()` はオーケストレーション中心に整理
  - 詳細更新は `UpdateTuningHotReload` / `HandleGlobalSceneShortcuts` / `UpdatePitRestartFlow` / `UpdateStageTransitionFlow` / `UpdateFrameTimers` に委譲
- `scenes/game/game_scene_gameplay.cpp`
  - Enemy/Gimmick 共通の被弾入口として `ApplyHazardDamageToPlayer` を利用
- `scenes/game/game_scene_collision.cpp`
  - 写真配置可否は「ルール定義 + 禁止対象ビットマスク」で判定
  - グループ追加時はルール定義側の拡張で対応
- `scenes/game/game_scene_render.cpp`
  - ペースト済みオブジェクトの前面描画は `PhotoPasteOrderComponent` を基準に安定ソート
  - 同一 order はレイヤ優先度で並びを安定化

## 触らない方針

- `scenes/game/game_scene.cpp` にドメイン詳細を追加しない
- `scenes/game/game_scene_gameplay.cpp` を仕様実装の主戦場にしない
- 1 つの PR で複数ドメインを跨ぎすぎない

## PR の基本ルール

1. タイトルにドメイン名を入れる
2. 1 PR 1 ドメインを原則にする
3. 共有ヘルパ変更は影響ドメインを明記する
