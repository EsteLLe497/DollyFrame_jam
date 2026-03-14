# Extension Guide

このドキュメントは、プロジェクトを拡張するときの入口をまとめます。

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
