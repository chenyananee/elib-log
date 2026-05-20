# elib-log

零动态内存分配嵌入式日志库

## 特性

- 零动态内存分配，框架内无局部数组
- 单次 output 回调调用，全量拼装后一次输出
- 可选时间戳（ms+s 组合接口）
- 四级日志：ERROR / WARNING / INFO / DEBUG
- 内置 ANSI 颜色着色（红/黄/绿/灰）
- 运行时等级过滤，低于阈值零开销跳过
- printf 风格格式化输出
- Hex dump 模式（大写，空格分隔）
- 便捷宏 ELIB_LOGI / ELIB_LOGW / ELIB_LOGE / ELIB_LOGD
- C99 标准，无外部依赖

## 目录结构

```
elib-log/
├── include/
│   ├── elib_log.h           # 公共API + 便捷宏
│   ├── elib_log_err.h       # 错误码
│   └── elib_log_types.h     # 类型定义
├── src/
│   ├── elib_log_core.c      # 核心实现
│   └── elib_log_core.h      # 内部头文件
├── test/
│   └── test_elib_log.c      # 单元测试
├── scripts/
│   ├── setup-push-remote.sh
│   └── setup-push-remote.bat
├── LICENSE
└── README.md
```

## API 参考

| 函数 | 说明 |
|------|------|
| `elib_log_init(ctx, cfg)` | 初始化，用户传入缓冲区和回调 |
| `elib_log_deinit(ctx)` | 反初始化 |
| `elib_log_log(ctx, level, head, fmt, ...)` | 格式化日志输出 |
| `elib_log_hex(ctx, level, head, data, len)` | Hex dump 输出 |
| `elib_log_set_level(ctx, min_level)` | 设置最低输出等级 |
| `elib_log_get_level(ctx)` | 获取当前最低输出等级 |

## 使用示例

```c
#include "elib_log.h"

/* 用户实现输出接口 */
void my_output(const char *str)
{
    uart_write(str, strlen(str));
}

/* 用户实现时间戳接口（可选） */
elib_log_timestamp_t my_timestamp(void)
{
    elib_log_timestamp_t ts;
    ts.s  = get_system_seconds();
    ts.ms = get_system_ms() % 1000;
    return ts;
}

int main(void)
{
    static char fmt_buf[128];
    static elib_log_ctx_t log_ctx;

    elib_log_cfg_t cfg = {
        .fmt_buf      = fmt_buf,
        .fmt_buf_size = sizeof(fmt_buf),
        .output       = my_output,
        .timestamp    = my_timestamp,  /* NULL = 无时间戳 */
    };

    elib_log_init(&log_ctx, &cfg);

    /* 输出: \r\n\033[32m[12323.678][Sensor]:data is 23.6\033[0m */
    ELIB_LOGI(&log_ctx, "Sensor", "data is %d.%d", 23, 6);

    /* 无时间戳: \r\n\033[32m[Sensor]:data is 23.6\033[0m */
    /* 无head:   \r\n\033[32mdata is 23.6\033[0m */

    /* Hex dump: \r\n\033[31m[12323.678][UART]:02 89 A6 FF 5E\033[0m */
    uint8_t data[] = {0x02, 0x89, 0xA6, 0xFF, 0x5E};
    ELIB_LOG_HEX(&log_ctx, ELIB_LOG_ERROR, "UART", data, 5);

    /* 运行时等级过滤 */
    elib_log_set_level(&log_ctx, ELIB_LOG_WARNING);
    ELIB_LOGD(&log_ctx, "Tag", "this will be skipped");
    ELIB_LOGW(&log_ctx, "Tag", "this will output");

    elib_log_deinit(&log_ctx);
    return 0;
}
```

## 输出格式

每条日志以 `\r\n` 开头，未注册/为空的前缀自动跳过：

完整格式：`\r\n\033[颜色m[秒.毫秒][Head]:消息\033[0m`

| 条件 | 输出 |
|------|------|
| 时间戳+Head | `\r\n\033[32m[12323.678][Sensor]:data is 23.6\033[0m` |
| 仅Head | `\r\n\033[32m[Sensor]:data is 23.6\033[0m` |
| 仅时间戳 | `\r\n\033[32m[12323.678]:data is 23.6\033[0m` |
| 都无 | `\r\n\033[32mdata is 23.6\033[0m` |

Hex dump：`\r\n\033[31m[12323.678][UART]:02 89 A6 FF 5E\033[0m`

## 日志等级与颜色

| 等级 | ANSI颜色 | 宏 |
|------|----------|-----|
| ERROR | 红色 `\033[31m` | `ELIB_LOGE` |
| WARNING | 黄色 `\033[33m` | `ELIB_LOGW` |
| INFO | 绿色 `\033[32m` | `ELIB_LOGI` |
| DEBUG | 灰色 `\033[90m` | `ELIB_LOGD` |

## 编译

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o test_elib_log test/test_elib_log.c src/elib_log_core.c && ./test_elib_log
```

## License

MIT
