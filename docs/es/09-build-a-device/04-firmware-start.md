---
title: "Inicio de firmware en idryer-core: primer inicio y vinculación al portal"
description: "Crear un proyecto PlatformIO con la biblioteca idryer-core: platformio.ini, Config del dispositivo, primer firmware en el ESP32, configuración de Wi-Fi y vinculación del dispositivo al portal iDryer en la aplicación."
---

# Inicio de firmware en el núcleo

En esta página, crea un proyecto de firmware, lleva el ESP32 al estado Online en el portal y verifica que la parte de red funciona. Los sensores y la lógica de calefacción se agregarán en los siguientes pasos.

El enfoque se basa en la fachada `iDryer::Link`. Describe el dispositivo con una única estructura `iDryer::Config`, llama a `link.begin()` y `link.loop()` — el núcleo maneja toda la conexión de red por sí solo.

## 1. Prepare las herramientas

Necesitará:

- VS Code con la extensión PlatformIO;
- Cable USB;
- Red Wi-Fi `2.4 GHz` (ESP32 no funciona con redes solo `5 GHz`).
- un smartphone con la aplicación iDryer, con la sesión iniciada en tu cuenta del portal iDryer: a través de ella el dispositivo recibe la red Wi-Fi y se vincula a la cuenta.

Qué es el firmware del controlador y cómo se carga en la placa — [Flasheo del controlador](../02-controllers/11-flashing-controller.md).

## 2. Cree el proyecto

En PlatformIO, un proyecto es una carpeta con estructura fija. Cree una carpeta de proyecto (por ejemplo `my-cabinet`) y ábrala en VS Code. Dentro deben estar estos archivos:

```text
my-cabinet/
├── platformio.ini        # configuración de compilación (completaremos en el paso 4)
├── lib/
│   └── idryer-core/      # librería del núcleo (enlace simbólico o copia)
└── src/
    └── main.cpp          # código del dispositivo: Config + setup() + loop()
```

Todos los fragmentos de código a continuación van exactamente en estos archivos — cada paso indica en cuál. Cree manualmente las carpetas `include/`, `lib/` y `src/` si no existen.

Coloque la librería `idryer-core` en `lib/` — PlatformIO encuentra librerías allí automáticamente. La forma más fácil es hacer un enlace simbólico a la librería descargada:

```bash
ln -s /ruta/a/idryer-core lib/idryer-core
```

Esto también es necesario para la generación del menú (capítulo 6) — el hook busca el generador dentro de `lib/idryer-core/`.

## 3. El Wi-Fi y la vinculación no están en el código

El firmware no contiene ni la contraseña de la red ni datos de la cuenta. En el primer arranque el dispositivo no tiene Wi-Fi y espera la configuración: la aplicación iDryer la envía por el aire (ESPTouch) y después vincula el dispositivo a tu cuenta con un token de vinculación de un solo uso. El core hace todo esto dentro de `s_link.begin()` y `s_link.loop()`; tú solo sigues los pasos de la aplicación, en la sección 9.

## 4. Configure platformio.ini

Complete `platformio.ini` en la raíz del proyecto:

```ini
[env:cabinet]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; Las bibliotecas del core (MQTT, ArduinoJson, WebSockets, Improv) llegan
; solas desde lib/idryer-core/library.json.
; ESPAsyncTCP es el transporte de ESP8266 de las dependencias de espMqttClient:
; no compila en ESP32 y hay que excluirlo.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Reemplace `board` por su placa (por ejemplo, `esp32-s3-devkitc-1`). No es necesario indicar `idryer-core` en `lib_deps` — se encuentra en `lib/` (paso 2).

!!! note "Qué hacen estas líneas"
    No hace falta listar las dependencias del core: PlatformIO las toma de `lib/idryer-core/library.json`. `lib_ignore = ESPAsyncTCP` es obligatorio: sin él la compilación falla en `ESPAsyncTCP.cpp`. Los flags `MQTT_BROKER` y `MQTT_PORT` también son obligatorios: sin ellos el core no compila (`'MQTT_BROKER' was not declared`).

## 5. Describa el dispositivo en Config

Desde aquí todo ocurre en un archivo — `src/main.cpp`. Ábralo e ingrese el código de este y los siguientes pasos.

`iDryer::Config` es el pasaporte del dispositivo. Los flags `has*` indican al portal qué tiene el dispositivo y determinan qué campos de telemetría se publican.

Para un gabinete calefaccionado, añada al inicio de `src/main.cpp`:

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Dryer,
    .unitsCount        = 1,
    // Periféricos:
    .hasHeater         = true,    // calefactor controlado
    .hasFan            = true,    // ventilador
    .hasAirTemp        = true,    // temperatura del aire (SHT31)
    .hasAirHumidity    = true,    // humedad del aire (SHT31)
    .hasHeaterTemp     = true,    // temperatura del calefactor (termistor)
    // Períodos de autopublicación:
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // Identificación en el portal:
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};

static iDryer::Link s_link(CFG);
```

!!! note "Los flags has* son un contrato con el portal"
    Un campo de telemetría cuyo flag correspondiente es `false` no se publica. Por ejemplo, sin `hasAirHumidity = true` la humedad no llegará a la nube, incluso si la escribe en el código. Active solo lo que físicamente existe en el dispositivo.

Lista de componentes y flags — [Composición del sistema](02-bom.md).

## 6. Main mínima

En el mismo archivo después del bloque `Config` agregue las funciones `setup()` y `loop()`. Para el primer inicio, es suficiente inicializar el enlace y ejecutarlo en `loop()`:

```cpp
void setup() {
    Serial.begin(115200);
    s_link.begin();
    // El dispositivo se desvinculó en el portal: borrar el secreto y esperar una nueva vinculación.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

`s_link.begin()` levanta el Wi-Fi, la vinculación y la conexión con el portal. El comando `revoke` llega desde el portal cuando el dispositivo se desvincula de la cuenta: `handleRevoke()` borra el secreto del dispositivo y este espera una nueva vinculación. Los sensores se añaden en el paso [Sensores](05-sensors.md).

### Completo `src/main.cpp` después de este capítulo

Tome ambos bloques anteriores en un archivo — este es todo el `src/main.cpp` en este paso:

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Dryer,
    .unitsCount        = 1,
    .hasHeater         = true,
    .hasFan            = true,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = true,
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

void setup() {
    Serial.begin(115200);
    s_link.begin();
    // El dispositivo se desvinculó en el portal: borrar el secreto y esperar una nueva vinculación.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

Cada capítulo muestra **qué agregar** y el **completo `src/main.cpp` después de los cambios**, para que siempre vea el cuadro completo, no fragmentos dispersos.

## 7. Flashee

```bash
pio run -e cabinet -t upload
```

## 8. Abre el Serial Monitor

```bash
pio device monitor -b 115200
```

Mientras el dispositivo no tiene Wi-Fi, el log está en silencio: el core reserva el puerto serie para el instalador web (Improv). Los logs se activan en cuanto se levanta el Wi-Fi. En un dispositivo que aún no está vinculado, el log termina así:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Deja el monitor abierto y pasa a la aplicación.

## 9. Conecta el Wi-Fi y vincula el dispositivo en la aplicación

1. Conecta el teléfono a la red Wi-Fi en la que funcionará el dispositivo (`2.4 GHz`) e inicia sesión en la aplicación iDryer con tu cuenta del portal.
2. En la pantalla de inicio, toca **Conectar un dispositivo nuevo**: se abre el paso **Wi-Fi**.
3. Comprueba el nombre de la red (la aplicación lo rellena sola si la ubicación está activada), escribe la contraseña y toca **Conectar dispositivo**. La aplicación envía la configuración durante hasta 90 segundos; cuando el dispositivo se une a la red, aparece **Dispositivo conectado**. Toca **Siguiente**.
4. En el paso **Vinculación**, toca **Vincular**. La aplicación encuentra el dispositivo en la red, obtiene del portal un token de vinculación de un solo uso, se lo entrega al dispositivo y espera a que el portal confirme que el dispositivo está en línea.
5. Tras **Dispositivo vinculado**, el dispositivo aparece en la lista de dispositivos del portal y de la aplicación.

Si el dispositivo ya está en la red, abre directamente el paso **Vinculación**: toca su chip en la parte superior de la ventana.

El log muestra la vinculación:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## Verificación del resultado

En este punto el dispositivo debería estar Online en el portal. Todavía no hay datos de sensores; es lo esperado. Si algo salió mal:

- la aplicación no vio que el dispositivo se uniera a la red: revisa la contraseña y que la red sea `2.4 GHz`; con una contraseña incorrecta el dispositivo vuelve a esperar la configuración, repite el paso Wi-Fi;
- en el paso **Vinculación** la aplicación no encontró el dispositivo: el teléfono y el dispositivo deben estar en la misma red, y la red no debe bloquear el descubrimiento de dispositivos (las redes de invitados suelen hacerlo);
- el dispositivo se reinicia: revisa la alimentación del ESP32 (las caídas de tensión al arrancar son una causa habitual de reinicios);
- consulta [Errores de alimentación](../08-common-mistakes/02-power-mistakes.md) y [Errores del controlador](../08-common-mistakes/04-controller-mistakes.md).

## Qué sigue

La parte de red funciona. Proceda a [Sensores](05-sensors.md): conectaremos SHT31 y el termistor y veremos sus datos en el portal.
