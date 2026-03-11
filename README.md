b# Rotating Table (ESP-IDF, M5Stamp-C3)

Проект управления вращающимся столом с наклоном:
- шаговый двигатель 28BYJ-48 через ULN2003
- серво для наклона (PWM)
- кнопка на `GPIO10`: короткое нажатие `Power`, удержание > 5 сек `Reset`
- Web UI для настроек Wi-Fi, движения и профилей

## Реализовано
- Wi-Fi:
  - `STA + fallback AP` (если STA не поднялся, включается AP)
  - `AP only`
- Web UI:
  - вкладка `Control`: профиль, угол поворота, угол наклона, `Start/Stop`, `Reset(Home 0/0)`
  - вкладка `Profiles`: параметры и профили `create / copy / delete / activate`
  - вкладка `Wi-Fi`: режим AP/STA и SSID/пароли
  - вкладка `About`: имя, версия, `Reset settings` с подтверждением
- Сохранение настроек в NVS

Параметры по умолчанию:
- `min_angle = -45`
- `max_angle = 45`
- стартовый угол при загрузке: `0`
- `turn_grad = 10` означает: 1 оборот стола изменяет наклон на 10 градусов

## Структура модулей
- `main/main.c` — оркестрация запуска
- `main/app_state.c` — состояние, профили, load/save NVS
- `main/motor_control.c` — шаговик + серво + motion task
- `main/button_control.c` — обработка кнопок
- `main/wifi_manager.c` — STA/AP/fallback
- `main/web_server.c` — Web UI и REST API
- `main/board_config.h` — пины и калибровка моторов

## Пины и калибровка (пункт 3)
Все аппаратные параметры вынесены в `main/board_config.h`:
- GPIO пины для ULN2003, серво и кнопок
- `STEPPER_STEPS_PER_REV`
- `STEPPER_DIR` (`1` или `-1`)
- `SERVO_MIN_US`, `SERVO_MAX_US`, `SERVO_PERIOD_HZ`
- пределы наклона `TILT_MIN_DEG_LIMIT`, `TILT_MAX_DEG_LIMIT`

Текущая дефолтная карта:
- `IN1=GPIO4`, `IN2=GPIO5`, `IN3=GPIO6`, `IN4=GPIO7`
- `SERVO_PWM=GPIO8`
- `BTN_POWER=GPIO10`, `BTN_RESET=GPIO10` (одна кнопка: short/long press)

Если у вас другая разводка, меняйте только `board_config.h`.

## Схема подключения
PNG версия: `docs/wiring-diagram.png`

```text
M5Stamp-C3                     ULN2003 board                 28BYJ-48
-----------                    -------------                 --------
GPIO4  ----------------------> IN1
GPIO5  ----------------------> IN2
GPIO6  ----------------------> IN3
GPIO7  ----------------------> IN4
GND    ----------------------> GND -------------------------> GND
5V ext ----------------------> VCC -------------------------> +5V motor
                                                     (5-pin cable from ULN2003 to motor)

M5Stamp-C3                     Servo (tilt)
-----------                    ------------
GPIO8  ----------------------> PWM/SIG
GND    ----------------------> GND
5V ext ----------------------> +5V

Button (active-low, shared actions):
GPIO10 ---[button]---------- GND
short press -> power ON/OFF
hold >5 sec -> factory reset

Power:
- M5Stamp-C3: from USB/5V as usual
- ULN2003 + stepper: external 5V
- Servo: external 5V (желательно отдельная линия)
- All grounds must be common: M5 GND = ULN2003 GND = Servo GND = PSU GND
```

Генератор PNG: `tools/generate_wiring_png.py`

## Сборка и прошивка
```bash
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Web UI
- STA режим: откройте IP устройства в локальной сети
- AP режим: подключитесь к `RTable-Setup` (пароль `12345678`) и откройте `http://192.168.4.1/`

## Подключение питания
- ULN2003 + 28BYJ-48: 5V
- Серво: отдельный 5V источник (желательно)
- Обязателен общий GND между M5Stamp-C3, ULN2003 и серво
