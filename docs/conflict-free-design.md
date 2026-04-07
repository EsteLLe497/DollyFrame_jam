# Conflict-Free Design

このドキュメントは、Git conflict を減らすための設計方針です。  
前提は「`GameScene` は呼び出し専用オーケストレータ」「責務は既存ドメインへ分散済み」です。

## 1. 担当境界

固定する担当境界:

- 入口・ライフサイクル系
- フロー/全体制御系
- 写真操作系
- ゲームプレイ更新パイプライン系
- エネミー系
- ギミック系
- マップエディタ/メニュー/描画エフェクト系

この境界を崩さないことを最優先にします。

## 2. `GameScene` の役割

`GameScene` に残す責務:

- シーンライフサイクル
- 更新順の定義
- 各ドメイン呼び出し
- 大きなモード遷移

`GameScene` に残さない責務:

- 個別ドメインの詳細ロジック
- ドメイン固有状態の直接管理

## 3. 同じ `.cpp` に寄せる判断

寄せる:

- フィルタールール本体 -> `gameplay/photo_filter_rules.cpp`
- Capture/Paste 共通処理 -> `gameplay/photo_shared.cpp`
- Enemy/Gimmick 共通の接触ダメージ判定 -> 共有ヘルパへ統合

分ける:

- `game_scene_gameplay.cpp` の Player/Enemy/Gimmick 混在
- `photo_system.cpp` の Capture/Paste 混在
- `game_scene_control_domain.cpp` / `game_scene_photo_control_domain.cpp` での入力責務混在

## 4. 競合源と回避

競合しやすい場所:

- `scenes/game/game_scene_entry_domain.cpp`
- `scenes/game/game_scene_control_domain.cpp`
- `scenes/game/game_scene_photo_control_domain.cpp`
- `scenes/game/game_scene_gameplay.cpp`
- `gameplay/components.*`
- `DirectXFoundation.vcxproj`

回避ルール:

1. 1 PR 1 ドメイン
2. ルール変更と見た目変更を分離
3. 新規 `.cpp/.h` 追加と機能変更を分離
4. `GameScene` には入口呼び出しだけ追加（詳細ロジックは各ドメインへ）

## 5. 運用ルール

- 新しい状態を足す前に「どのドメイン所有か」を決める
- `components.*` に追加するときは写真系か敵系かを明記する
- 生成物（`.exe`, `.pdb`, `.lib`, ログ）をコミットしない
- 変更したら関連 `docs/` を同時更新する

## 6. 実装移行の優先順

1. `game_scene_gameplay.cpp` の混在ロジックを分割
2. `photo_system.cpp` をファサード用途に限定
3. Enemy/Gimmick 共通ダメージ判定を共有化
4. 残存する混在ロジックを対象ドメインへ移譲

詳細な担当表は `docs/domain-ownership-guide.md` を参照してください。
