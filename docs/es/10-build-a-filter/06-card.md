---
title: "Filtro inteligente: tarjeta en el portal (card manifest)"
description: "Tarjeta dinámica del dispositivo: declaramos sensor VOC, modo, umbral y diseño a través de link.card() — el portal y la aplicación construyen la interfaz automáticamente."
---

# Tarjeta del dispositivo

Este es el capítulo principal de la sección. Aquí el dispositivo obtiene interfaz en el portal y en la aplicación móvil — **sin una sola línea de código en su lado**.

## Cómo funciona

El dispositivo publica un **card manifest** — descripción legible por máquina de "qué mostrar y cómo controlar". El portal y la aplicación leen el manifest y construyen la tarjeta: los sensores se convierten en celdas con valores en vivo, los controles en botones, campos de entrada y listas. El diseño también se puede especificar desde el firmware.

No necesitas publicar nada manualmente: declaras entidades a través de `link.card()`, y el núcleo automáticamente reúne el manifest y lo envía cuando se conecta.

## 1. Declaramos entidades

Todas las declaraciones se hacen en `setup()`, después de `s_link.begin()`. Nuestro filtro tiene tres entidades: lectura de VOC, lista de modos y campo de umbral. Desglosemos cada una por separado, y al final reuniremos el bloque completo.

### Principio general: id y label

Cada entidad tiene dos nombres, no los confundas:

- **id** — interno, nombre de máquina (`"voc"`, `"mode"`). Latín, dígitos, guión bajo, sin espacios. Por id la entidad se reconoce en el diseño, comandos y portal entre sí. Lo inventas una vez — no lo cambias;
- **label** — firma para humanos (`"VOC index"`, `"Mode"`). Lo que escribas, eso es lo que verá el usuario en la tarjeta. Puedes cambiarlo libremente.

### Sensor: lectura de VOC

```cpp
s_link.card().sensor(
    "voc",              // id: nombre interno de la entidad
    "VOC index",        // label: firma en la tarjeta
    "",                 // unit: unidad de medida a la derecha del número ("°C", "%", "g");
                        //   el índice VOC no tiene unidades — cadena vacía
    "units[0].vocIndex" // path: de dónde tomar el valor — ruta en el JSON de telemetría.
                        //   Este es ESE MISMO campo que agregamos en el capítulo 5:
                        //   doc["units"][0]["vocIndex"]. Los nombres deben coincidir
                        //   carácter por carácter, de lo contrario habrá un guión en la tarjeta.
);
```

Un sensor es una celda de "solo lectura": el portal toma el valor del JSON de telemetría por `path` y lo muestra. El sensor no tiene comando.

### Lista de selección: modo de trabajo

```cpp
// Variantes de la lista. El usuario las verá en el menú desplegable tal como están.
static const char* kModes[] = { "auto", "on", "off" };

s_link.card().select(
    "mode",                  // id: nombre interno de la entidad
    "Mode",                  // label: firma en la tarjeta
    kModes,                  // options: array de variantes (declarado arriba)
    3,                       // número de variantes en el array — auto, on, off = tres.
                             //   C++ no sabe la longitud del array por sí mismo, la decimos nosotros
    [](const char* opt) {    // callback: función que el núcleo llamará cuando
                             //   el usuario seleccione una variante en el portal.
                             //   opt — la cadena seleccionada, por ejemplo "on"
        onModeSelected(opt); //   la pasamos a nuestra lógica (escribimos en el capítulo 7)
    }
);
```

Aquí aparece la segunda mitad del mecanismo: **control**. Cuando el usuario elige una variante en el portal, el dispositivo recibe un comando, el núcleo mismo lo acepta, lo verifica (las cadenas ajenas que no están en `options` no llegará a ti) y llama a tu callback con el valor seleccionado. No necesitas analizar mensajes MQTT manualmente — tu área de responsabilidad comienza dentro de `onModeSelected`.

### Campo numérico: umbral de activación

```cpp
s_link.card().number(
    "threshold",       // id: nombre interno de la entidad
    "VOC threshold",   // label: firma en la tarjeta
    100,               // min: el portal no permitirá escribir menos que esto
    400,               // max: más que esto tampoco; el núcleo además
                       //   recorta el valor por estos límites en su lado
    10,                // step: paso de cambio de valor con las flechas del campo
    "",                // unit: unidad de medida; el índice no la tiene
    [](float v) {              // callback: se llama cuando el usuario envía
                               //   un nuevo valor; v — número dentro de min..max
        onThresholdChanged(v); //   lo pasamos a nuestra lógica (escribimos en el capítulo 7)
    }
);
```

### Lo reunimos todo

Vista final del bloque en `setup()` — lo que debe quedarse en tu código. Las funciones `onModeSelected` y `onThresholdChanged` las escribimos en el capítulo 7; para que el código compile ahora, declara sus versiones de prueba **arriba** de `setup()`:

```cpp
// Versiones de prueba: los cuerpos verdaderos los escribimos en el capítulo 7 (lógica de automatización).
static void onModeSelected(const char* opt) {}
static void onThresholdChanged(float v) {}

void setup() {
    Serial.begin(115200);
    s_link.begin();
    // El dispositivo se desvinculó en el portal: borrar el secreto y esperar una nueva vinculación.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetría: campo vocIndex propio (capítulo 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Tarjeta: sensor + dos órganos de control.
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });
}
```

¿Y el ventilador? **No necesitas** declararlo: la bandera `hasFan = true` en `Config` ya agregó la celda "Ventilador" al manifest automáticamente — es una habilidad del diccionario, el núcleo lo sabe todo sobre él.

!!! note "Los corchetes en los callbacks — siempre vacíos"
    `[](const char* opt) { ... }` — esto es una lambda, una función anónima; la explicamos en detalle en la [nota del capítulo 5](05-sensor-and-telemetry.md). Recordamos la regla del núcleo: los corchetes de captura siempre están vacíos (`[]`), no llevamos nada "contigo" a la lambda, todo lo necesario lo guardamos en variables globales — como `g_mode` y `g_threshold` del siguiente capítulo.

## 2. Diseño automático de la tarjeta

No tienes que especificar el diseño en absoluto. El portal reunirá la tarjeta a partir de las entidades declaradas — y la reunirá pulcramente: las celdas de lectura se agrupan en filas (hasta tres en una fila, luego salto de línea), los órganos de control van debajo, cada uno en su línea, todo en el diseño de marca del portal. Para la mayoría de dispositivos esto es suficiente — la interfaz queda pulida sin una sola reflexión sobre la maquetación.

El orden de entidades en la tarjeta es el orden de su declaración en `setup()`.

## 3. Tu propio diseño de la tarjeta (opcional)

Primero, cómo está organizada la tarjeta. La tarjeta es una pila vertical de **filas**. Una fila de celdas de sensores reparte el ancho a partes iguales: una celda ocupa todo el ancho, dos, la mitad cada una, tres, un tercio cada una. Los controles (lista, campo, botón) de una fila se colocan uno debajo de otro a todo el ancho.

El diseño automático del párrafo anterior distribuye las entidades en estas filas por sí solo. Si quieres decidir tú qué va junto a qué, — especifica las filas manualmente con llamadas `layoutRow`. Una llamada = una fila, el orden de llamadas = el orden de filas de arriba abajo:

```cpp
// Fila 1: dos celdas — índice VOC y ventilador, cada una por la mitad del ancho.
s_link.card().layoutRow("voc", "fan");

// Fila 2: dos controles — modo y umbral, uno debajo del otro.
s_link.card().layoutRow("mode", "threshold");
```

En `layoutRow` pasas los **id** de las entidades — esos nombres internos que les diste cuando las declaraste (por eso el id era necesario). `"fan"` — id de la entidad del ventilador del diccionario, la creó la bandera `hasFan`.

En la tarjeta esto dará esta composición:

```text
┌─ Air filter ────────────────────┐
│  ▮ 129            │  ≋ Apagado  │   ← fila 1: voc, fan
├───────────────────┴─────────────┤
│  Mode                           │   ← fila 2: mode, threshold
│  [auto                       ▾] │
│  VOC threshold                  │
│  [100                         ] │
└─────────────────────────────────┘
```

Fíjate en las celdas: el portal dibuja el icono y el valor, sin la firma. El `label` de la entidad va al nombre para los lectores de pantalla y a otras interfaces, y así se ahorra espacio en la tarjeta. En los órganos de control la firma sí se ve — `Mode` y `VOC threshold` en el esquema de arriba.

Las entidades que no menciones en ninguna fila no desaparecerán — el portal las dibujará abajo con lista automática. Así puedes diseñar solo lo "principal" y dejar el resto en la automática.

## 4. Qué va por el aire

El núcleo publicará en el tópico `idryer/{serial}/card` (retained):

```json
{
  "v": 1,
  "entities": [
    { "id": "fan",  "type": "binary_sensor", "device_class": "fan",
      "source": "telemetry", "path": "units[0].fanStatus" },
    { "id": "voc",  "type": "sensor", "label": "VOC index",
      "source": "telemetry", "path": "units[0].vocIndex" },
    { "id": "mode", "type": "select", "label": "Mode",
      "options": ["auto", "on", "off"], "action": "card.mode", "arg": "value" },
    { "id": "threshold", "type": "number", "label": "VOC threshold",
      "min": 100, "max": 400, "step": 10, "action": "card.threshold", "arg": "value" }
  ],
  "layout": [ ["voc", "fan"], ["mode", "threshold"] ]
}
```

No necesitas entender este JSON — el núcleo lo genera a partir de tus llamadas. Pero es útil saber: si escribes firmware **no** en `idryer-core` (Rust, MicroPython, lo que sea), es suficiente publicar ese JSON manualmente — el portal lo come todo, solo que el formato coincida.

## 5. Verificación

Graba el firmware y abre el **dashboard del portal** — la tarjeta vive allí mismo, donde las tarjetas de los dispositivos de fábrica, y en la aplicación móvil. En la página del propio dispositivo no está: allí hay gráficos de las magnitudes del diccionario, menú e integraciones.

![Tarjeta del filtro en el dashboard del portal](../../img/10-filter/06-portal-card.png)
*La tarjeta se arma según el manifest: el índice VOC y el ventilador en una fila, el modo y el umbral, abajo.*

![La misma tarjeta en la aplicación móvil](../../img/10-filter/06-app-card.png)
*La aplicación construye la tarjeta a partir del mismo manifest — no hace falta código aparte para el teléfono.*

- la celda del índice muestra el valor en vivo (sopla en el sensor — el número crece en la siguiente actualización);
- la celda del ventilador — Encendido/Apagado;
- **Mode** — lista desplegable, **VOC threshold** — campo numérico: el valor se envía unos 0,6 s después de cambiarlo, sin botón de confirmación. La lista y el campo empiezan en la primera opción y en el mínimo: envían comandos y no muestran el ajuste actual del dispositivo; el estado lo muestran las celdas (por ejemplo, el ventilador).

La selección de modo y umbral aún no hace nada — callbacks de prueba. Los animaremos en el [siguiente capítulo](07-auto-logic.md).

!!! note "Este es ese concepto mismo"
    Nota qué pasó: describiste la interfaz en cinco líneas en el firmware — y apareció en el portal y en la aplicación. El mismo truco funciona para cualquier dispositivo: solo cambian id, firmas y callbacks.

!!! note "Operaciones con arranque y parada"
    La lista, el campo y el botón envían un valor al momento. Las operaciones con arranque y parada —secado, calentamiento, iluminación— se declaran como **acciones** de la tarjeta con un modo y parámetros de arranque: la tarjeta muestra un formulario de arranque y, mientras dura la operación, un bloque de sesión con el botón de parada. Ejemplo: el [capítulo 7](../09-build-a-device/07-heating-control.md) de la sección del armario.
