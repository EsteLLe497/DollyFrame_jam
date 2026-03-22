# Conflict-Free Design

このドキュメントは、`DollyFrame_jam` を複数人で触っても Git conflict が起きにくい形へ寄せるための設計方針です。  
理想論ではなく、今のリポジトリ構成と `GameScene` の実装量を前提にしています。

## 目的

このプロジェクトで避けたいのは次の 3 つです。

- 複数人が同じ `GameScene` ファイルを同時に触ること
- 新機能のたびに `game_scene.h` と `DirectXFoundation.vcxproj` が毎回変更されること
- 見た目変更、ルール変更、データ変更が同じ差分に混ざること

## 現在の競合源

実際に衝突源になっている場所は次です。

- `scenes/game/game_scene.h`
  状態が集まりすぎていて、新しい仕様を足すたびに触りやすい
- `scenes/game/game_scene.cpp`
  入力、更新順、シーン制御が集まる
- `scenes/game/game_scene_gameplay.cpp`
  プレイヤー、敵、相互作用、ダメージ処理が集まる
- `scenes/game/game_scene_render.cpp`
  描画分岐が多く、演出担当が集中しやすい
- `scenes/game/game_scene_setup.cpp`
  配置、初期化、prefab 呼び出しの集約点になっている
- `gameplay/components.*`
  汎用コンポーネント追加のたびに集中しやすい
- `DirectXFoundation.vcxproj`
  新規ファイル追加のたびに衝突しやすい

## 基本方針

衝突を減らすには、ファイルを分けるだけでは足りません。  
次の 4 つを同時に守る必要があります。

1. 状態の所有者を分ける
2. 更新責務を分ける
3. 見た目とルールを分ける
4. 登録地点を減らす

## 目標構造

`GameScene` は「全部を実装する場所」ではなく、各機能を束ねるオーケストレータに下げます。

### 1. `GameScene` が持つもの

`GameScene` に残す責務は次だけです。

- シーンライフサイクル
- フレーム更新順の定義
- 共通サービスの受け渡し
- 大きなモード遷移

`GameScene` は詳細ロジックを持たず、各 feature context を呼ぶだけにします。

### 2. 状態を feature 単位へ分割する

`game_scene.h` に直接フィールドを増やし続けないよう、状態を次の単位に分けます。

- `scenes/game/player_state.h`
  プレイヤー移動、速度、接地、回避、見た目補間
- `scenes/game/photo_state.h`
  撮影、保存スロット、配置プレビュー、コピーグループ
- `scenes/game/world_state.h`
  敵数、ゴール解放、マップ参照、進行フラグ
- `scenes/game/debug_state.h`
  チューニング UI、デバッグ表示、ホットリロード

`GameScene` 側は各 state を 1 つずつ持つだけにします。

## 更新責務の分割

今後の主な feature 単位は次です。

- `game_player_system.*`
  入力解釈、移動、接地、回避、ダメージ受付
- `game_photo_system.*`
  撮影、保存、配置、コピー生成
- `game_world_system.*`
  敵、ギミック、ゴール、pickup、接触解決
- `game_render_system.*`
  ワールド描画、キャラ描画、写真コピー描画
- `game_ui_system.*`
  HUD、オーバーレイ、デバッグ UI、チューニング UI
- `game_stage_loader.*`
  prefab 生成、CSV 読み込み、初期配置

重要なのは、各 system が自分の state だけを主に変更することです。

## 依存方向

依存は次の向きに固定します。

- `core` -> 何にも依存しない
- `gameplay` -> `core` に依存してよい
- `scenes/game` -> `core` と `gameplay` に依存してよい
- feature system 同士は直接書き換えない

feature system 同士のやりとりは次で行います。

- 引数で必要 state を受け取る
- `EventBus`
- 小さい共有サービス

他 feature の内部状態へ直接触り始めると、また同じ場所に変更が集中します。

## 担当境界

並行開発の担当境界は次で切るのが安全です。

### ルール担当

触る場所:

- `game_player_system.*`
- `game_world_system.*`
- `gameplay/world_components.*`

原則:

- 描画コードを触らない
- UI 文言を変えない

### 写真担当

触る場所:

- `game_photo_system.*`
- `gameplay/photo_components.*`
- `gameplay/photo_filter_rules.*`

原則:

- プレイヤー移動の責務を持たない
- `GameScene` 本体へ分岐を増やさない

### 演出担当

触る場所:

- `game_render_system.*`
- `game_ui_system.*`
- `shaders/src/*`

原則:

- ダメージ計算や接触判定を触らない
- gameplay 側の bool を増やしたくなったら先に state 設計を見直す

### ステージ担当

触る場所:

- `game_stage_loader.*`
- `assets/prefabs.json`
- `assets/maps/*.csv`
- `assets/tuning.json`

原則:

- ルール本体を書き換えない
- 配置や値の変更を優先する

## 追加時に中央ファイルを触らない設計

競合を減らすには、新機能追加時の「中央登録」を減らす必要があります。

### コンポーネント追加

追加先は責務別に固定します。

- 汎用: `gameplay/components.*`
- 写真: `gameplay/photo_components.*`
- ワールド: `gameplay/world_components.*`

新しい敵用状態を `components.*` に入れないことが重要です。

### 描画追加

`DrawEntity()` に全部追加しない方針にします。  
対象ごとに描画関数を分けます。

- `DrawPlayer`
- `DrawPhotoBox`
- `DrawInteractiveObject`
- `DrawWorldDebug`

`DrawEntity()` は dispatch だけにします。

### 相互作用追加

`HandleWorldInteractions()` に全部追記しない方針にします。

- `ResolveTileInteractions`
- `ResolveEnemyInteractions`
- `ResolveGimmickInteractions`
- `ResolvePhotoCopyInteractions`

この単位まで切っておくと担当ごとにファイルを分けやすいです。

## `vcxproj` 競合の回避

このリポジトリでは、新規 `.cpp` / `.h` の追加時に `DirectXFoundation.vcxproj` 競合が起きやすいです。  
回避策は次の優先順です。

1. 既存ファイル内で収まる小変更は新規ファイルを増やさない
2. feature ごとにまとめてファイル追加する
3. ファイル追加専用 PR を先に入れる
4. 将来的には CMake か wildcard 管理へ移行する

今の Visual Studio プロジェクトを維持するなら、少なくとも「機能追加 PR」と「プロジェクト登録 PR」を分けるだけでかなり楽になります。

## 実装ルール

日々の実装では次をルールにします。

- 新しい状態を足す前に、その状態の所有者を決める
- `GameScene` 本体に bool や float を直接増やさない
- `Update()` に新しい仕様を書かない
- `DrawEntity()` に新しい仕様を書き足し続けない
- `assets` で済む変更はコード化しない
- 生成物、`.exe`、`.pdb`、`.cso` をコミットしない

## このプロジェクトでの現実的な移行順

一気に全面改修する必要はありません。  
順番は次が安全です。

1. `GameScene` のメンバを state struct へ逃がす
2. `HandleWorldInteractions()` を相互作用種別ごとに分ける
3. `DrawEntity()` を対象種別ごとの描画関数へ分ける
4. `game_scene.cpp` の入力分岐を input handling へ寄せる
5. `game_scene_setup.cpp` から stage loader を独立させる

## まず守るべき最小ルール

今すぐ全部直さなくても、次の 5 つを守るだけで conflict はかなり減ります。

- `GameScene` に新しい状態を直置きしない
- ルール変更と見た目変更を同じ PR に入れない
- 敵・ギミックの状態は `world_components.*` に寄せる
- 写真の状態は `photo_components.*` と `photo_system.*` に寄せる
- `DirectXFoundation.vcxproj` を触る変更は単独で通す

この方針なら、今のコードベースを崩しすぎずに並行作業しやすい形へ持っていけます。
