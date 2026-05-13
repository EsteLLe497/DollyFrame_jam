# Recent Changes

最終更新: 2026-04-08

このファイルは、直近の設計変更を短く確認するためのサマリです。

## 反映済み

- `RotatePoint` を共通化
  - `game_scene_internal.h` に寄せて、描画/衝突/UI の重複を削除
- `core/input.cpp` を整理
  - ゲームパッド判定の重複を共通化
  - 入力名の解決をテーブル化して、分岐を減らした
- `game_scene_render_ui.cpp` を分割
  - `game_scene_render_ui_helpers.cpp` に helper 群を移した
- `photo_shared` と UI 補助の描画重複を整理
  - `DrawTriangleItem` / `DrawProjectileItem` を `game_scene_draw_helpers.h` に共通化
- `ResourceManager` / `AssetManifest` の軽量化
  - テクスチャキャッシュを事前予約するように変更
- `core` 基盤の軽量化
  - `SceneRegistry` の検索で一時 `std::string` を生成しないように変更
  - `Logger` の `Info` / `Warn` / `Error` を共通化し、`spdlog` に直接 `string_view` を渡すように変更
- `GameScene` の大規模責務分割
  - 入口: `game_scene_entry_domain.cpp`
  - ライフサイクル: `game_scene_lifecycle_domain.cpp`
  - フレーム進行: `game_scene_facade_domain.cpp`
  - フロー: `game_scene_flow_domain.cpp`
  - 写真操作: `game_scene_photo_control_domain.cpp`
  - 全体制御: `game_scene_control_domain.cpp`
  - 更新パイプライン: `game_scene_gameplay_pipeline_domain.cpp`
  - エフェクト: `game_scene_effects_domain.cpp`
  - デバッグUI: `game_scene_debug_domain.cpp`
  - エンティティ検索: `game_scene_entity_query_domain.cpp`
- 旧 `game_scene_update_domains.cpp` を廃止
- 写真配置ルールの整理
  - `PhotoPlacementRuleGroup` ごとの禁止対象をビットマスクで評価
  - 既定: Group1 は Enemy 禁止、Group2 は Floor + Enemy 禁止
- ペースト済みオブジェクトの描画順整理
  - `PhotoPasteOrderComponent` を基準に前面描画を安定化
  - 後からペーストしたオブジェクトほど前面
- Enemy/Gimmick 共通被弾入口の共有化
  - `ApplyHazardDamageToPlayer` 経由でダメージ処理を統一
- `gameplay/components.*` の責務境界コメント整理
  - 写真系、敵系、ギミック系、描画/物理系の境界を明確化

## 未完了の低優先候補

- `.vcxproj` の競合削減運用
- 将来的なビルド構成自動化
