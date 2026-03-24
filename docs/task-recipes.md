# 実装レシピ集

このファイルは、チームメンバーが「何をどこから触ればいいか」を最短で追えるようにするための実例集です。

## 使い方

- 全体像: `docs/programming-guide.md`
- 担当境界: `docs/domain-ownership-guide.md`
- 競合回避: `docs/conflict-free-design.md`

## レシピ1: 敵を1体追加する

### まず見るファイル

- `gameplay/components.h`
- `scenes/game/game_scene_enemy_domain.cpp`
- `scenes/game/game_scene_setup.cpp`
- `scenes/game/game_scene_render.cpp`

### 最短手順

1. `EnemyArchetype` や `EnemyComponent` に必要な状態を追加
2. `assets/prefabs.json` に敵 prefab を追加
3. `game_scene_setup.cpp` に配置を追加
4. `game_scene_enemy_domain.cpp` で挙動・接触を実装
5. `game_scene_render.cpp` で見た目を確認

## レシピ2: ギミックを1個追加する

### まず見るファイル

- `gameplay/components.h`
- `scenes/game/game_scene_gimmick_domain.cpp`
- `scenes/game/game_scene_setup.cpp`
- `assets/prefabs.json`

### 最短手順

1. `GimmickType` と必要状態を追加
2. prefab 定義を追加
3. 配置を追加
4. `game_scene_gimmick_domain.cpp` に挙動を追加

## レシピ3: フィルターを1個追加する

### まず見るファイル

- `gameplay/components.h`
- `gameplay/photo_filter_rules.cpp`
- `scenes/game/game_scene_update_domains.cpp`
- `scenes/game/game_scene_render_ui.cpp`
- `docs/filter-spec.md`

### 最短手順

1. `PhotoFilterTheme` を追加
2. `photo_filter_rules.cpp` に表示名・順序・効果を追加
3. フィルター入力切り替えを更新
4. UI 色や表示を更新
5. `docs/filter-spec.md` を更新

## レシピ4: 写真配置の感触を調整する

### まず見るファイル

- `gameplay/photo_capture_system.cpp`
- `gameplay/photo_paste_system.cpp`
- `gameplay/photo_shared.cpp`

### 最短手順

1. 撮影処理は `photo_capture_system.cpp` で調整
2. 配置入力と確定条件は `photo_paste_system.cpp` で調整
3. 共通の生成・回転・ブリッジは `photo_shared.cpp` で調整

## レシピ5: 変更をレビュー前に確認する

- ビルドが通るか
- 新規 `.cpp/.h` の `.vcxproj` 登録漏れがないか
- ルール変更と見た目変更が混ざりすぎていないか
- 不要な生成物を混ぜていないか
- 関連 `docs/` を更新したか
