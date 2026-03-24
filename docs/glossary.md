# 用語集

このプロジェクトで頻繁に使う言葉をまとめます。

## ゲーム全体

### Scene

画面や状態の単位です。`Title`、`Game`、`Result` などがあります。

### GameScene

メインゲームプレイのシーンです。現在は各ドメインを呼び出すオーケストレータとして運用します。

### tileSize

グリッドの基本サイズです。配置やサイズ決めの基準になります。

## 写真システム

### Capture

写真を撮る処理です。`photo_capture_system.*` が担当します。

### Paste

写真を配置する処理です。`photo_paste_system.*` が担当します。

### Shared

Capture/Paste 共通の変換・生成ルールです。`photo_shared.*` が担当します。

### Filter Theme

写真に与える現象の種類です。現在は `None`、`Hot`、`Cold`、`Invert`、`Sepia` があります。

## 敵・ギミック

### EnemyComponent

敵の種類や有効状態などを持つコンポーネントです。

### EnemyMoverComponent

敵の移動、停止、凍結、巻き戻しに関わるコンポーネントです。

### GimmickComponent

ギミックの種類、有効状態、単発利用かどうかを持つコンポーネントです。

### Prefab

再利用するためのオブジェクト定義です。`assets/prefabs.json` から生成します。

## 運用

### Artifact

ビルド生成物やログなど、ソースコードではない派生ファイルです。`.lib`、`.pdb`、`foundation.log` などが該当します。

### dxlib_support_libs

このプロジェクトのビルドに必要な補助ライブラリ群です。無いとリンクエラーになります。
