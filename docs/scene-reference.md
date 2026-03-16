# シーン別リファレンス

各シーンを触る前に、どこを見るべきかをまとめます。

## Title

タイトル画面です。

### まず見る場所

- `title_scene.cpp`
- `title_scene.h`

### よくある作業

- ボタンや開始演出の調整
- 次のシーンへの遷移変更

## Game

実ゲーム部分です。

### まず見る場所

- `game_scene.cpp`
- `game_scene.h`
- `game_scene_gameplay.cpp`
- `game_scene_render.cpp`

### よくある作業

- プレイヤー挙動の調整
- 写真システムの変更
- フィルター仕様の追加
- 敵やギミックの追加
- UI や演出の調整

### 注意

`GameScene` は責務が集中しているので、変更の影響範囲が大きいです。

## Result

リザルト画面です。

### まず見る場所

- `result_scene.cpp`
- `result_scene.h`

### よくある作業

- スコア表示
- 再挑戦導線の調整

## Demo / ShaderShowcase

検証用、見本用のシーンです。

### まず見る場所

- `demo_scene.cpp`
- `shader_showcase_scene.cpp`

### よくある作業

- prefab 動作確認
- シェーダの見た目確認

## SceneManager 周辺

シーン遷移や登録を扱います。

### まず見る場所

- `scene_manager.cpp`
- `scene_registry.cpp`

### よくある作業

- 新シーンの登録
- 起動シーンの切り替え
- 遷移条件の追加

## Application 周辺

ゲーム全体の初期化、メインループ、更新順を持ちます。

### まず見る場所

- `application.cpp`
- `application.h`

### よくある作業

- フレーム更新順の確認
- 入力や描画の全体制御確認

## 迷ったとき

- ゲームプレイ変更なら `GameScene`
- シーン追加なら `scene_registry.cpp`
- 起動やループの不具合なら `Application`
- 描画だけの話なら各 `*_render` か該当シーン
