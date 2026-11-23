# CtrlHz180

PSP controller plugin for ARK/PRO. / ARK・PRO向けPSP入力プラグイン。

## 日本語

- 標準の入力サンプリング周期（0）を **約180Hz（5555µs）** に変更。ゲームが指定した周期は尊重します。
- ボタンは**押下を即時反映、離した状態は2サンプル連続で確定**（1件取得のRead/Peekのみ）。
- アナログスティック（Lx/Ly）は無加工。平滑化やデッドゾーン補正は行いません。
- ReadBufferの待機動作、タイムスタンプ、複数件取得、Latch APIは変更しません。

**ビルド**（PSPDEVとCFW用systemctrlライブラリが必要）：

```sh
cd source
make clean && make
```

生成物：`source/ctrlhz180.prx`

**PCテスト**：

```sh
bash tests/run_tests.sh
```

PRXを `ms0:/seplugins/` に配置し、GAME用として有効化してください。ほかの入力サンプリング変更PRXとは併用しないでください。

注意：OFFが1サンプルだけの高速連打では、離した動作が消える場合があります。PSP実機での検証が必要です。

## English

- Changes the default sampling cycle (0) to **~180Hz (5555µs)**. Explicit game-requested cycles are respected.
- Buttons: **immediate press; release after two consecutive OFF samples** (single-item Read/Peek only).
- Analog stick (Lx/Ly): unchanged; no smoothing or dead-zone adjustment.
- ReadBuffer waiting, timestamps, multi-item reads, and latch APIs are unchanged.

**Build** (requires PSPDEV and CFW systemctrl libraries):

```sh
cd source
make clean && make
```

Output: `source/ctrlhz180.prx`

**Host tests**:

```sh
bash tests/run_tests.sh
```

Place the PRX in `ms0:/seplugins/` and enable it for GAME. Do not combine it with another controller-sampling PRX.

Note: a single OFF sample between presses can be filtered out. PSP hardware testing is still required.
