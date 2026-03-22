# リファクタ候補一覧

このファイルは、今すぐ直すものではなく、今後の改善候補を整理するためのメモです。

## 優先度高

### 1. `GameScene` の責務分割

現状の `GameScene` は責務が集中しすぎています。

- 入力
- プレイヤー更新
- 写真撮影
- 写真配置
- フィルター反応
- 敵
- ギミック
- 描画補助

が近い場所に集まっています。

候補:

- `PhotoSystem`
- `EnemySystem`
- `GimmickSystem`
- `FilterRules`
- `PlacementSystem`

への分割

進捗:

- `game_scene_state.h` を追加し、`GameScene` の状態を `m_flow` / `m_player` / `m_debug` へ集約開始
- `game_scene_photo_state.h` を追加し、写真系 state を `game_scene.h` から分離
- `HandleWorldInteractions()` の写真コピーまわりは責務ごとの補助関数へ整理済み
- 写真スロット入力は `game_scene_photo_tray_system.h` へ分離
- プレイヤー入力と回避開始判定は `game_scene_player_system.h` へ分離
- プレイヤー移動解決とカメラ追従は `game_scene_player_movement_system.h` へ分離
- プレイヤー見た目更新と残像制御は `game_scene_player_visual_system.h` へ分離
- ワールド接触と被弾判定は `game_scene_world_interaction_system.h` へ分離
- tuning panel は固定描画から ImGui ベースへ移行
- 敵 AI / bullet 更新は `game_scene_combat_system.h` へ分離
- プレイヤー回避は `dodge_speed` / `dodge_distance` / `dodge_invincibility` / `dodge_cooldown` を tuning tool から調整可能にした
- `EnemyComponent` / `GimmickComponent` / `EnemyMoverComponent` は `world_components.*` へ分離済み

残りの主課題:

- `m_photo` を含む写真状態の所有をさらに明確化する
- `GameScene` 本体を orchestration 中心に寄せる
- state ごとに system 依存をさらに分離する

### 2. フィルター適用処理の独立

今はフィルターのロジックが `GameScene` 側に寄っています。

進捗:

- `photo_filter_rules.*` を導入し、キャプチャ対象とコピー生成物への適用処理は集約済み
- テーマ名 / 効果文 / UI オーバーレイ色 / 配置プレビュー反映も `photo_filter_rules.*` 側へ寄せ始めている
- 写真系の列挙とコンポーネントは `photo_components.*` へ分離済み

候補:

- 元オブジェクト側とコピー側の処理を整理する
- `Hot / Cold / Invert / Sepia` の追加コストを下げる
- 残る描画演出用のテーマ色も段階的に寄せる

### 3. 生成物運用の整理

`.cso` や補助ライブラリが運用事故を起こしやすいです。

候補:

- どこまで Git 管理するか固定する
- `.gitignore` とビルド手順を揃える
- merge 競合しやすいものを減らす

## 優先度中

### 4. prefab と直書き配置の整理

今はコード直書きと prefab が混在しています。

進捗:

- `GameScene` の主要配置物は `assets/prefabs.json` の `sandbox_*` prefab から生成する形へ移行済み
- コード側は「どの prefab をどこへ置くか」に寄せ、構成詳細は prefab 側へ移動済み

候補:

- 使い回すものは prefab 化を進める
- 一時検証だけ直書きを許す
- どこまでデータ駆動にするか決める

### 5. 入力管理の見直し

入力層で明示的にキー対応しています。

進捗:

- 主要シーン遷移と `GameScene` の主要操作は `InputAction` / `InputAxis` 化済み
- まだ一部の補助挙動は直接キー参照が残っています

候補:

- 使用中キーの一覧化
- アクション単位の入力定義を残り箇所へ拡張
- シーンごとの入力責務整理

### 6. コンポーネント定義の競合分散

`components.h` / `components.cpp` に定義が集中すると、複数人作業で衝突しやすいです。

進捗:

- 写真系の列挙とコンポーネントは `photo_components.h` / `photo_components.cpp` へ分離済み
- `GameScene` と `photo_filter_rules` は写真系ヘッダを直接参照する形へ整理済み
- 敵、ギミック、敵移動は `world_components.h` / `world_components.cpp` へ分離済み

候補:

- 物理系コンポーネントの分離
- 描画系コンポーネントの分離
- `components.*` を汎用・描画・物理でさらに責務分割

### 7. 描画ヘルパの整理

`game_scene_render.cpp` に UI と演出が集まりやすいです。

進捗:

- HUD / 背景 / 撮影オーバーレイ / 写真プレビューを `game_scene_render_ui.cpp` へ分離済み
- `game_scene_render.cpp` はエンティティ描画中心に整理済み

候補:

- HUD 描画補助
- グリッド描画

メモ:

- フィルターの基本色定義と説明文は `photo_filter_rules.*` 側へ移行を開始済み

を切り出す

## 優先度低

### 8. ドキュメント更新フローの整備

今は説明書を増やしている段階で、更新ルールはまだ弱いです。

候補:

- 変更種別ごとの更新対象一覧
- PR テンプレートに docs 更新確認を入れる

### 9. シーン登録まわりの整理

`SceneManager` と `SceneRegistry` は今すぐ大きな問題ではないですが、将来的にシーン追加が増えると整理余地があります。

## 判断基準

- 新しい人が触るたびに迷う場所は優先度を上げる
- merge 事故が起きやすい場所は優先度を上げる
- 単にきれいにしたいだけの変更は後回しにする
