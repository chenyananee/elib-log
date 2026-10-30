# elib-log

资源受限嵌入式系统用的零动态内存日志库：用户静态提供上下文与格式化缓冲，纯 C99，单头伞 API。

当前版本：**2.0.0**（`ELIB_LOG_VERSION`），单头 `include/elib_log.h`。

## 特性

- **零动态内存 / 零全局**：`ctx` 与格式化缓冲全部由用户静态提供，库自身不持有任何状态
- **单头伞 API**：全部公共类型与函数集中在 `elib_log.h`，无子头
- **一次拼装、一次输出**：整行在用户缓冲内组装完成后再调用 `output`，传输层永远看不到半行
- **绝不越界**：颜色转义与行尾在 `init` 时按固定成本预留，内容只可用剩余空间，永远写不出 `buf_size` 之外
- **截断可观测**：内容放不下返回 `ELIB_LOG_ERR_TRUNCATED`，行尾保留、转义序列绝不被切断
- **等级语义直观**：严重度升序（`DEBUG=0 … ERROR=3`），`level` 是"最低可通过的严重度"
- **颜色是按 ctx 决定**：`ELIB_LOG_FLAG_COLOR` 显式开启，默认完全不出 ANSI 转义
- **行尾可选**：默认 CRLF，可切 LF 或"不加行尾"
- **可选时间戳**：`[秒.毫秒]` 前缀，时钟回调由用户提供
- **可选临界区钩子**：`init` 拒收单边钩子；`output` 在临界区之外执行
- **重入保护**：在 `output` 回调里再次打日志返回 `ELIB_LOG_ERR_BUSY`，不会污染缓冲
- **编译期裁剪**：`ELIB_LOG_COMPILE_LEVEL` 让低等级宏连同参数一起从二进制里消失
- **纯 C99 实现**，C++ 兼容（`extern "C"`）

## 快速开始

### 1. 静态定义缓冲与上下文

```c
#include "elib_log.h"   /* 构建时加 -I<repo>/include */

static char            g_log_buf[256];
static elib_log_ctx_t  g_log_ctx;

static void log_output(const char *str, void *user_data)
{
    (void)user_data;
    /* str 仅在本次调用期间有效：异步发送必须先拷贝
     * 这里把整行交给 UART / RTT / 文件 */
    uart_write_line(str);
}

static elib_log_timestamp_t log_clock(void *user_data)
{
    (void)user_data;
    elib_log_timestamp_t ts;
    ts.s  = g_uptime_ms / 1000u;
    ts.ms = g_uptime_ms % 1000u;
    return ts;
}

static void app_log_init(void)
{
    elib_log_cfg_t cfg = {
        .buf       = g_log_buf,
        .buf_size  = sizeof(g_log_buf),
        .output    = log_output,          /* 必填 */
        .timestamp = log_clock,           /* 可选，可 NULL */
        .user_data = NULL,                /* 透传给两个回调 */
        .level     = ELIB_LOG_INFO,       /* 只输出 INFO / WARNING / ERROR */
        .flags     = ELIB_LOG_FLAG_EOL_CRLF,   /* 见"布局开关" */
    };

    /* cfg 按值拷贝，可以放栈上；缓冲必须长期存在
     * 钩子见"并发模型"，单上下文使用时传 NULL, NULL */
    elib_log_init(&g_log_ctx, &cfg, crit_enter, crit_exit);
}
```

### 2. 打日志

```c
ELIB_LOGI(&g_log_ctx, "Sensor", "temperature is %d.%01d C", t_int, t_frac);
ELIB_LOGW(&g_log_ctx, "Comm",   "retry %u of %u", n, max);
ELIB_LOGE(&g_log_ctx, "Flash",  "erase failed at 0x%08lX", (unsigned long)addr);
ELIB_LOGD(&g_log_ctx, "Loop",   "tick %lu", (unsigned long)tick);

/* 没有便捷宏时直接用原始接口，等级是运行时值 */
elib_log_write(&g_log_ctx, ELIB_LOG_INFO, "Boot", "version %s", fw_version);

/* Hex dump：大写、单空格分隔、无尾随空格 */
ELIB_LOG_HEX(&g_log_ctx, ELIB_LOG_INFO, "Sensor", payload, payload_len);
```

产出（默认 CRLF、未开颜色）：

```
[Sensor]:temperature is 23.6 C
[Comm]:retry 2 of 5
[Sensor]:02 89 A6 FF 5E
```

### 3. 运行时调节等级

```c
elib_log_set_level(&g_log_ctx, ELIB_LOG_DEBUG);    /* 全开 */
elib_log_set_level(&g_log_ctx, ELIB_LOG_ERROR);    /* 只留 ERROR */

elib_log_level_t level;
if (elib_log_get_level(&g_log_ctx, &level) == ELIB_LOG_OK) {
    /* ... */
}
```

### 4. 反初始化

```c
elib_log_deinit(&g_log_ctx);   /* 之后除 init 外所有 API 返回 NOT_INITIALIZED */
```

## API 一览

| 分组 | 函数 | 说明 |
|------|------|------|
| 生命周期 | `elib_log_init(ctx, cfg, enter_critical, exit_critical)` | 初始化（memset 覆盖 ctx；cfg 按值拷贝；非法配置拒收） |
| | `elib_log_deinit(ctx)` | 反初始化（NULL 安全；之后除 init 外全部拒收） |
| 输出 | `elib_log_write(ctx, level, head, fmt, ...)` | 格式化一行并输出（`head` 为 NULL 时省略前缀） |
| | `elib_log_hex(ctx, level, head, data, len)` | Hex dump 一行 |
| 过滤 | `elib_log_set_level(ctx, level)` | 设置最低可通过的严重度 |
| | `elib_log_get_level(ctx, &level)` | 读取当前阈值 |
| 宏 | `ELIB_LOGE / ELIB_LOGW / ELIB_LOGI / ELIB_LOGD(ctx, head, ...)` | 便捷宏，`fmt` 必填（C99 可变参数） |
| | `ELIB_LOG_HEX(ctx, level, head, data, len)` | Hex 便捷宏（等级是运行时值，不做编译期裁剪） |

错误码（5 个，`elib_log_err_t`）：

| 错误码 | 含义 |
|--------|------|
| `ELIB_LOG_OK` | 成功；**也包括被等级过滤掉**（什么都没写） |
| `ELIB_LOG_ERR_INVALID_PARAM` | 空指针、非法等级、非法 flags 组合、缓冲过小 |
| `ELIB_LOG_ERR_NOT_INITIALIZED` | 未 `init`，或已 `deinit` |
| `ELIB_LOG_ERR_TRUNCATED` | 内容放不下，已尽力输出（行尾完整） |
| `ELIB_LOG_ERR_BUSY` | 上一行还在 `output` 回调里，缓冲仍被读取 |

## 输出布局

```
[<ESC>颜色 m][秒.毫秒][HEAD]:正文[<ESC>0m]<行尾>
```

| 片段 | 何时出现 | 说明 |
|------|----------|------|
| 颜色开/闭 | `ELIB_LOG_FLAG_COLOR` | 由 `init` 时的 ctx 决定，**不是** per-call 参数 |
| `[秒.毫秒]` | `ELIB_LOG_FLAG_TIMESTAMP` | 毫秒固定三位补零，例如 `[7.005]`、`[0.000]` |
| `[HEAD]:` | `head != NULL` | `head` 必须是 NUL 结尾字符串；传 `""` 得到 `[]:` |
| 正文 | 总有 | `printf` 风格；`elib_log_hex` 为大写、单空格分隔 |
| 行尾 | 由 EOL flag 决定 | 默认 CRLF |

逐字节样例（`head = "T"`，正文 `hi`）：

| 配置 | 实际字节 |
|------|----------|
| 无 flag | `[T]:hi` `0D 0A` |
| `EOL_LF` | `[T]:hi` `0A` |
| `EOL_NONE` | `[T]:hi` |
| `COLOR` + INFO | `1B 5B 33 32 6D` `[T]:hi` `1B 5B 30 6D` `0D 0A` |
| `COLOR` + ERROR + `EOL_LF` | `1B 5B 33 31 6D` `[T]:hi` `1B 5B 30 6D` `0A` |
| `TIMESTAMP`（7s005ms） | `[7.005][T]:hi` `0D 0A` |

颜色表（仅在 `ELIB_LOG_FLAG_COLOR` 下生效）：

| 等级 | 序列 |
|------|------|
| DEBUG | `ESC[90m`（灰） |
| INFO | `ESC[32m`（绿） |
| WARNING | `ESC[33m`（黄） |
| ERROR | `ESC[31m`（红） |

### 布局开关（`cfg.flags`）

| flag | 作用 |
|------|------|
| `ELIB_LOG_FLAG_COLOR` | 输出 ANSI 颜色；未给则完全不出转义，适合日志文件与非 ANSI 终端 |
| `ELIB_LOG_FLAG_TIMESTAMP` | 输出时间戳前缀，**必须**同时提供 `timestamp` 回调，否则 `init` 拒收 |
| `ELIB_LOG_FLAG_EOL_CRLF` | 行尾 CRLF（也是未给任何 EOL flag 时的默认值） |
| `ELIB_LOG_FLAG_EOL_LF` | 行尾 LF |
| `ELIB_LOG_FLAG_EOL_NONE` | 不加行尾 |

三个 EOL flag **互斥**，同时给两个及以上 `init` 拒收；未知 flag 位同样拒收。

## 等级语义

严重度随数值上升：`DEBUG(0) < INFO(1) < WARNING(2) < ERROR(3)`。`level` 是**最低可通过的严重度**。

| 阈值 | DEBUG | INFO | WARNING | ERROR |
|------|-------|------|---------|-------|
| `ELIB_LOG_DEBUG` | ✅ | ✅ | ✅ | ✅ |
| `ELIB_LOG_INFO`（推荐默认） | — | ✅ | ✅ | ✅ |
| `ELIB_LOG_WARNING` | — | — | ✅ | ✅ |
| `ELIB_LOG_ERROR` | — | — | — | ✅ |

- 阈值为 `X` 时，等级 **等于** `X` 的行照常输出
- 被过滤的行返回 `ELIB_LOG_OK`，**不格式化、不加锁、不碰缓冲、不调时钟**
- 等级越界（`(elib_log_level_t)9`、`-1`）是 `ELIB_LOG_ERR_INVALID_PARAM`，不会被当成"被过滤"

### 编译期裁剪

```c
#define ELIB_LOG_COMPILE_LEVEL ELIB_LOG_LEVEL_WARNING   /* 或 2 */
#include "elib_log.h"
```

`ELIB_LOGD` / `ELIB_LOGI` 展开为 `((void)0)`，**连参数都不求值**，适合把调试表达式彻底移出产物：

```c
ELIB_LOGD(&ctx, "Loop", "state=%s cost=%lu", state_name(s), measure_cost());
/* 低于编译阈值时，state_name 与 measure_cost 都不会被调用 */
```

默认值为 `ELIB_LOG_LEVEL_DEBUG`（全开）。编译期裁剪与运行时阈值可叠加使用。

## 截断

固定成本（颜色开/闭 + 行尾 + NUL）在 `init` 时就被预留，内容只能用剩下的空间：

```
可用正文空间 = buf_size - (COLOR ? 9 : 0) - 行尾字节数 - 1
```

因此：

- 截断**只可能砍内容**，颜色转义序列绝不会被切成半个（不会留下孤立的 `ESC`）
- 行尾与 NUL 一定完整，`strlen(str) <= buf_size - 1`
- 返回 `ELIB_LOG_ERR_TRUNCATED`，调用方可以据此降级（例如加大缓冲、缩短 `head`）
- 恰好放满返回 `ELIB_LOG_OK`（等于可用空间就不算截断）
- `init` 会拒收连"固定成本 + 1 个内容字节"都装不下的缓冲：

| 配置 | 最小可用缓冲 |
|------|--------------|
| 无色 + CRLF | 8（`ELIB_LOG_MIN_BUF_SIZE` 也正好是 8） |
| 无色 + LF | 8 |
| 无色 + 无行尾 | 8 |
| 有色 + CRLF | 13 |
| 有色 + LF | 12 |
| 有色 + 无行尾 | 11 |

Hex dump 也走同一套规则：空间用尽即停，返回 `TRUNCATED`，不会读到 `data` 之外。

## 并发模型（ISR + 主循环）

格式化缓冲是共享的，所以整行拼装在 **可选临界区钩子** 内完成，`output` 回调在**临界区之外**执行：

```c
/* 例：Cortex-M 下用 PRIMASK 保存/恢复 */
static uint32_t g_crit_save;

static void crit_enter(void)
{
    g_crit_save = __get_PRIMASK();
    __disable_irq();
}

static void crit_exit(void)
{
    __set_PRIMASK(g_crit_save);
}

elib_log_init(&g_log_ctx, &cfg, crit_enter, crit_exit);
```

| 规则 | 说明 |
|------|------|
| 钩子必须成对 | 只给一个时 `init` 拒收（`INVALID_PARAM`） |
| 钩子应可嵌套 | 用保存/恢复式中断屏蔽，而不是无条件开中断 |
| 时钟回调在临界区内 | 它属于拼装的一部分，必须短小、非阻塞 |
| `output` 在临界区外 | 可以在里面做较慢的搬运，但不能重入日志 |
| 重入 → `BUSY` | 在 `output` 里再打日志、或另一上下文恰好在输出时打日志，都返回 `BUSY`，该行被丢弃 |
| `set_level` / `get_level` 随时可用 | 它们不碰缓冲，因此在 `output` 回调内也安全 |
| 单上下文时传 `NULL, NULL` | 声明"不存在并发"，此时无任何额外开销 |

> **ISR 打日志的正确姿势**：把 `BUSY` 当成"这一行丢了"而不是"等一等"。日志绝不能阻塞中断；若必须保证不丢，就让 ISR 只置标志，由主循环补打。

## 回调约定

```c
typedef void (*elib_log_output_fn)(const char *str, void *user_data);
typedef elib_log_timestamp_t (*elib_log_timestamp_fn)(void *user_data);
```

- `str` 指向**库内部复用的用户缓冲**，只在本次调用期间有效：需要异步发送必须先拷贝
- `user_data` 由 `init` 的 cfg 透传，两个回调拿到同一个指针
- `output` 必填，`timestamp` 可选；给了 `timestamp` 但没给 `TIMESTAMP` flag 时它不会被调用
- 每行只调用一次 `output`，也只读一次 `timestamp`

## 生命周期与内存

- `init` **无条件** memset `ctx`：任意历史状态都能重初始化，`busy` 也会被清掉
- `cfg` 是**值结构**：逐字段拷贝，不保存指针，因此 `cfg` 可以放栈上
- `cfg.buf` 指向的缓冲必须**长期存在**（寿命 ≥ `ctx` 的使用期）；库不会替它清零
- `head` 必须是 NUL 结尾的字符串，且寿命覆盖该次调用；库不做长度校验
- `deinit` 只清 `initialized`（并顺手清 `busy`），配置与缓冲留在原处但不再可用
- 重新 `init` 会整体替换 flags / level / 回调 / 钩子

## 关于 `%f` 与"零动态内存"

库本身不做任何分配，但正文由 `vsnprintf()` 产生。在 newlib 上，浮点转换（`%f`）走 `_dtoa_r()` →
`_Balloc()` → `_malloc_r()`，**会发生堆分配**；整数、字符、字符串转换则不分配。

若应用必须严格零分配（例如禁止堆的 MCU 工程），**不要向本库传浮点**，先自行投影成整数或字符串：

```c
/* 不要：ELIB_LOGI(&ctx, "T", "%.3f", value); */
int32_t milli = (int32_t)(value * 1000.0f);
ELIB_LOGI(&ctx, "T", "%ld.%03ld", (long)(milli / 1000), (long)(milli % 1000));
```

## 编译

将 `include/` 加入头文件搜索路径，源文件编译为普通 C99 目标文件：

```bash
arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude \
    -c src/elib_log_core.c -o elib_log_core.o
```

源文件直连伞头（`#include "elib_log.h"`），不依赖任何相对路径，无随库构建系统。

## 测试

`test/` 下 11 个独立测试文件（共 112 个用例），每件自带 `main()`，可分别编译运行：

```bash
for t in test/test_*.c; do
    gcc -std=c99 -Wall -Wextra -Werror -Wpedantic -Iinclude \
        "$t" src/elib_log_core.c -o "${t%.c}.exe"
    "./${t%.c}.exe"
done
```

| 文件 | 覆盖 |
|------|------|
| `test_init.c` | 版本宏、cfg 逐字段拷贝、栈上 cfg 可销毁、各类非法配置拒收（含固定成本不足、EOL 冲突、未知 flag、时间戳缺时钟）、脏 ctx 重初始化、写操作不越过 `buf_size` 的哨兵检查 |
| `test_level.c` | 4×4 过滤矩阵、被过滤行零副作用、阈值本身通过、hex 同样过滤、非法等级报错而非静默丢弃、宏走同一套过滤 |
| `test_write_format.c` | `head` 三种形态、printf 各转换、每次调用一行、空格式、NUL 终止 |
| `test_write_flags.c` | 默认 CRLF / LF / NONE、逐等级颜色与 reset、时间戳补零与 uint32 全范围、全 flag 组合、无 flag 时零转义字节、未给 flag 时不读时钟、hex 同布局 |
| `test_truncation.c` | 各 flag 组合的截断点、行尾保留、转义不被切断、恰满 vs 超一字节、最小有色缓冲、固定成本不足被拒收、反复截断始终在界内 |
| `test_hex.c` | 文档样例 `02 89 A6 FF 5E`、大写与单空格、单字节、零长度、`data` 为 NULL 但 `len>0`、`len` 超过 65535、每次 dump 一次输出 |
| `test_callbacks.c` | 两个回调的 `user_data`、时钟先于输出、每行读一次时钟、过滤行不读时钟、`output` 收到用户缓冲、两个 ctx 互不干扰 |
| `test_critical.c` | 钩子成对存储、write/hex/截断行的 enter/exit 配平、`output` 在临界区外、时钟在临界区内、过滤行不加锁 |
| `test_reentrancy.c` | 回调内再打日志返回 `BUSY`、外层行不被破坏、之后守卫释放、回调内可查等级、过滤先于守卫（返回 OK） |
| `test_level_api.c` | set/get 往返、越界拒收且保留旧值、`ELIB_LOG_COMPILE_LEVEL = WARNING` 时 DEBUG/INFO 连同参数一起被编译掉、运行时阈值叠加 |
| `test_lifecycle.c` | `deinit` NULL 安全、清 `busy`、死后全部拒收、初始化检查先于过滤、重新 init 复活并替换配置 |

行为一致性另用独立 Python 参考模型交叉验证：**222409** 项断言，包含

- C 测试里逐条断言的精确行内容（`strcmp` 的期望值独立重算）
- 两套独立拼装算法（字符串切片 vs 逐字符有界写入器）在 `buf_size 8..40 × 10 组 flags × 4 等级 × 4 种 head × 6 种正文长度` 上的全量对拍
- 不变量：`len(line) <= buf_size - 1`、截断行必定填满、非截断行正文完整、颜色转义始终成对完整、无色时零 `ESC` 字节
- `init` 接受边界与过滤矩阵

结果 `TOTAL FAILS: 0`。

## 许可证

MIT License（见 `LICENSE`）。
