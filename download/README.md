
スクリプトで bit ファイルをダウンロードする

## パスの追加

```bash
cd download
echo "export PATH=\$PATH:`pwd`" >> ~/.zshrc  #zsh を使っている場合はこの行を実行; ohmyzsh を強く推奨！ YYDS
echo "export PATH=\$PATH:`pwd`" >> ~/.bashrc #bash を使っている場合はこの行を実行
```



## bit ファイルのダウンロード

bit ファイルがあるディレクトリで次を実行：

```
downloadbit.sh ./xxxxxxxxxxx.bit
```


## 2. bin ファイルのダウンロード

```bash
downloadbin.sh ./xxxxxxxxxxx.bit

#NOTES: 内部で bit ファイルを適切な bin ファイルに自動変換してから、bin をダウンロードします;
```


enjoy !

