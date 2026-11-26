# PS4 + PS5 S3 Fusion Portal

一块 **ESP32-S3 N16R8**（16 MB Flash + 8 MB PSRAM）同时为 PS4 和 PS5 提供本地漏洞网页托管：

- Wi‑Fi 热点 + 通配符 DNS 劫持
- HTTP（80）+ HTTPS（443）静态站点
- PS5 WebKit Autoloader（用户指南入口，自动识别固件）
- **PS4 一键破解**（GamerHack g2all）：打开 `/ps4/` 自动识别系统版本并加载对应漏洞（5.05 / 7.00–14.02），无需手动选择
- PS4 6.72 专用入口（g2all 不支持 6.72，单独保留 PSPulse 的 6.72 链）
- exfathax USB 大容量存储模拟（ESP32-S3 USB‑OTG，保留备用；g2all 的 9.00 已走 WebKit 链，不再依赖 USB）
- 网页管理端：系统监控、payload 升级（无需重烧固件）、重启

> 这是**临时越狱 /  exploits 本地托管**，不是给 PS4/PS5 刷写官方系统固件。主机重启后需要重新触发。

---

## 1. 硬件

| 项目 | 要求 |
|---|---|
| 芯片 | ESP32-S3，推荐 `N16R8`（16 MB Flash + 8 MB Octal PSRAM） |
| USB | 如要用 exfathax 备用方案需 **USB‑OTG**（GPIO19/20）；只用 g2all 网页链则不需要 |
| 供电 | 插主机背面 USB 口；前置口或 HUB 可能供电不足 |
| 其他 | 数据线（刷机用），电脑一台 |

只做 PS5 + PS4 纯网页链时，ESP32-C3 也能跑，但本工程按 S3 N16R8 配置。

---

## 2. 覆盖的系统版本

| 主机 | 版本 | 入口 | 说明 |
|---|---|---|---|
| PS5 | 7.00 – 13.60（Poops / Relapse） | 用户指南 → 本机 `/` | 自动识别固件；9.05 只能用 Poops |
| PS4 | 5.05 / 5.07 | `/ps4/` → 自动跳转 | 首次加载会把缓存装入主机 |
| PS4 | 6.72 | `/ps4/pspulse/672/`（手动页里的专用入口） | g2all 不支持，单独保留 |
| PS4 | 7.00 – 8.52 | `/ps4/` → 自动跳转 | psfree + lapse 链 |
| PS4 | 9.00 – 9.60 | `/ps4/` → 自动跳转 | 纯 WebKit 链，无需 USB |
| PS4 | 10.00 – 11.02 | `/ps4/` → 自动跳转 | ChendoChap CSS 内核漏洞 |
| PS4 | 11.50 – 14.02 | `/ps4/` → 自动跳转 | slopkit 链 |

**打开 `/ps4/` 即自动识别**：入口页读取主机 User-Agent 里的系统版本，跳到对应 g2all 漏洞链。首次会把文件装入主机离线缓存，按提示关闭浏览器、断开网络再打开即可触发。g2all 覆盖 5.05 / 7.00–14.02；**6.72 不在其识别区间**，需从 `/ps4/` 的手动区进入 6.72 专用入口。传统 PPPwn（网线 PPPoE）方案不在本 WiFi 托管范围内。

---

## 3. 工程结构

```
ps4-ps5-s3-fusion/
├── platformio.ini              # S3 N16R8 构建配置
├── partitions_16mb.csv         # 1.5 MB app x2 + ~12.9 MB LittleFS
├── src/
│   ├── main.cpp                # 启动流程
│   ├── portal_config.h         # SSID / 密码 / 路由常量
│   ├── portal_common.*         # 路径规范化、MIME、gzip 文件解析
│   ├── dns_portal.*            # 自实现 UDP DNS 通配符响应
│   ├── http_portal.*           # HTTP 80：静态页 + 管理 API
│   ├── https_portal.*          # HTTPS 443：PS5 用户指南入口
│   ├── usb_msc.*               # exfathax USB MSC（9.00）
│   ├── server_certs.h          # 生成的自签名证书
│   └── exfathax.h              # 由 stage 脚本从上游生成，不入库
├── templates/                  # 自写的门户 / PS4 入口 / 监控 / 管理页
├── tools/
│   ├── stage_payloads.py       # 拉取 + 裁剪 + gzip 上游主机文件
│   └── gen_certs.py            # 生成自签名证书头文件
└── data/                       # 生成物，不入库（烧录 LittleFS 用）
```

上游的 exploit 页面和 payload **不提交到 git**，`data/` 和 `src/exfathax.h` 都由脚本生成。

---

## 4. 构建

### 4.1 安装 PlatformIO

```bash
python3 -m pip install platformio
```

### 4.2 生成证书（仓库里已附带一份；需要更换时执行）

```bash
python3 tools/gen_certs.py --output src/server_certs.h
```

### 4.3 拉取并打包上游页面

```bash
python3 tools/stage_payloads.py --update
```

脚本会克隆四个浅仓库（PS5 autoloader、PSPulse 仅 6.72、GamerHack g2all、exfathax）到 `.cache/`，把静态文件放进 `data/`，对文本/payload 做 gzip（原文件删除），并生成 `src/exfathax.h`。
g2all 自带 5.05 / 7.00–14.02 的完整自洽漏洞链，因此 PSPulse 的其他固件目录与 Afterglow 已精简掉，仅保留 g2all 不支持的 6.72。
当前打包体积约 **2.6 MiB**，远小于 12.94 MiB 的 LittleFS 分区。

### 4.4 烧录

按住板子 **BOOT** 键再插 USB 进入下载模式，然后：

```bash
pio run -t uploadfs    # 上传 LittleFS（网页和 payload）
pio run -t upload      # 上传固件
```

也可以用网页烧录器刷入 `merge_bin` 后的整片镜像（自行用 esptool 合并 bootloader / partitions / app / littlefs）。

本地已用 PlatformIO 6.2.0 + `espressif32@6.7.0` 编译验证通过：固件约 **968 KB / 1.5 MB**，LittleFS 镜像按分区填满 **13 MB**，数据实际占用约 2.6 MB。

> Windows 用户名含中文时，PlatformIO 可能因为工具链路径编码报 `Arduino.h: No such file or directory`。
> 建议把 core 目录（默认在 `%USERPROFILE%\.platformio`，工具链缓存约 4 GB）整体放到 D 盘的纯 ASCII 路径，
> 既避开中文路径问题，也节省 C 盘空间：
>
> ```powershell
> Move-Item "$env:USERPROFILE\.platformio" D:\.platformio
> New-Item -ItemType Junction -Path "$env:USERPROFILE\.platformio" -Target D:\.platformio
> $env:PLATFORMIO_CORE_DIR = "D:\.platformio"
> pio run
> ```
>
> 若之前已用 `C:\pio` 联接，把它重新指向 `D:\.platformio` 即可保持原有编译命令不变。

---

## 5. 使用

1. 给 ESP32-S3 上电，等待约 2 秒。
2. 主机连接热点：
   - SSID：`PS4_PS5_PORTAL`
   - 密码：`12345678`
3. 按需访问：
   - **PS5**：打开「用户指南」（DNS 已把 `manuals.playstation.net` 指到 ESP32），自动识别固件并按提示安装快捷方式。
   - **PS4**：浏览器打开 `http://192.168.4.1/ps4/`，**自动识别系统版本**并加载对应漏洞，无需手动选择。
   - **PS4 6.72**：g2all 不支持该版本，在 `/ps4/` 的手动区点「6.72 专用入口」。
4. 电脑浏览器可访问门户与维护页（见下表）。

### 主要路由

| 路径 | 用途 |
|---|---|
| `/` | PS5 WebKit Autoloader（用户指南入口，自动识别固件） |
| `/portal.html` | 简洁门户首页（PS5 / PS4 / 监控 / 升级） |
| `/ps4/` | **PS4 一键入口**：自动识别固件 → g2all 对应漏洞链 |
| `/ps4/g2all/` | GamerHack g2all 自动识别主机（5.05 / 7.00–14.02） |
| `/ps4/pspulse/672/` | PS4 6.72 专用入口（g2all 不支持的版本） |
| `/status.html` | 系统监控（AP 客户端、内存、PSRAM、LittleFS、运行时间） |
| `/admin.html` | 破解程序升级 / payload 管理 / 重启 |
| `/api/status` | 运行状态 JSON |
| `/api/files` | 可替换 payload 列表（全盘扫描 `.bin` / `.elf` / PS5 `.js`） |
| `/upload?path=...` / `/delete` / `/reboot` | 管理 API（POST） |

**升级破解程序**：管理页会列出固件里所有可替换的 payload（GoldHEN、g2all 的 `goldhen_2.4b18.14.bin`、各固件补丁、PS5 autoloader 等）。选中目标后上传新的原始文件即可覆盖对应路径，旧的 gzip 缓存会自动清除，下次触发漏洞时主机直接加载新文件——**无需重新烧录固件**。请务必选对与主机固件匹配的路径；上传的文件不会自动校验内容，用错 payload 会导致触发失败。

---

## 6. 已知限制

- **g2all 的区间覆盖**：10.00–11.02 走 ChendoChap CSS 内核漏洞，11.50–14.02 走 slopkit；9.60、10.01、11.00 等具体小版本以 g2all 的 User-Agent 判定为准。传统 PPPwn（网线 PPPoE）方案不在本 WiFi 托管范围内。
- **6.72 是唯一例外**：g2all 的识别逻辑只认 5.05 / 5.07 和 7.00–14.02，6.72 会显示 Unsupported Firmware，本工程为此单独保留 PSPulse 的 6.72 链。
- 越狱不持久；主机重启后重新触发。
- 高版本页面可能失败，WebKit 漏洞本身有成功率，不是 100%。
- DNS 是通配符劫持：连接热点的设备所有域名都会被指到 ESP32；请仅用于离线测试。
- HTTPS 使用自签名证书，仅供本地触发，不提供真实安全性。
- **USB-OTG（仅备用）**：g2all 的 9.00 已是纯 WebKit 链，不再需要 USB；保留的 exfathax 模拟依赖 ESP32-S3 原生 USB 引脚（GPIO19/20）。DevKitC-1 板载 USB 口是 USB-UART 桥，不是 OTG，需要飞线；T-Dongle-S3 等 USB-A 直插板天然满足。
- 每次启动会生成随机 AppCache 版本号，确保更新页面后主机刷新缓存。
- 请保持主机离线，不要登录账号；可能违反 Sony 服务条款并导致封禁。
- 仅在自有设备上使用；作者不对任何损坏或数据丢失负责。

---

## 7. 来源与许可

本工程原始代码的许可证见 [LICENSE](LICENSE)（MIT）。以下上游资源由脚本运行时下载，**不随本仓库分发**：

- [percdotdev/ps5-webkit-autoloader-esp32](https://github.com/percdotdev/ps5-webkit-autoloader-esp32) — PS5 autoloader host
- [psx8/psx8.github.io](https://github.com/psx8/psx8.github.io) — PSPulse 6.72 host 与 payload（仅保留 6.72）
- [GamerHack/GamerHack.github.io](https://github.com/GamerHack/GamerHack.github.io) — g2all 多固件自动识别入口（5.05 / 7.00–14.02；仓库未声明许可证，请自行确认再分发条款）
- [doj/ESP32-Server-900u](https://github.com/doj/ESP32-Server-900u) — exfathax USB 数据结构

发布包含上游资源的固件镜像前，请自行确认各上游项目的再分发条款。
