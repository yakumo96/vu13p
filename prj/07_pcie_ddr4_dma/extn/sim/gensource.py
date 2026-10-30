#!/usr/bin/python3
# -*- coding: utf-8 -*-
#========================================================================
#        author   : masw
#        creattime: Sun 19 Mar 2023 09:41:08 PM CST
#========================================================================
#---Model: GPT-4
#---
#---python で 2 つの正弦波を足し合わせた信号を生成し、高周波成分を除去する 33 係数の FIR フィルタを設計して
#---Python で 2 つの正弦波を足し合わせた信号を生成し、高周波信号を除去する 33 係数の FIR フィルタを設計するには、以下のライブラリとコードを使用できます：
#---
#---必要なライブラリのインストール：
#---bash
#---pip install numpy scipy matplotlib
#---Python コードの作成：



import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import firwin, lfilter
def save_txt(signal, filename, scale_factor=(32768/2)):
    # 信号を正規化
    norm_signal = signal / np.max(np.abs(signal))

    # 指定されたスケール係数で信号をスケーリング
    scaled_signal = np.round(norm_signal * scale_factor)

    # テキストファイルとして保存
    np.savetxt(filename, scaled_signal, fmt='%d')


# パラメータ設定
fs = 1000  # サンプリング周波数
duration = 100  # 信号の継続時間 (秒)
f1 = 5  # 1 つ目の正弦波の周波数 (Hz)
f2 = 100  # 2 つ目の正弦波の周波数 (Hz)
num_taps = 33  # FIR フィルタの係数の数
cutoff_freq = 30  # フィルタのカットオフ周波数 (Hz)

# Noto フォントをインストール済みであれば、次のコマンドを使用できます
plt.rcParams['font.family'] = 'Noto Sans CJK JP'

# あるいは、別のフォントを使用している場合は 'font_name' を目的のフォント名に置き換えてください
# plt.rcParams['font.family'] = 'font_name'

# 時系列を生成
t = np.linspace(0, duration, int(fs * duration), endpoint=False)

# 正弦波信号を生成
sin_wave_1 = np.sin(2 * np.pi * f1 * t)
sin_wave_2 = np.sin(2 * np.pi * f2 * t)
combined_signal = sin_wave_1 + sin_wave_2
# 合成信号をテキストファイルとして保存
save_txt(combined_signal, "combined_signal.txt", 6000)

# FIR フィルタを設計
fir_coefficients = firwin(num_taps, cutoff_freq, fs=fs, pass_zero=True, window="hamming")
print(fir_coefficients )
# 合成信号をテキストファイルとして保存
save_txt(fir_coefficients, "fir_coefficients.txt")

# フィルタを適用
filtered_signal = lfilter(fir_coefficients, 1.0, combined_signal)

# 元の信号とフィルタ後の信号をプロット
plt.figure()
plt.subplot(2, 1, 1)
plt.plot(t, combined_signal)
plt.title("原始信号")
plt.xlabel("时间 (秒)")
plt.ylabel("幅度")

plt.subplot(2, 1, 2)
plt.plot(t, filtered_signal)
plt.title("滤波后的信号")
plt.xlabel("时间 (秒)")
plt.ylabel("幅度")

plt.tight_layout()
plt.show()

