# リファクタ候補一覧

最終更新: 2026-04-07

## 完了（今回反映済み）

- 連動ギミック更新/再生成関数の命名整理
  - `UpdateElevatorGimmicks` → `UpdateLinkedGimmicks`
  - `RefreshElevatorGimmicksFromMarkers` → `RefreshLinkedGimmicksFromMarkers`
- `RefreshMarkerDrivenSystemsByMarkerChange` の重複再生成呼び出し削減
- `UpdateLinkedGimmicks` 内の共通化
  - プレイヤー/バッテリー追従処理
  - Tint更新処理
  - linkPowered更新処理
  - マジックナンバー定数化
- マーカー判定/入力処理の整理
  - `IsMarkerInSet` による判定共通化
  - マーカーホットキーのテーブル化
  - 数字キー処理の共通化
- `RefreshLinkedGimmicksFromMarkers` の責務分割/生成整理
  - マーカー収集・リンクID構築・シャッターリンク解決を分離
  - 生成処理のラムダ化とループ整理
- 大きめリファクタ（影響が広い）
  - `UpdateLinkedGimmicks` を `game_scene_gameplay.cpp` から `game_scene_gimmick_domain.cpp` へ分割移動
  - 生成設定（サイズ/色/速度）を `LinkedGimmickSpawnConfig` 構造体へ外出し
- 大きめリファクタ（影響が広い）
  - `game_scene_update_domains.cpp` の責務分割
  - マップエディタ操作群を `game_scene_map_editor_domain.cpp` へ移動
  - マーカー駆動の再生成群を `game_scene_marker_spawn_domain.cpp` へ移動
- 大きめリファクタ（影響が広い）
  - Escapeメニュー更新処理を `game_scene_menu_domain.cpp` へ分割
  - `update_domains` からメニュー関連定数/処理を切り離し

## 優先度高（残り）

なし

## 優先度低（残り）

### 1. ビルド構成の将来改善

進めたいこと:

- `.vcxproj` 競合を減らす運用整理
- 将来的なビルド構成自動化の検討
