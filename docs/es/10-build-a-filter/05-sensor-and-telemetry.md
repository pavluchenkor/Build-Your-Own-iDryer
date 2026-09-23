---
title: "Filtro inteligente: sensor VOC y telemetría"
description: "Lectura de SGP40 por I2C y publicación de campo vocIndex propio en telemetría de iDryer a través del callback onTelemetryPublish."
---

# Sensor y telemetría

En este capítulo el filtro comienza a medir el aire y enviar datos a la nube. La técnica clave es **tu propio campo en telemetría**: el diccionario del ecosistema no sabe nada sobre VOC, pero el núcleo permite agregar cualquier campo a la telemetría.

## 1. Biblioteca del sensor

En `platformio.ini` agrega a `lib_deps`:

```ini
    adafruit/Adafruit SGP40 Sensor
```

## 2. Leer SGP40

En `src/main.cpp` (los pines son del [esquema](03-wiring.md)):

```cpp
#include <Wire.h>
#include <Adafruit_SGP40.h>

static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;   // -1 = sin datos aún

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // measureVocIndex() realiza su propia compensación interna del sensor.
    // Índice: ~100 = aire normal, más alto = más sucio (máx 500).
    g_vocIndex = s_sgp.measureVocIndex();
}
```

Agrega la llamada a `initVocSensor()` en `setup()` después de `s_link.begin()`, y `readVocSensor()` — en `loop()` una vez por segundo (con temporizador millis, ¡no a través de `delay`!):

```cpp
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();
    }
}
```

!!! warning "Sin delay() en loop"
    `s_link.loop()` debe llamarse constantemente — en él se mantienen Wi-Fi, MQTT y comandos del portal. `delay(1000)` congelará todo esto. Solo temporizadores millis.

## 3. Tu propio campo en telemetría

Cada `telemetryPeriodMs` el núcleo reúne automáticamente un mensaje JSON de telemetría y lo envía a la nube. Para tu dispositivo (una unidad, solo ventilador del diccionario) el núcleo reúne este mensaje:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false } ],
  "rssi": -52,
  "uptime": 120
}
```

Desglosemos la estructura:

- `units` — array de **unidades** (cámaras) del dispositivo. El secador iDryer en serie puede tener hasta cuatro cámaras independientes, por eso la telemetría es siempre un array, incluso si hay una cámara;
- `units[0]` — primera (y única para nosotros) unidad: especificamos `unitsCount = 1` en `Config`;
- `fanStatus` — campo del diccionario, apareció porque `hasFan = true`;
- `rssi`, `uptime` — nivel de Wi-Fi y tiempo de trabajo, el núcleo siempre agrega esto.

No hay nada sobre VOC en este mensaje — el núcleo no sabe del sensor. Pero justo antes de enviar, el núcleo le da al código la oportunidad de agregar sus propios campos al mensaje. Para hacer esto, registras un **callback** (devolución de llamada, "llamada inversa") — una función que le das al núcleo, y el núcleo mismo la llama en cada publicación, pasando adentro el JSON recopilado (el argumento `doc` es ese).

En `setup()`:

```cpp
s_link.onTelemetryPublish([](JsonObject doc) {
    // doc — ese mismo mensaje de telemetría recopilado por el núcleo (ver JSON arriba).
    // Agregamos nuestro campo vocIndex a la primera unidad.
    if (g_vocIndex >= 0) {
        doc["units"][0]["vocIndex"] = g_vocIndex;
    }
});
```

La línea `doc["units"][0]["vocIndex"] = g_vocIndex;` se lee así: "en el mensaje `doc` toma el array `units`, en él el elemento `0` (nuestra única unidad) y escribe allí el campo `vocIndex`". El nombre del campo lo eliges tú — en el [siguiente capítulo](06-card.md) harás referencia a él para mostrar el valor en la tarjeta.

!!! note "Si encuentras la palabra hook"
    En el código fuente del núcleo este callback se llama `PublishHook` — "hook" ("gancho") significa lo mismo: un punto donde la biblioteca te permite "enganchar" tu función. Los términos son intercambiables; en esta documentación decimos "callback".

!!! note "Lambda y por qué está "vacía""
    La construcción `[](JsonObject doc) { ... }` se llama **lambda** — es una función sin nombre, escrita en el lugar de uso, para no separarla y no inventar un nombre.

    Los corchetes al principio — "lista de captura": en ellos enumeras las variables locales que la función toma consigo. La regla del núcleo: **los corchetes siempre están vacíos** (`[]`) — la lambda no captura nada y no arrastra ningún estado (esto se llama *stateless*, "sin estado").

    La razón es técnica: las lambdas con captura requieren memoria dinámica, y sus asignaciones frecuentes en ESP32 fragmentan el montón y en el peor caso derriban Wi-Fi. Por eso el núcleo solo acepta funciones simples.

    Conclusión práctica: todo lo que el callback necesita, guárdalo en variables **globales** — como nuestra `g_vocIndex`. Esta regla aplica a todos los callbacks de `idryer-core`.

El estado del ventilador se publica por la ruta del diccionario — simplemente escribe en el campo del núcleo cuando enciendas/apagues (la lógica en el [capítulo 7](07-auto-logic.md)):

```cpp
s_link.telemetry.fanOn[0] = fanIsOn;
```

## 4. ¿No tienes un sensor a mano? Modo demo

Puedes recorrer el camino hasta la tarjeta también sin SGP40: el índice lo calculará un modelo del aire. Copia el archivo `demo_voc.h` del ejemplo:

```bash
git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
cp ~/byo-idryer/example/10-filter/src/demo_voc.h src/
```

y agrega en `platformio.ini` la bandera de compilación:

```ini
build_flags =
    -DDEMO_VOC=1
```

Ambas ramas están escondidas tras una sola función, y `loop()` no sabe de dónde vino el valor:

```cpp
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}
```

El modelo se comporta como una habitación en la que imprime una impresora: mientras el ventilador está parado, el índice sube desde un valor de fondo cercano a `100`, y con el ventilador en marcha baja. El modelo toma el estado del ventilador de tu propia lógica, por eso el umbral y la histéresis del [capítulo 7](07-auto-logic.md) se ven en funcionamiento.

Para un dispositivo real no se pone la bandera: entonces se compila la rama con el sensor de verdad.

## 5. Verificación

Después de cargar el firmware en el flujo MQTT del dispositivo (o en el registro Serial de publicaciones) la telemetría se ve así:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false, "vocIndex": 103 } ],
  "rssi": -52,
  "uptime": 120
}
```

`vocIndex` — tu propio campo, llegó a la nube junto al `fanStatus` del diccionario. El portal ya lo recibe, pero aún no sabe qué hacer con él: muéstrale esto en el siguiente capítulo.

!!! note "Tu propio campo vive en tiempo real"
    El portal reparte la telemetría a los clientes tal cual, por eso la tarjeta muestra `vocIndex` de inmediato. Pero en el historial para los gráficos el servidor escribe solo las magnitudes del diccionario — temperatura, humedad, calentamiento. Tu propio campo no estará en el gráfico de telemetría; si necesitas historial, recógelo por tu cuenta (por ejemplo, Home Assistant o tu propia base de datos).

Respira sobre el sensor o acerca un marcador — el índice debe crecer notablemente en segundos.
