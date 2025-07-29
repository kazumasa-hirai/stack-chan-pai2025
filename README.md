# stack-chan-pai2025

日本語 | [English](README_en.md)

M5Stack CoreS3を組み込んだｽﾀｯｸﾁｬﾝ用の、M5Stack CoreS3に内蔵されたカメラを用いて、顔追従を行うプログラムです。<br>


# 対応キット
 Takao AkakiさんがBOOTHで頒布している [ｽﾀｯｸﾁｬﾝ M5GoBottom版 組み立てキット](https://mongonta.booth.pm/)とM5Stack CoreS3とSG90を組み合わせて組み立てたｽﾀｯｸﾁｬﾝでのみ動作を確認しています。


# サーボのオフセット調整
SG90系のPWMサーボは個体差が多く、90°を指定しても少しずれる場合があります。その場合は下記のライブラリで設定されている各角度を調整してください。
参考として、本プログラムの作者が組み立てた個体は、以下の値に設定しました。
- _servo[AXIS_X].start_degree = 80;
- _servo[AXIS_Y].start_degree = 105;
- _servo[AXIS_Y].lower_limit = 65;
- _servo[AXIS_Y].upper_limit = 105;

- .pio/libdeps/m5stack-cores3/stackchan-arduino/src/Stackchan_system_config.cpp
上記ライブラリはPlatformIOによって追加されます。またPlatformIOによってライブラリが更新される度に設定がデフォルトの値に上書きされてしまうため、注意してください。


# 使い方
* ボタンA： X軸、Y軸のサーボを90°にします。固定前に90°にするときに使用してください。（90°の位置はPWMサーボによって若干異なるためオフセット値の調整値を調べる必要があります。）
* ボタンB： 顔を初期位置に向けた後、顔追従を開始します。この状態でボタンAを押すと顔追従を終了します。<br>ダブルクリックすると、Groveの5V(ExtPower)出力のON/OFFを切り替えます。Stack-chan_Takao_Baseの後ろから給電をチェックする場合OFFにします。
* ボタンC： ランダムで動きます。
    * ボタンC: ランダムモードの停止
* ボタンAの長押し：オフセットを調整して調べるモードに入ります。
    * ボタンA: オフセットを減らす
    * ボタンB: X軸とY軸の切り替え
    * ボタンC: オフセットを増やす
    * ボタンB長押し: 調整モードの終了


## CoreS3のボタン操作
CoreS3はCore2のBtnA、B、Cの部分がカメラやマイクに変わってしまったためボタンの扱いが変わりました。<br>
画面最下部を縦に3分割して左：BtnA、中央：BtnB、右：BtnCとなっています。

# 必要なライブラリ（動作確認済バージョン）
※ 最新の情報はplatformio.iniを確認してください。最新で動かない場合はライブラリのバージョンを合わせてみてください。
- [M5Unified](https://github.com/m5stack/M5Unified) v0.1.6で確認
- [M5Stack-Avatar](https://github.com/meganetaaan/m5stack-avatar) v0.8.2で確認<br>v0.7.4以前はM5Unifiedに対応していないのでビルド時にM5の二重定義エラーが出ます。
- [ServoEasing](https://github.com/ArminJo/ServoEasing) v3.1.0で確認
- [ESP32Servo](https://github.com/madhephaestus/ESP32Servo) v0.13.0で確認<br>CoreS3はv0.13.0じゃないと動かない場合があります。
- [esp32-camera](https://github.com/espressif/esp32-camera)



# ビルド方法
　PlatformIOでのビルドのみ動作を確認しています。

# ｽﾀｯｸﾁｬﾝについて
ｽﾀｯｸﾁｬﾝは[ししかわさん](https://github.com/stack-chan)が公開しているオープンソースのプロジェクトです。

https://github.com/stack-chan/stack-chan

# 参照元について
本プログラムは以下のプログラムをベースとして作成しています。
- [stack-chan-tester](https://github.com/mongonta0716/stack-chan-tester)
- [M5CoreS3_FaceDetect](https://github.com/ronron-gh/M5CoreS3_FaceDetect)

# author
 Kazumasa Hirai

# LICENSE
 MIT
 LGPL
