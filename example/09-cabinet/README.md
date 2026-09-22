# Пример к главе 9: нагреваемый шкаф хранения

Готовый рабочий проект к разделу [«Собираем своё устройство»](../../docs/ru/09-build-a-device/). Один ESP32-C3 + `idryer-core`: SHT31 (климат), термистор (контроль нагревателя), нагреватель и вентилятор через GPIO-ключи, меню и привязка к порталу.

## Состав

```
09-cabinet/
├── platformio.ini            # окружения cabinet и bench (ESP32-C3)
├── lib/idryer-core ->        # библиотека ядра (локально — симлинк; при публикации станет git submodule)
├── extra_scripts/
│   └── pre_gen_menu.py       # pre-build генерация меню из menu.yaml
└── src/
    ├── main.cpp              # код устройства (финал главы 7): меню на портале, действия карточки, revoke
    ├── menu/menu.yaml        # описание меню (target_temp, hysteresis)
    ├── Sht31ClimateSensor.*  # драйвер SHT31 (скопирован из iDryer-Storage)
    ├── IClimateSensor.h
    └── sensor_reading.h
```

## Сборка

```bash
pio run -e cabinet      # рабочая прошивка устройства
pio run -e bench        # стендовый тест без облака (см. BENCH.md)
```

При первой сборке PlatformIO скачает библиотеки ядра из `lib/idryer-core/library.json` и драйвер `SHT31`, а pre-build хук сгенерирует файлы меню в `src/menu/`.

Wi-Fi и привязка к аккаунту в прошивку не зашиваются: после прошивки подключите устройство в приложении iDryer — «Подключить новое устройство», шаги Wi-Fi и «Привязка» (глава 4).

Подробное пошаговое объяснение — в главах раздела 9. Проверка на железе — [BENCH.md](BENCH.md).
