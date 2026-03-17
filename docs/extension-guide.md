# Extension Guide

このドキュメントは、プロジェクトを拡張するときの入口をまとめます。

## 関連ドキュメント

- `docs/programming-guide.md`
- `docs/team-programming-guide.md`

## 新しいシーンを追加する

### 手順

1. `Scene` を継承したクラスを作る
2. `GetSceneId()` を実装する
3. `OnEnter()`、`Update()`、`Draw()`、`DrawDebugUI()` を実装する
4. 必要なら `GetEventBus()` をオーバーライドする
5. `Application` 初期化時に `SceneRegistry::Register()` へ登録する

### 最低限必要な実装

```cpp
class TitleScene final : public Scene
{
public:
    const char* GetSceneId() const override { return "title"; }
    void OnEnter(ResourceManager& resources) override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DrawDebugUI() override;
};
```

### シーン切り替え

切り替え方法は 2 つです。

- `SceneChangeRequested` を発行する
- `SceneManager::SetScene()` を直接呼ぶ

基盤としては前者を優先してください。  
イベント駆動の境界が維持しやすくなります。

## 新しいコンポーネントを追加する

### 手順

1. `Component` 継承クラスを追加する
2. 必要なメンバを持たせる
3. `Update()`、`Draw()`、`DrawDebugUI()`、`OnAttach()` を必要に応じて実装する
4. `Entity::AddComponent<T>()` で組み込む

### 向いている処理

- 移動
- アニメーション
- AI
- 弾生成
- HP 管理
- UI 追従

### 向いていない処理

- 全シーン共通のミドルウェア初期化
- グローバルなシーン切り替え制御
- DirectX デバイス生成

それらは `Application` や専用マネージャへ置くべきです。

### 今のプロジェクトでよく足されるもの

- フィルター反応用の状態
- 敵専用状態
- ギミック専用状態
- 写真コピー専用状態

これらは `components.h` にまとまっているので、追加前に既存コンポーネントの責務と重複していないか見てください。

## 新しいイベントを追加する

### 手順

1. `event_bus.h` の `EventType` に追加する
2. 必要なら `Event` 構造体の payload を見直す
3. 発行側を追加する
4. `Application::ProcessSceneEvents()` など消費側を追加する

### 追加例

- `SpawnEntityRequested`
- `OpenDialogRequested`
- `DamageApplied`
- `SetMusicState`

### 注意

イベント種別が増えてきたら、`name` 文字列頼みではなく ID 化や専用 payload への移行を検討してください。

## 新しいプレハブを追加する

### 手順

1. `assets/manifest.json` に必要なテクスチャキーを追加する
2. `assets/prefabs.json` に新しいプレハブを定義する
3. シーン側で `PrefabFactory::Create("your_prefab")` を呼ぶ

### 例

- `enemy_basic`
- `pickup_coin`
- `trigger_goal`

### 注意

今の `GameScene` は prefab と直書き配置が混在しています。  
新しいオブジェクトを追加するときは、

- `prefab` で増やすのか
- `game_scene.cpp` へ直書きするのか

を最初に決めた方が混乱しません。

短期的な試作なら直書きでもよいですが、量産したいなら prefab 化を優先してください。

## 新しいフィルターを追加する

手順:

1. `components.h` の `PhotoFilterTheme` に追加する
2. `photo_filter_rules.cpp` のテーマ名表示とサイクル順を更新する
3. `game_scene.cpp` の入力切り替えを更新する
4. `photo_filter_rules.cpp` の
   - `ApplyPhotoFilterToPhotoBox()`
   - `ApplyPhotoFilterToCapturedTarget()`
   を更新する
5. `game_scene_render.cpp` の
   - UI
   - オーバーレイ
   - プレビュー
   - 描画エフェクト
   を更新する

注意:

- フィルターはコピーだけでなく元オブジェクトにも作用します
- 片方だけ更新すると仕様が半分壊れます

## 新しい敵を追加する

手順:

1. `EnemyComponent` で足りるか確認する
2. 足りなければ専用コンポーネントを追加する
3. まず `game_scene.cpp` に 1 体だけ置く
4. `Cold` / `Invert` / `Sepia` への反応を確認する

今の敵はフィルターの影響をかなり受けるので、  
「通常挙動だけ確認して終わり」にしない方が安全です。

## Lua API を増やす

### 手順

1. `script_engine.cpp` の `BindEventBus()` に関数を追加する
2. スクリプト側の利用例を `assets/demo_scene.lua` へ追加する
3. README と `docs/data-formats.md` を更新する

### 方針

Lua には高水準 API だけを公開してください。  
`DirectX` や `Box2D` の生 API を直接出すと基盤の境界が壊れます。

良い例:

- `request_sound("enemy_hit")`
- `request_scene_change("title")`

悪い例:

- `create_dx11_buffer(...)`
- `b2Body_SetTransform(...)`

## 新しい音を追加する

現在の音は `audio.cpp` の簡易トーン生成です。  
効果音を増やす場合は以下のどちらかです。

- 既存方式でキュー名を追加する
- ファイルベース再生へ発展させる

本格運用するなら、`Audio_PlayCue()` の裏側を WAV/OGG ロードへ置き換える方がよいです。

## 拡張時の優先ルール

この基盤では以下を優先してください。

1. ハードコードを増やしすぎない
2. まずイベントでつなぐ
3. 量産対象は JSON か Lua へ逃がす
4. `Application` にゲーム固有ロジックを入れない
5. `third_party` は極力直接触らない
6. 生成物や依存バイナリを安易にコミットしない
