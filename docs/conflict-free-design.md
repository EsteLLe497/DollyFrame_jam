# Conflict-Free Design

このドキュメントは、Git conflict を減らすための設計方針です。  
前提は「`GameScene` は呼び出し専用オーケストレータ」「担当は 6 ドメイン固定」です。

## 1. 担当境界

固定する担当境界:

- Player 系
- キャプチャーモード系
- ペーストモード系
- フィルター系
- エネミー系
- ステージギミック系

この 6 境界を崩さないことを最優先にします。

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
- `game_scene.cpp` の入力分岐と個別挙動

## 4. 競合源と回避

競合しやすい場所:

- `scenes/game/game_scene.cpp`
- `scenes/game/game_scene_gameplay.cpp`
- `gameplay/components.*`
- `DirectXFoundation.vcxproj`

回避ルール:

1. 1 PR 1 ドメイン
2. ルール変更と見た目変更を分離
3. 新規 `.cpp/.h` 追加と機能変更を分離
4. `GameScene` にはドメイン呼び出しだけ追加

## 5. 運用ルール

- 新しい状態を足す前に「どのドメイン所有か」を決める
- `components.*` に追加するときは写真系か敵系かを明記する
- 生成物（`.exe`, `.pdb`, `.lib`, ログ）をコミットしない
- 変更したら関連 `docs/` を同時更新する

## 6. 実装移行の優先順

1. `game_scene_gameplay.cpp` の混在ロジックを分割
2. `photo_system.cpp` をファサード用途に限定
3. Enemy/Gimmick 共通ダメージ判定を共有化
4. `game_scene.cpp` の分岐をドメイン関数へ移譲

詳細な担当表は `docs/domain-ownership-guide.md` を参照してください。
