# CtrlHz180

PSP controller plugin for ARK/PRO. / ARK・PRO向けPSP入力プラグイン。

## 日本語

- 標準の入力サンプリング周期（0）を **約180Hz（5555µs）** に変更。ゲームが指定した周期は尊重します。
- ボタンは**押下を即時反映、離した状態は2サンプル連続で確定**（1件取得のRead/Peekのみ）。
- アナログスティック（Lx/Ly）は無加工。平滑化やデッドゾーン補正は行いません。
- ReadBufferの待機動作、タイムスタンプ、複数件取得、Latch APIは変更しません。

### 入力遅延への効果（理論値）

標準設定が約60Hzの場合の比較。**ゲーム全体の入力遅延が下記の数値だけ減る保証ではありません。**

| 項目 | 標準 約60Hz | CtrlHz180 約180Hz | 変化 |
| --- | ---: | ---: | ---: |
| サンプリング間隔 | 16.67ms | 5.56ms | 約11.11ms短縮 |
| 押下検出までの平均待ち時間 | 8.33ms | 2.78ms | 約5.56ms短縮 |
| 押下検出までの最大待ち時間 | 約16.67ms | 約5.56ms | 約11.11ms短縮 |
| 押下時のチャタリング補正待ち | なし | なし | 追加遅延なし |
| 離脱時のチャタリング補正待ち | なし | 2回連続OFFを確認 | 最初のOFF検出から約5.56ms追加 |

※平均値はボタン操作の時刻がサンプリング周期に対して均等に分布する場合。ゲーム側の入力取得・描画周期は変更せず、明示指定されたサンプリング周期も尊重します。体感差と実際の総遅延はゲームごとに異なります。

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

### Input latency (theoretical)

Comparison assuming the default sampling rate is approximately 60Hz. **These are sampling-stage estimates, not guaranteed reductions in total game input lag.**

| Metric | Stock ~60Hz | CtrlHz180 ~180Hz | Difference |
| --- | ---: | ---: | ---: |
| Sampling interval | 16.67ms | 5.56ms | ~11.11ms shorter |
| Average wait to detect a press | 8.33ms | 2.78ms | ~5.56ms shorter |
| Maximum wait to detect a press | ~16.67ms | ~5.56ms | ~11.11ms shorter |
| Extra press debounce delay | None | None | No added delay |
| Release debounce | None | Two consecutive OFF samples | ~5.56ms after the first OFF sample |

*Average values assume button events are uniformly distributed relative to the sampling clock. The plugin does not change game polling or rendering frequency, and respects explicit sampling-cycle requests. Actual total latency and perceived benefit vary by game.*

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
