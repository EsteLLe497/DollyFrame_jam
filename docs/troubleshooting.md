# トラブルシュート

新しく参加した人や、久しぶりに触る人が詰まりやすい問題をまとめます。

## ビルドが通らない

### `DxLib_vs2015_x64_MTd.lib` が見つからない

`dxlib_support_libs` が足りていない可能性が高いです。

- `dxlib_support_libs` が作業ツリーにあるか確認する
- `.vcxproj` の追加ライブラリ設定がその配置と一致しているか確認する
- 詳しくは `docs/build-setup-guide.md` を見る

### 新しい `.cpp` を追加したのに反映されない

Visual Studio プロジェクトにファイル登録が漏れている可能性があります。

- `DirectXFoundation.vcxproj` に `ClCompile` / `ClInclude` の追加があるか確認する
- Solution Explorer に表示されているか確認する
- 詳しくは `docs/git-and-artifacts-guide.md` を見る

## 入力が効かない

### キーを押しても反応しない

`core/input.cpp` のアクション対応に入っていない可能性があります。

- ゲーム側だけでなく、入力層のキー変換も確認する
- `InputAction` / `InputAxis` に追加したか確認する
- 実際に押して反応確認する

## 追加したものが見えない

### オブジェクトを置いたのに見えない

次を確認します。

- 座標がグリッド基準で極端にずれていないか
- サイズが `0` や極小になっていないか
- 描画レイヤや tint が背景に埋もれていないか
- `enabled` が `false` になっていないか

## フィルターが思った通りに動かない

### UI の表示だけ変わっていて挙動が変わらない

表示だけ更新して、ルール本体を触っていない可能性があります。

- 表示: `scenes/game/game_scene_render_ui.cpp`
- 効果本体: `gameplay/photo_filter_rules.cpp`

### コピー側にしか効かない

フィルターは

- 元オブジェクトへの効果
- 貼り付けコピーへの効果

の両方を持てます。片方だけ変えていないか確認します。

## 敵やギミックが変な状態になる

### 倒した敵が戻らない

`EnemyComponent` だけでなく `EnemyMoverComponent` の凍結や巻き戻し状態も確認します。

### 接触ダメージが不安定

接触ダメージ判定は複数箇所にまたがります。

- `scenes/game/game_scene_world_interaction_system.h`
- `scenes/game/game_scene_enemy_domain.cpp`
- `scenes/game/game_scene_gimmick_domain.cpp`

共通化したい場合は共有ヘルパへ寄せる方針にします。

## ドキュメントが実装と違う

このプロジェクトは `GameScene` まわりの改修が多いです。

- 実装を正として読む
- 変更したら関係する `docs/` も一緒に更新する
- 迷ったら `docs/programming-guide.md` と `docs/domain-ownership-guide.md` を先に見る
