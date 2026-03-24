# Git And Artifacts Guide

このドキュメントは、Git 運用と生成物の扱い方をまとめたものです。

## 1. 基本方針

- ソースコードをコミットする
- 生成物はできるだけコミットしない
- 依存バイナリは必要性を確認してから扱う

## 2. 競合しやすいもの

- `.lib`
- `.pdb`
- `.exe`
- `foundation.log`
- `imgui.ini`

理由:

- バイナリで差分レビューしにくい
- 自動マージできない
- 環境差で中身が変わりやすい

## 3. 原則コミットしないもの

- `build/`
- `*.pdb`
- `*.exe`
- `foundation.log`
- `imgui.ini`

## 4. 例外扱い

### `dxlib_support_libs/`

このプロジェクトは現状、`dxlib_support_libs/` が無いとリンクエラーになります。  
更新時は次を確認してください。

- 本当に必要な更新か
- 全員のビルドを助ける変更か
- 単なるローカル復旧ではないか

## 5. マージで起こりやすい事故

### `.vcxproj` と実ファイルの不整合

例:

- `.vcxproj` に `*.cpp` 登録がある
- 実ファイルが Git に含まれていない

対処:

1. `git status --short` で未追跡ファイルを確認
2. `DirectXFoundation.vcxproj` の `ClCompile Include=` と実ファイルを突き合わせる
3. 必要ファイルを追加コミットする

## 6. コミット前チェック

- `git status --short`
- `git diff --stat`

確認ポイント:

- `foundation.log` が混ざっていないか
- `build/` が混ざっていないか
- `.vcxproj` に登録した新規ソースを入れ忘れていないか
- `dxlib_support_libs` を本当に含めるのか

## 7. push 前チェック

- ビルドが通るか
- 関係ない生成物を含んでいないか
- 競合しやすいファイルを不用意に更新していないか

## 8. 共有した方がいい変更

- `DirectXFoundation.vcxproj` の変更
- `dxlib_support_libs/` の変更
- `.vcxproj` 変更と新規ソース追加のセット変更
- `.gitignore` の変更

## 9. 事故を減らす運用

- 1 PR 1 目的
- 生成物を PR に入れない
- merge 前に `master` を取り込む
- docs 更新を同じ PR で行う
