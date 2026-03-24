# UI担当ガイド

このガイドは、HUD、メニュー、ラベル、画面内情報を触る担当者向けです。

## まず見るファイル

- `scenes/game/game_scene_render_ui.cpp`
- `scenes/game/game_scene_render.cpp`
- `scenes/title_scene.cpp`
- `scenes/result_scene.cpp`
- `rendering/imgui_layer.cpp`

## 基本方針

- UI とルール変更を同時に入れすぎない
- デバッグ UI とプレイヤー向け HUD を分離する
- 色だけでなくラベルや配置でも状態を伝える

## よくある作業

### ゲーム中 HUD を直す

- 位置、色、視認性を `game_scene_render_ui.cpp` で調整
- 撮影中・配置中・通常時で情報優先度を分ける

### タイトルやリザルトを直す

- 各シーン描画で導線を確認
- 入力説明と実装が一致するか確認

## 作業後チェック

- プレイ中に読みやすいか
- 撮影/配置中に必要情報が欠けていないか
- `filter-spec` と表示が矛盾していないか
