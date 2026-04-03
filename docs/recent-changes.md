# Recent Changes

最終更新: 2026-04-03

このファイルは、直近の設計変更を短く確認するためのサマリです。

## 反映済み

- `GameScene::Update` のオーケストレーション化
  - 詳細更新処理を `game_scene_update_domains.cpp` の小関数へ分離
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
