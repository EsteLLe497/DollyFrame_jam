# 実装レシピ集

このファイルは、チームメンバーが「何をどこから触ればいいか」を最短で追えるようにするための実例集です。

## 使い方

- まずこのファイルで作業の流れを見る
- 詳しい全体像は `docs/programming-guide.md` を読む
- チーム内の作業ルールは `docs/team-programming-guide.md` を読む
- データや仕様の前提は `docs/data-formats.md` と `docs/filter-spec.md` を見る

## レシピ1: 敵を1体追加する

### 目的

ゲーム内に新しい敵を1体置きたい。

### まず見るファイル

- `components.h`
- `components.cpp`
- `game_scene.cpp`
- `game_scene_gameplay.cpp`
- `game_scene_render.cpp`

### 最短手順

1. `components.h` の `EnemyArchetype` が足りなければ種類を追加する
2. 敵が使う状態を `EnemyComponent` か `EnemyMoverComponent` に追加する
3. `game_scene.cpp` の敵生成処理で、その敵を生成する
4. `game_scene_gameplay.cpp` で移動や接触時の挙動を追加する
5. `game_scene_render.cpp` で見た目や色を追加する

### 例

- 「左右移動しかしない敵」を増やすだけなら、新しいクラスを作らず既存の `EnemyMoverComponent` の設定差分で済むことが多い
- 「撮られると特殊反応する敵」を作るなら、`ApplyPhotoFilterToCapturedTarget()` 側も確認する

### 壊しやすい点

- `defeated` と `enabled` の意味を混ぜると壊れやすい
- フィルター反応を追加したのに、コピー側と元オブジェクト側の両方を見ていない
- 見た目だけ追加して接触処理を追加していない

### 作業後チェック

- プレイヤー接触時に意図通り反応するか
- `Hot / Cold / Invert / Sepia` に対して破綻しないか
- 撮影後の元オブジェクトと、貼り付け後のコピーで挙動が変にならないか

## レシピ2: ギミックを1個追加する

### 目的

新しいワールドオブジェクトや補助装置を追加したい。

### まず見るファイル

- `components.h`
- `game_scene.cpp`
- `game_scene_gameplay.cpp`
- `prefab_factory.cpp`
- `assets/prefabs.json`

### 最短手順

1. `components.h` の `GimmickType` に必要なら新しい種類を追加する
2. 必要な状態を新しいコンポーネントか既存コンポーネントに足す
3. `game_scene.cpp` か `PrefabFactory` で生成できるようにする
4. `game_scene_gameplay.cpp` に接触・有効化・無効化の処理を足す
5. 将来的に使い回すなら `assets/prefabs.json` に prefab を追加する

### 例

- 1回だけ反応する装置なら `GimmickComponent` の `singleUse` と `enabled` を使う
- ステージごとに何度も置くなら、直書きより `prefab` 化した方が安全

### 壊しやすい点

- `game_scene.cpp` にだけ置いて、`PrefabFactory` 側を放置する
- `enabled=false` の見た目がなく、動いていないだけに見える
- リセット時に元へ戻す処理を入れていない

### 作業後チェック

- リトライ後に正しく復元されるか
- フィルターや写真コピーと干渉したとき破綻しないか
- 見た目だけでなく、当たり判定も意図通りか

## レシピ3: フィルターを1個追加する

### 目的

`Hot / Cold / Invert / Sepia` に続く新しい写真フィルターを増やしたい。

### まず見るファイル

- `components.h`
- `prefab_factory.cpp`
- `game_scene.cpp`
- `game_scene_gameplay.cpp`
- `game_scene_render.cpp`
- `docs/filter-spec.md`

### 最短手順

1. `components.h` の `PhotoFilterTheme` に新しい theme を追加する
2. `game_scene.cpp` の入力切り替えに追加する
3. `game_scene_render.cpp` に UI 名称と色を追加する
4. `game_scene_gameplay.cpp` のコピー側適用処理に追加する
5. `game_scene_gameplay.cpp` の元オブジェクト側適用処理に追加する
6. `docs/filter-spec.md` に仕様を追記する

### 例

- 「毒フィルター」なら、コピー側は危険床、元オブジェクト側は継続ダメージという分け方ができる
- 「時間停止フィルター」なら、元オブジェクト側だけ停止し、コピー側は通常ブロックにする手もある

### 壊しやすい点

- コピー側にしか効果を入れていない
- 撮影時の枠色や UI 名が更新されていない
- キー切り替え順と表示順が一致していない

### 作業後チェック

- 切り替え操作でちゃんと選べるか
- 撮影枠の色が変わるか
- 元オブジェクトとコピーの両方に仕様通り反映されるか

## レシピ4: prefab を1個追加する

### 目的

`assets/prefabs.json` から新しい配置物を作れるようにしたい。

### まず見るファイル

- `assets/prefabs.json`
- `prefab_factory.cpp`
- `data-formats.md`

### 最短手順

1. `assets/prefabs.json` に新しいエントリを追加する
2. 必要なキーを `PrefabFactory` が読めるか確認する
3. `game_scene.cpp` か対象シーンで prefab 名を指定して生成する
4. 色、サイズ、タグ、追加コンポーネントが想定通りか確認する

### 壊しやすい点

- JSON に書いただけで、`PrefabFactory` 側が未対応
- 既存 prefab と似た名前で混同しやすい
- コード直書きと prefab 定義が二重管理になる

## レシピ5: ステージに物を置く

### 目的

テスト用に敵やギミックを置きたい。

### まず見るファイル

- `game_scene.cpp`
- `docs/level-editing-guide.md`

### 最短手順

1. まずグリッド単位で置くサイズを決める
2. `tileSize` を基準に座標を置く
3. 可能なら `AlignToGrid()` を通す
4. 見た目、当たり判定、写真への映り方を確認する

### 壊しやすい点

- グリッドに乗っていない
- 見た目サイズと当たり判定サイズがズレている
- プレイヤーと同じレイヤに重なって見づらくなる

## レシピ6: 変更をレビューに出す前に見ること

- ビルドが通るか
- 新しく足した仕様を `docs/` に反映したか
- `GameScene` の1か所だけで閉じず、描画・入力・挙動が揃っているか
- `foundation.log` など不要な生成物を混ぜていないか
- 競合しやすいバイナリを不用意に触っていないか

## 迷ったときの基準

- 一度しか使わない試作なら `game_scene.cpp` 直書きでもよい
- 何度も置くなら prefab 化する
- 見た目だけではなく、接触、復元、フィルター反応まで揃えて初めて完成
- 触る場所が多すぎると感じたら、`GameScene` に責務が集まりすぎている可能性が高い
