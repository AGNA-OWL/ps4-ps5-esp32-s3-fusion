# PS4 + PS5 S3 Fusion Portal

一块 **ESP32-S3 N16R8**（16 MB Flash + 8 MB PSRAM）同时为 PS4 和 PS5 提供本地漏洞网页托管：

- Wi‑Fi 热点 + 通配符 DNS 劫持
- HTTP（80）+ HTTPS（443）静态站点
- PS5 WebKit Autoloader（用户指南入口）
- PS4 PSPulse 多固件页面（5.05 / 6.72 / 7.00 / 9.00 / 13.00 / 13.52）
- PS4 Afterglow 页面（12.00 – 14.02）
- PS4 9.00 所需的 exfathax USB 大容量存储模拟（ESP32-S3 USB‑OTG）
- 网页管理端：上传 payload、查看状态、重启

> 这是**临时越狱 /  exploits 本地托管**，不是给 PS4/PS5 刷写官方系统固件。主机重启后需要重新触发。

---

## 1. 硬件

| 项目 | 要求 |
|---|---|
| 芯片 | ESP32-S3，推荐 `N16R8`（16 MB Flash + 8 MB Octal PSRAM） |
| USB | 必须使用 **USB‑OTG**（GPIO19/20）才能模拟 9.00 的 U 盘；DevKitC‑1 / T‑Dongle‑S3 均可 |
| 供电 | 插主机背面 USB 口；前置口或 HUB 可能供电不足 |
| 其他 | 数据线（刷机用），电脑一台 |

只做 PS5 + PS4 纯网页链时，ESP32-C3 也能跑，但本工程按 S3 N16R8 配置。

---

## 2. 覆盖的系统版本

| 主机 | 版本 | 入口 | 说明 |
|---|---|---|---|
| PS5 | 7.00 – 13.60（Poops / Relapse） | 用户指南 → 本机 HTTPS | 与上游 autoloader 一致；9.05 只能用 Poops |
| PS4 | 5.05 / 6.72 / 7.00 | `/ps4/pspulse/<版本>/index.html` | 纯 WebKit 链 |
| PS4 | 9.00 | `/ps4/pspulse/900/index.html` | 需要 USB exfathax，本工程开机即模拟 |
| PS4 | 13.00 / 13.52 | `/ps4/pspulse/<版本>/index.html` | PSPulse 页面 |
| PS4 | 12.00 – 14.02 | `/ps4/afterglow/index.html` | 选与系统版本对应的 patchset |
| PS4 | 9.60 / 10.00 / 10.01 / 11.00 | 不支持 | 这些走 PPPwn（网线 PPPoE），不是 WiFi 网页方案 |

**每个版本必须选对应页面**，选错不会生效，也不会降级主机。

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
├── templates/                  # 自写的 PS4 选择页 / 管理页
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

脚本会克隆四个浅仓库到 `.cache/`，把静态文件放进 `data/`，对文本/payload 做 gzip（原文件删除），并生成 `src/exfathax.h`。
当前打包体积约 **6.5 MiB**，远小于 12.94 MiB 的 LittleFS 分区。

### 4.4 烧录

按住板子 **BOOT** 键再插 USB 进入下载模式，然后：

```bash
pio run -t uploadfs    # 上传 LittleFS（网页和 payload）
pio run -t upload      # 上传固件
```

也可以用网页烧录器刷入 `merge_bin` 后的整片镜像（自行用 esptool 合并 bootloader / partitions / app / littlefs）。

本地已用 PlatformIO 6.2.0 + `espressif32@6.7.0` 编译验证通过：固件约 **965 KB / 1.5 MB**，LittleFS 镜像按分区填满 **13 MB**，数据实际占用约 6.5 MB。

---

## 5. 使用

1. 给 ESP32-S3 上电，等待约 2 秒。
2. 主机连接热点：
   - SSID：`PS4_PS5_PORTAL`
   - 密码：`12345678`
3. 按需访问：
   - **PS5**：打开「用户指南」（DNS 已把 `manuals.playstation.net` 指到 ESP32），加载 WebKit Autoloader，按页面提示安装快捷方式。
   - **PS4**：用浏览器打开 `http://192.168.4.1/ps4/`，选择对应固件页面。
   - **PS4 9.00**：选择 9.00 页面；USB‑OTG 口已模拟 exfathax 设备，把它插到主机 USB 口。
4. 管理页面：`http://192.168.4.1/admin.html`

### 主要路由

| 路径 | 用途 |
|---|---|
| `/` | PS5 WebKit Autoloader |
| `/ps4/` | PS4 固件选择页 |
| `/ps4/pspulse/` | PSPulse 固件选择器 |
| `/ps4/pspulse/900/index.html` | PS4 9.00（USB 触发） |
| `/ps4/afterglow/` | Afterglow 12.00 – 14.02 |
| `/admin.html` | 管理页 |
| `/api/status` | 运行状态 JSON |
| `/api/files` | 已上传 payload 列表 |
| `/upload` / `/delete` / `/reboot` | 管理 API（POST） |

`/payloads/` 下的文件可通过管理页上传，供页面按需引用。

---

## 6. 推送到你自己的 GitHub 仓库

本地仓库已经准备好了一个初始提交。如果你刚注册 GitHub，按下面做：

1. 打开 <https://github.com/new>。
2. Repository name 填 `ps4-ps5-s3-fusion`（或任意名字）。
3. 选择 Public 或 Private。
4. **不要**勾选 “Add a README file”、`.gitignore` 或 license，保持仓库为空。
5. 点 Create repository，复制页面上的地址，例如
   `https://github.com/你的用户名/ps4-ps5-s3-fusion.git`。
6. 把地址发给助手，或自己在项目目录执行：

   ```bash
   git remote add origin https://github.com/你的用户名/ps4-ps5-s3-fusion.git
   git push -u origin main
   ```

如果本机未登录 GitHub，推送时会要求凭据。可以用 `gh auth login` 登录，或在 GitHub → Settings → Developer settings → Personal access tokens 生成 token，在提示时把它当密码使用（勾选 `repo` 权限）。

## 7. 已知限制

- **PPPwn 不在范围内**：9.60 / 10.x / 11.00 需要网线 PPPoE，不是访问网页。
- 越狱不持久；主机重启后重新触发。
- 高版本页面可能失败，WebKit 漏洞本身有成功率，不是 100%。
- DNS 是通配符劫持：连接热点的设备所有域名都会被指到 ESP32；请仅用于离线测试。
- HTTPS 使用自签名证书，仅供本地触发，不提供真实安全性。
- 请保持主机离线，不要登录账号；可能违反 Sony 服务条款并导致封禁。
- 仅在自有设备上使用；作者不对任何损坏或数据丢失负责。

---

## 8. 来源与许可

本工程原始代码的许可证见 [LICENSE](LICENSE)（MIT）。以下上游资源由脚本运行时下载，**不随本仓库分发**：

- [percdotdev/ps5-webkit-autoloader-esp32](https://github.com/percdotdev/ps5-webkit-autoloader-esp32) — PS5 autoloader host
- [psx8/psx8.github.io](https://github.com/psx8/psx8.github.io) — PSPulse PS4 host 与 payload
- [well0nez/afterglow-web](https://github.com/well0nez/afterglow-web) — Afterglow host（AGPL-3.0）
- [doj/ESP32-Server-900u](https://github.com/doj/ESP32-Server-900u) — exfathax USB 数据结构

发布包含上游资源的固件镜像前，请自行确认各上游项目的再分发条款。
