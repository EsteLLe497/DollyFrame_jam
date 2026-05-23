# リファクタ候補一覧

最終更新: 2026-04-08

## 完了（今回反映済み）

- `RotatePoint` の共通化
  - `game_scene_internal.h` に寄せて、複数ファイルの重複実装を削除
- `core/input.cpp` の整理
  - ゲームパッド判定の共通化
  - アクション名 / キー名 / predicate 名のテーブル化
- `game_scene_render_ui.cpp` の分割
  - 共通 helper を `game_scene_render_ui_helpers.cpp` へ切り出し
- `photo_shared` と UI 補助の共通描画ヘルパー化
  - `DrawTriangleItem` / `DrawProjectileItem` を `game_scene_draw_helpers.h` に集約
- `ResourceManager` のキャッシュ予約
  - `AssetManifest` から見積もり件数を渡して rehash を抑制
- `SceneRegistry` の透過検索化
  - `Create` / `Contains` の一時 `std::string` 生成を削減
- `Logger` の共通ログ入口化
  - `Info` / `Warn` / `Error` を共通ヘルパーへ集約
  - `spdlog` 呼び出し時の一時 `std::string` 生成を削減
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
- 大きめリファクタ（影響が広い）
  - `GameScene` を Facade 化する下地として、`Update/Draw` のフェーズ関数を導入
  - フェーズ実装を `game_scene_facade_domain.cpp` へ分割
- 大きめリファクタ（影響が広い）
  - フロー制御 (`UpdatePitRestartFlow` / `UpdateStageTransitionFlow` / `UpdateFrameTimers`) を `game_scene_flow_domain.cpp` へ分割
  - `update_domains` からフロー責務を切り離し
- 大きめリファクタ（影響が広い）
  - `RunGameplayFrame` を `game_scene_gameplay_pipeline_domain.cpp` へ分割
  - ゲーム更新パイプラインを `UpdateGameplayActors` / `ResolveGameplayOutcomes` / `FlushPendingEntities` に整理
- 大きめリファクタ（影響が広い）
  - `PhotoControl` 系 (`UpdateCameraMode` / `UpdatePhotoModes` / `UpdateCaptureFinderZoomInput` / `ProcessFilterInput`) を `game_scene_photo_control_domain.cpp` へ分割
  - `update_domains` から写真操作責務を切り離し
- 大きめリファクタ（影響が広い）
  - `SceneControl` 系 (`UpdateTuningHotReload` / `HandleGlobalSceneShortcuts`) を `game_scene_control_domain.cpp` へ分割
  - `game_scene_update_domains.cpp` を廃止（責務移管完了）
- 大きめリファクタ（影響が広い）
  - `UpdateEffects` を `game_scene_effects_domain.cpp` へ分割
  - `DrawDebugUI` を `game_scene_debug_domain.cpp` へ分割
  - `DrawEscapeMenuOverlay` を `game_scene_menu_domain.cpp` へ移管
- 大きめリファクタ（影響が広い）
  - `OnCancelAction` を `game_scene_control_domain.cpp` へ移管
  - `game_scene.cpp` をエントリーファイル寄りに整理
- 大きめリファクタ（影響が広い）
  - `FindEntityByTag` を `game_scene_entity_query_domain.cpp` へ分割
  - `game_scene.cpp` からエンティティ検索責務を分離
- 大きめリファクタ（影響が広い）
  - `OnEnter` / `OnExit` を `game_scene_lifecycle_domain.cpp` へ分割
  - `game_scene.cpp` をライフサイクル責務から分離
- 大きめリファクタ（影響が広い）
  - `GetSceneId` / `Update` / `Draw` / `GetEventBus` を `game_scene_entry_domain.cpp` へ分割
  - `game_scene.cpp` をコンストラクタ実装のみに整理
- 大きめリファクタ（影響が広い）
  - `game_scene.h` の宣言を責務セクションで整理
  - 不要な `<filesystem>` include を削除

## 優先度高（残り）

なし

## 優先度低（残り）

### 1. ビルド構成の将来改善

進めたいこと:

- `.vcxproj` 競合を減らす運用整理
- 将来的なビルド構成自動化の検討
