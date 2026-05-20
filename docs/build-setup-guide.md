# Build Setup Guide

このドキュメントは、このプロジェクトをローカルでビルドして起動するための手順です。  
チームメンバーが「まず動かす」ための説明書として使ってください。

## 1. 必要環境

- Windows
- Visual Studio 2022
- MSVC v143
- Windows SDK
- Git

## 2. 使うプロジェクト

メイン:

- `DirectXFoundation.vcxproj`

調整ツール:

- `TuningTool.vcxproj`

## 3. 基本のビルド手順

### Visual Studio

1. `DirectXFoundation.sln` を開く
2. `Debug | x64` を選ぶ
3. ビルドする
4. 実行する

### コマンドライン

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "D:\DollyFrame_jam\DirectXFoundation.sln" /p:Configuration=Debug /p:Platform=x64 /m
```

出力:

- `build/Debug/DirectXFoundation.exe`

## 4. よくあるリンクエラー

### `DxLib_vs2015_x64_MTd.lib` が見つからない

原因:

- `dxlib_support_libs/` が無い
- ライブラリが壊れている
- ブランチ切り替えで消えた

確認場所:

- `dxlib_support_libs/Debug/DxLib_vs2015_x64_MTd.lib`
- `dxlib_support_libs/Release/DxLib_vs2015_x64_MT.lib`

この 2 つが無ければ、まずそこを疑ってください。

## 5. 現在の実行前提

このプロジェクトは今、実質 `DxLib` ベースです。  
README の古い説明だけを見ると `DirectX 11` 単体基盤に見える箇所がありますが、実装の実態は違います。

ビルド時に重要なのは次です。

- `third_party/dxlib/include`
- `dxlib_support_libs/Debug`
- `dxlib_support_libs/Release`

## 6. レンダリング資産について

シェーダの調整は `rendering/shader.cpp` と `scenes/game/game_scene_render*.cpp` を中心に行います。  
ビルド成果物ではなく、ソース差分をレビュー対象にします。

## 7. 初回ビルドで見る場所

うまく動かないときは次を見てください。

- `DirectXFoundation.vcxproj`
- `DirectXFoundation.vcxproj.filters`
- `dxlib_support_libs/`
- `third_party/`
- `assets/`

## 8. 実行できるかの最低確認

最低限の確認はこれです。

1. ビルド成功
2. `build/Debug/DirectXFoundation.exe` が生成される
3. 起動してクラッシュしない

## 9. デバッグ時の見方

よく見るもの:

- `ImGui` の `Game Scene`
- `foundation.log`

ただし `foundation.log` は実行ログなので、通常は Git に入れません。

## 10. チーム向けの注意

- 生成物をビルド通過確認のたびにコミットしない
- `dxlib_support_libs/` を動かしたらチームへ共有する
- `.vcxproj` の `ClCompile` 登録と実ファイルの整合を確認する

## 11. 推奨確認コマンド

### 作業前

```powershell
git status --short
```

### ビルド

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "D:\DollyFrame_jam\DirectXFoundation.sln" /p:Configuration=Debug /p:Platform=x64 /m
```

### 生成物確認

```powershell
Test-Path D:\DollyFrame_jam\build\Debug\DirectXFoundation.exe
```

## 12. 追加で読むとよいもの

- `docs/programming-guide.md`
- `docs/team-programming-guide.md`
- `docs/git-and-artifacts-guide.md`


