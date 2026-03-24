# Rendering Effects Guide

このガイドは、レンダリングと演出を触る担当者向けです。

## まず見るファイル

- `rendering/shader.h`
- `rendering/shader.cpp`
- `rendering/sprite.h`
- `rendering/sprite.cpp`
- `scenes/game/game_scene_render.cpp`
- `scenes/game/game_scene_render_ui.cpp`

## 基本方針

- ルール変更と演出変更を分離する
- フィルターの見た目は `photo_filter_rules.*` と整合させる
- UI 読みやすさを最優先にする

## よくある作業

### 画面エフェクトの調整

1. 使用箇所を `game_scene_render.cpp` で確認
2. `rendering/shader.cpp` の適用順を確認
3. 実機プレイで可読性を確認

### プレビューやオーバーレイの調整

1. `game_scene_render_ui.cpp` で描画位置と色を確認
2. フィルター名と色の一致を確認
3. スロー中・撮影中・配置中の視認性を確認

## 作業後チェック

- 既存 UI が埋もれていないか
- プレイ中に情報が読めるか
- パフォーマンス劣化がないか
