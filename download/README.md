
スクリプトで bit ファイルを FPGA／Flash に書き込みます

## 1. パスの追加

```bash
cd download
echo "export PATH=\$PATH:`pwd`" >> ~/.zshrc  #zsh を使っている場合はこの行を実行; ohmyzsh を強く推奨！
echo "export PATH=\$PATH:`pwd`" >> ~/.bashrc #bash を使っている場合はこの行を実行
```



## 2. bit ファイルを FPGA に書き込む

bit ファイルがあるディレクトリで次を実行します：

```
downloadbit.sh ./xxxxxxxxxxx.bit
```


## 3. bin ファイルを Flash に書き込む

```bash
downloadbin.sh ./xxxxxxxxxxx.bit

#NOTES: 内部で bit ファイルを適切な bin ファイルに自動変換してから、bin を Flash に書き込みます;
```


enjoy !

