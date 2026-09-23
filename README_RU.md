# Ambilight ESP32-C6

[English](README.md) | [Русский](README_RU.md)

[![Latest release](https://img.shields.io/github/v/release/VooxergAlt2/ambilight)](https://github.com/VooxergAlt2/ambilight/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

DIY-контроллер Ambilight на ESP32-C6 для работы с HyperHDR через DDP. Проект поддерживает четыре синхронных выхода на светодиодные ленты, опциональный датчик VL53L5CX для коррекции яркости по расстоянию до стены, встроенный Web UI и ограниченный WLED-compatible интерфейс для Home Assistant.

Проект вырос из реальной установки за телевизором, а не из попытки сделать универсальный LED-контроллер. Поэтому основное внимание уделено стабильному realtime-потоку RGB, предсказуемой работе с физической топологией лент, удобной пусконаладке и сохранности конфигурации.

> **Статус проекта:** активно развивается и проверяется на реальном железе. Прошивка уже пригодна для использования, но это по-прежнему DIY-проект, а не готовое потребительское устройство.

## Возможности

- realtime RGB от **HyperHDR по DDP UDP/4048**;
- **4 синхронных LED-выхода** через периферию PARLIO ESP32-C6;
- **без отдельного фиксированного общего лимита количества LED**: runtime-буферы масштабируются под активную топологию;
- изменение количества LED, GPIO и направления каждой стороны без перекомпиляции;
- безопасный режим пусконаладки для логических сторон и физических GPIO;
- возможность постоянно отключить один физический пиксель на каждой стороне без сдвига логической адресации;
- опциональный **VL53L5CX 8×8 ToF** для оценки положения стены и коррекции по каждому LED;
- режимы коррекции DISABLED / SHADOW / ACTIVE;
- встроенный Web UI для настройки и диагностики;
- управление питанием, раздельной яркостью, RGB и локальными эффектами из **Home Assistant** через WLED-compatible API;
- хранение runtime-настроек в NVS;
- сохранение неизвестных записей конфигурации при upgrade/downgrade прошивки;
- экспорт и восстановление конфигурации из JSON backup;
- резервная Wi-Fi точка доступа через 60 секунд без подключения к основной сети;
- защищённое **Wi-Fi OTA** файла `firmware.bin` в неактивный app-slot с сохранением NVS.

HyperHDR и Home Assistant работают независимо. Realtime-видео всегда приходит по DDP.

## Архитектура

```text
HyperHDR
   |
   | DDP UDP/4048
   v
ESP32-C6
   +--> сборка кадров / защита от нескольких отправителей
   +--> опциональная ToF-коррекция
   +--> AUTO-переключение DDP/локальная подсветка + две яркости
   +--> маска неисправного физического пикселя
   +--> PARLIO x4
   v
LED-ленты

Home Assistant
   |
   +--> mDNS _wled._tcp
   +--> HTTP/80, WLED-compatible JSON

Браузер
   |
   +--> HTTP/80, встроенный Ambilight Web UI
```

WLED realtime UDP и HyperHDR Hyperk намеренно не реализованы. Для HyperHDR используется обычный DDP.

## Железо

### Обязательно

- плата **ESP32-C6**, совместимая с PlatformIO target `esp32-c6-devkitc-1`;
- **16 MiB flash** для текущей таблицы разделов;
- адресная лента с таймингами WS2812/WS2812B GRB;
- отдельный правильно рассчитанный блок питания для ленты;
- общая земля между ESP32/логикой и LED-лентой.

Выходы прошивки:

| PARLIO lane | GPIO |
| --- | ---: |
| 0 | 18 |
| 1 | 19 |
| 2 | 20 |
| 3 | 21 |

Топология по умолчанию соответствует авторской 65-дюймовой установке:

| Сторона ТВ | LED | GPIO | Направление |
| --- | ---: | ---: | --- |
| Верх | 230 | 20 | REV |
| Правая | 160 | 19 | REV |
| Низ | 230 | 21 | REV |
| Левая | 160 | 18 | FWD |

Отдельного потолка в 920 адресов больше нет. Длина каждой стороны хранится как
16-битное значение (`1..65535`), поэтому формат четырёх сторон теоретически
представляет до 262 140 логических LED суммарно. RGB/DDP/gain/PARLIO-буферы
формируются по активной топологии. Практический предел определяется доступной памятью и
временным бюджетом ESP32-C6, а не искусственной константой прошивки.

**На реальном железе проверено до 230 LED на одном выходе.** Конфигурации с
большим количеством LED программно разрешены и покрыты host-тестами больших
топологий, но на физической ленте пока не валидированы.

Не нужно подгонять физическую проводку под эту таблицу. После установки GPIO, стороны и FWD/REV можно переназначить через Web UI.

Между 3.3 В выходом ESP32 и 5 В логикой LED рекомендуется нормальный однонаправленный level shifter/buffer. Питать ленту через плату ESP32 нельзя.

### VL53L5CX

Для ToF используются:

| Сигнал | GPIO |
| --- | ---: |
| SDA | 6 |
| SCL | 7 |

Если VL53L5CX не установлен или не инициализируется, обычный DDP Ambilight продолжает работать. Коррекция fail-open и не блокирует основной RGB-поток.

## Установка прошивки на Windows

Готовые бинарники находятся в **GitHub Releases**.

Установите Python и esptool:

```powershell
py -m pip install --upgrade esptool
```

### Первая установка

Для чистой платы используйте `firmware.factory.bin`:

```powershell
py -m esptool --chip esp32c6 --port COM5 write-flash 0x0 firmware.factory.bin
```

Замените `COM5` на фактический COM-порт платы.

`firmware.factory.bin` содержит bootloader, partition table, boot app и само приложение. Это образ для первой установки и восстановления пустой платы.

### Обновление с сохранением настроек

Для уже настроенного контроллера используйте **только `firmware.bin`**:

```powershell
py -m esptool --chip esp32c6 --port COM5 write-flash 0x10000 firmware.bin
```

Перед обычным обновлением **не запускайте `erase-flash`** и не используйте `firmware.factory.bin`, если хотите сохранить NVS.

Перед обслуживанием рекомендуется скачать backup в Web UI: **System → Configuration backup**.

Подробности: [Windows / HyperHDR / firmware installation](docs/windows-hyperhdr.md).

## Первый запуск и резервная Wi-Fi сеть

Wi-Fi выбирается в следующем порядке:

1. настройки из NVS;
2. опциональные compile-time credentials из `include/secrets.h`;
3. если подключения нет, через 60 секунд запускается резервная AP.

```text
SSID:     Ambilight-XXXXXX
пароль:   ambilight
IP:       4.3.2.1
Web UI:   http://4.3.2.1/
```

Контроллер продолжает пытаться подключиться к основной Wi-Fi сети даже при работающей AP. После успешного подключения резервная точка доступа автоматически выключается.

## HyperHDR на Windows

Отдельный проприетарный или специальный Windows-плагин этому проекту **не нужен**. PC-side часть проекта — обычный HyperHDR.

В HyperHDR контроллер добавляется как DDP-устройство:

```text
Протокол: DDP
Порт:     UDP/4048
Адрес:    IP контроллера
LED:      сумма четырёх активных сторон
```

Количество LED отображается в Web UI контроллера.

Нюансы:

- не выбирайте WLED realtime UDP;
- не выбирайте HyperHDR Hyperk;
- физические GPIO и REV/FWD настраиваются на ESP32, а не костылями в раскладке HyperHDR;
- одновременно контроллер принимает одного активного DDP-отправителя;
- короткий пропуск DDP-пакетов не создаёт искусственный чёрный кадр; в AUTO после 1,5 с без полного DDP-кадра включается локальный fallback, а свежий DDP автоматически возвращает Ambilight;
- настоящий полный чёрный кадр от HyperHDR остаётся валидным и выключает LED.

Подробная инструкция: [docs/windows-hyperhdr.md](docs/windows-hyperhdr.md).

## Home Assistant

ESP32 публикует `_wled._tcp.local.` и реализует тот минимум WLED JSON API, который нужен интеграции Home Assistant.

Доступны:

- включение/выключение;
- яркость;
- RGB;
- `Ambilight` (AUTO);
- `Solid`, `Rainbow`, `Breathing`;
- `Warm White`, `Bias White`, `Sunset`, `Candle`, `Aurora`, `Twinkle`.

`Ambilight` работает как AUTO: пока DDP свежий, выводит DDP; после 1,5 с без полного кадра включает сохранённый локальный fallback; первый свежий DDP-кадр автоматически возвращает Ambilight. Явно выбранный локальный эффект имеет приоритет, при этом DDP продолжает приниматься в фоне.

Это compatibility layer, а не форк WLED. Ограничения описаны в [docs/wled-ha-compat.md](docs/wled-ha-compat.md).

## Web UI

Откройте:

```text
http://<IP-контроллера>/
```

Разделы:

- **Home** — состояние, раздельная яркость DDP/подсветки, AUTO/fallback, эффекты, DDP и ToF;
- **LED** — топология, GPIO, REV/FWD, тесты и маска неисправного пикселя;
- **ToF** — матрица 8×8, геометрия, gain curve и калибровка;
- **Diagnostics** — DDP, heap, ToF и renderer counters;
- **System** — Wi-Fi, Wi-Fi OTA, backup/restore и factory reset.

У Web UI пока нет отдельной аутентификации. Не публикуйте TCP/80 в интернет, используйте интерфейс только в доверенной LAN.

## Backup и сохранение настроек

JSON backup содержит:

- power, отдельные DDP brightness и lighting brightness, локальный lighting/fallback profile;
- correction mode;
- LED topology;
- disabled-pixel mask;
- ToF geometry;
- gain curve;
- Wi-Fi SSID.

Пароль Wi-Fi намеренно **не экспортируется**.

Обычное обновление `firmware.bin`, включая Wi-Fi OTA через System, не стирает NVS. Кроме того, при загрузке прошивка не удаляет неизвестные schema-version, поэтому downgrade/recovery остаётся возможным.

Полный erase flash или прошивка combined factory image поверх существующей установки NVS уничтожит.


### Wi-Fi OTA

В **System → Обновление прошивки по Wi-Fi** нажмите «Разрешить OTA на 120 с», выберите только `firmware.bin` из релиза и запустите загрузку. Контроллер выдаёт одноразовый токен, проверяет, что образ является ESP32-C6 application image, пишет его потоково в неактивный app-slot, гасит LED на время записи и после успешной проверки перезагружается. `firmware.factory.bin`, bootloader и образы другой SoC отклоняются до записи приложения. Подробнее: [docs/wifi-ota.md](docs/wifi-ota.md).

## ToF-коррекция

VL53L5CX используется как медленный датчик геометрии, а не как часть realtime RGB pipeline.

```text
distance_i = wall_plane(x_i, y_i) - led_z_i
gain_i     = configured_curve(distance_i)
```

Режимы:

- **DISABLED** — без коррекции;
- **SHADOW** — расчёт коррекции без изменения физического RGB;
- **ACTIVE** — применение коррекции с ограниченной скоростью изменения.

Если геометрия невалидна, устарела или ToF недоступен, вывод возвращается к исходному RGB HyperHDR.

## Пусконаладка

Для монтажа предусмотрены отдельные инструменты:

- raw GPIO probe для определения физической линии;
- logical-side probe для проверки стороны ТВ;
- проверка REV/FWD;
- изменение COUNT/GPIO/REV без перепрошивки;
- отключение одного физического LED на стороне;
- live 8×8 ToF matrix.

## Сборка из исходников

Требуются Python 3 и PlatformIO.

```powershell
python -m pip install --upgrade platformio
powershell -ExecutionPolicy Bypass -File tools/validate.ps1
```

Или:

```bash
pio test -e native
pio run -e esp32-c6-devkitc-1
```

## Текущая проверка

Для `v0.46.4`:

- partition check: **PASS**;
- Web UI structural check: **PASS**;
- native tests: **272 / 272 PASS**;
- ESP32-C6 build: **PASS**;
- RAM: **70 460 / 327 680 байт (21,5%)**;
- application image: **1 342 096 / 7 340 032 байт (18,3%)**.

## Документация

- [Архитектура](docs/architecture.md)
- [Windows / HyperHDR](docs/windows-hyperhdr.md)
- [Runtime Wi-Fi](docs/runtime-wifi.md)
- [Web UI](docs/web-ui.md)
- [WLED / Home Assistant](docs/wled-ha-compat.md)
- [LED mapping](docs/runtime-led-mapping.md)
- [Сохранение и восстановление конфигурации](docs/runtime-config-recovery.md)
- [ToF processing](docs/tof-processing.md)

Часть файлов в `docs/` сохраняет инженерную историю отдельных stage разработки. Для текущего поведения авторитетны код, этот README и английский `README.md`.

## Лицензия

Проект распространяется по лицензии [MIT](LICENSE).

Лицензии и атрибуция сторонних компонентов приведены в [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Важно

Это независимый DIY-проект, не связанный официально с HyperHDR, WLED, Home Assistant, Espressif, STMicroelectronics или Adafruit.

Ответственность за электрическую безопасность, расчёт блока питания, сечение проводов, предохранители, температурный режим и совместимость конкретной LED-ленты остаётся на сборщике устройства.
