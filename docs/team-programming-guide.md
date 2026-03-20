# Team Programming Guide

このドキュメントは、チーム制作メンバー向けのプログラム作業ガイドです。  
目的は「誰がどこを触ると安全か」「どう進めると衝突しにくいか」を共有することです。

## 1. 最初に知っておくこと

このプロジェクトは、完全なエンジンではなく「ゲーム本体にかなり近い基盤」です。  
そのため、見た目を変えるだけでも `GameScene` 側のコードに触ることがあります。

設計レベルで競合を減らす方針は `docs/conflict-free-design.md` にまとめています。  
実装前に「どの state / system の責務か」を先に見ると事故を減らせます。

大事なのは次の 3 つです。

- 変更対象を最初に狭く決める
- 見た目変更とルール変更を混ぜすぎない
- 生成物やバイナリで競合しやすいことを意識する

## 2. 誰がどこを触るべきか

### 演出・UI を触る人

主に見る場所:

- `game_scene_render.cpp`
- `game_scene_render_ui.cpp`
- `game_scene.cpp`
- `shader*.hlsl`
- `assets/`

向いている作業:

- 背景
- オーバーレイ
- フィルター色
- 画面内 UI
- 写真プレビューの見た目

触る前に気をつけること:

- ルール変更まで入れない
- `DrawEntity()` を触ると影響範囲が広い
- HUD やオーバーレイだけなら `game_scene_render_ui.cpp` を優先する

### プレイヤー操作・ゲームルールを触る人

主に見る場所:

- `game_scene_gameplay.cpp`
- `game_scene_collision.cpp`
- `game_scene.h`

向いている作業:

- ジャンプや移動
- 写真撮影と配置
- スロー演出
- 敵との接触
- フィルター効果

触る前に気をつけること:

- 新しい状態を足したら `game_scene.h` を更新する
- `Update()` に処理を詰め込みすぎない

### データ調整を触る人

主に見る場所:

- `assets/prefabs.json`
- `assets/maps/side_scroll_stage01.csv`
- `assets/tuning.json`

向いている作業:

- 配置
- 数値調整
- プレハブ追加

注意:

- 主要配置物は `sandbox_*` prefab から生成する形へ移行している
- 位置は `game_scene_setup.cpp`、構成は `assets/prefabs.json` を見る
- JSON だけで全部は変えられない

### 基盤・ビルドを触る人

主に見る場所:

- `application.*`
- `DirectXFoundation.vcxproj`
- `input.*`
- `audio.*`
- `resource_manager.*`

注意:

- ここを触る変更は影響範囲が広い
- 先にチームへ共有した方がよい

## 3. まず読む順番

初見メンバーは、次の順で読むと把握しやすいです。

1. `docs/programming-guide.md`
2. `game_scene.h`
3. `game_scene.cpp`
4. `game_scene_setup.cpp`
5. `game_scene_gameplay.cpp`
6. `game_scene_render.cpp`
7. `game_scene_render_ui.cpp`
8. `photo_components.h`
9. `world_components.h`
10. `assets/prefabs.json`

## 4. よくある変更パターン

### 新しいフィルターを追加したい

触る場所:

- `photo_components.h`
  `PhotoFilterTheme`
- `game_scene.cpp`
  入力切り替え
- `game_scene_gameplay.cpp`
  元オブジェクトへの効果、コピーへの効果
- `game_scene_render.cpp`
  エンティティ側の見た目
- `game_scene_render_ui.cpp`
  UI、オーバーレイ、プレビュー

### 新しい敵を追加したい

最小手順:

1. `world_components.*` に状態を足す
2. `assets/prefabs.json` に定義し、`game_scene_setup.cpp` に配置を追加する
3. 必要なら `prefab_factory.*` と `assets/prefabs.json` を更新する

## 敵担当向けガイド

### まず見る場所

敵を触る人は、まず次を見ます。

- `world_components.h`
- `world_components.cpp`
- `photo_components.h`
- `game_scene_setup.cpp`
- `game_scene_gameplay.cpp`
- `game_scene_render.cpp`
- `game_scene_render_ui.cpp`

特に重要なのは次のコンポーネントです。

- `EnemyComponent`
- `EnemyMoverComponent`
- `TransformComponent`
- `TintComponent`

### 今の敵の作られ方

現在の敵は `assets/prefabs.json` の `sandbox_*` prefab を `game_scene_setup.cpp` で配置しています。  
つまり今の段階では、構成は prefab、配置はコード側です。

今の基本構成:

- タグ: `"Enemy"`
- `EnemyComponent`
- `EnemyMoverComponent`
- `TransformComponent`
- `TintComponent`
- `SpriteRenderComponent`

### 敵を調整するときに触る場所

#### 動き

見る場所:

- `EnemyMoverComponent::Update()`
- `EnemyMoverComponent::SetFrozen()`
- `EnemyMoverComponent::Rewind()`

ここでできること:

- 軌道変更
- 停止
- 凍結
- 位相巻き戻し

#### 接触ダメージや生死

見る場所:

- `EnemyComponent`
- `HandleWorldInteractions()`
- `RemoveDefeatedEnemies()`

ここでできること:

- 接触ダメージ変更
- 無効化条件変更
- 撃破後の消し方変更

#### 見た目

見る場所:

- `game_scene_render.cpp`
- `TintComponent`

ここでできること:

- 敵の色
- フィルターを受けたときの見た目
- 味方化や凍結の見え方

### 敵を新しく増やすときのおすすめ手順

1. まずは `EnemyComponent` に必要な状態を足す
2. その敵専用の挙動が必要なら新しいコンポーネントを作る
3. `GameScene` で最小構成の 1 体を出す
4. 接触とフィルターの反応を確認する
5. 問題なければ prefab 化を考える

最初から大きく一般化しすぎない方が安全です。

### 敵実装で壊しやすいところ

#### 1. 移動と当たり判定を同時に変える

`EnemyMoverComponent` と `HandleWorldInteractions()` を同時に大きく変えると、  
「見た目上は触れていないのに当たる」みたいなズレが起きやすいです。

#### 2. 凍結や巻き戻しで状態を戻し切れない

`Cold` や `Sepia` のような効果を足すときは、

- 動き
- 生死
- tint
- 有効/無効

のどれを戻す必要があるかを明確にしてください。

#### 3. 敵の見た目変更をルール側へ混ぜる

`game_scene_gameplay.cpp` で tint を直接いじりすぎると、  
後でレンダリング側の責務と混ざります。

判断基準:

- ルール: gameplay
- 表現強化: render

### いまのフィルターと敵の関係

今の敵はフィルターに対して次の反応をします。

- `Hot`
  無効化される
- `Cold`
  凍結して移動停止する
- `Invert`
  敵対を失って停止する
- `Sepia`
  復活状態に戻り、移動位相を少し巻き戻す

敵を追加するときは、最低限この 4 つへの反応を確認した方がよいです。

### 敵担当のレビュー観点

PR を出す前に、自分で次を見ます。

- 敵はプレイヤーにちゃんと当たるか
- フィルターごとの反応が壊れていないか
- 凍結後に動き続けていないか
- 撃破後に残骸判定だけ残っていないか
- 色変更だけでなく挙動も変わっているか

### 将来的にやりたい整理

今の敵は `EnemyComponent + EnemyMoverComponent` ベースの簡易構成です。  
敵の種類が増えたら、次の単位で分けると整理しやすいです。

- `EnemyState`
- `EnemyBehavior`
- `EnemyReactionToFilter`
- `EnemySpawner`

ただし、今はまだそこまで一般化しなくてよいです。  
1 体ずつ安全に増やす方が優先です。

### 新しいギミックを追加したい

最初に確認すること:

- `GimmickComponent` だけで足りるか
- 専用コンポーネントが必要か

よく触る場所:

- `world_components.*`
- `game_scene_gameplay.cpp`
- `game_scene_render.cpp`
- `game_scene_render_ui.cpp`

### 操作感を変えたい

見る場所:

- `UpdatePlayer()`
- `HandlePhotoCapture()`
- `HandlePhotoSpawn()`
- `game_scene_internal.h` の定数

## 5. ブランチと作業の切り方

おすすめ:

- 1 ブランチ 1 テーマ
- 1 PR 1 目的

良い例:

- `filter-sepia`
- `camera-ui-tweak`
- `enemy-freeze-fix`

悪い例:

- フィルター追加
- UI調整
- ビルド設定変更

を同じブランチにまとめること

## 6. 競合しやすい場所

特に競合しやすいのは次です。

- `game_scene.cpp`
- `game_scene_setup.cpp`
- `game_scene_gameplay.cpp`
- `game_scene_render.cpp`
- `game_scene_render_ui.cpp`
- `components.h`
- `components.cpp`
- `photo_components.h`
- `photo_components.cpp`
- `world_components.h`
- `world_components.cpp`
- `DirectXFoundation.vcxproj`
- バイナリ生成物

`GameScene` 系は複数人が触りやすいので、作業前に

- 「ルール側を触る」
- 「見た目側を触る」

を分けて声をかけた方がよいです。

## 7. 触らない方がいいもの

基本的に以下は勝手にいじらない方が安全です。

- `third_party/`
- `build/`
- `foundation.log`
- `.cso`
- `.pdb`
- `.exe`

理由:

- 生成物や依存ライブラリは競合しやすい
- GitHub 上でバイナリ競合になる
- 本質的なレビュー対象になりにくい

## 8. コミット前チェック

最低限これを見ます。

- ビルドは通るか
- 不要な生成物をコミットしていないか
- `foundation.log` が混ざっていないか
- `dxlib_support_libs` を本当に含める必要があるか
- `README` や `docs` の説明がズレていないか

## 9. レビューで見るポイント

### 見た目変更

- 既存の操作感を壊していないか
- 色や UI がフィルターごとに一貫しているか

### ルール変更

- `GameScene` の状態が増えすぎていないか
- 元オブジェクトとコピーの両方に作用して破綻していないか
- 時間制御と演出制御が混ざっていないか

### 基盤変更

- 他シーンへ影響しないか
- 入力全体やリンク設定を壊していないか

## 10. 今のプロジェクトでよく起こる事故

### 1. 入力が効かなくなる

原因になりやすい場所:

- `input.cpp`

対策:

- 使うキーを明示マップで追加する
- 連番変換を安易に入れない

### 2. ビルドは通るのに Git が荒れる

原因:

- `.cso`
- `.lib`
- `.exe`
- `.pdb`

対策:

- 生成物をコミットしない
- どうしても必要な場合は事前共有する

### 3. `GameScene` が読めなくなる

原因:

- 新しい仕様を全部 `Update()` に直書きする

対策:

- 状態は `game_scene.h`
- 初期配置は `game_scene_setup.cpp`
- ルールは `game_scene_gameplay.cpp`
- 描画は `game_scene_render.cpp` と `game_scene_render_ui.cpp`

に分ける

## 11. 相談した方がいい変更

次の変更は、着手前にチームへ言った方がよいです。

- 入力方式の変更
- ビルド設定の変更
- 依存ライブラリの差し替え
- `DirectXFoundation.vcxproj` の変更
- `GameScene` の更新順の変更
- 写真システムの基本仕様変更

## 12. いまの開発で一番大事なこと

このプロジェクトでは、実装を増やすことより「壊れ方を小さくすること」が大事です。

そのために意識すること:

- 小さく切る
- 触る場所を決めてから触る
- 生成物を混ぜない
- `GameScene` の責務を増やしすぎない

迷ったら、まずは

- 何を変えたいか
- その変更は見た目か、ルールか、基盤か

を 1 行で言える状態にしてから作業を始めると事故が減ります。
