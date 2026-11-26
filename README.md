# Proyecto CAN clásico - S32K312

## 1. Resumen del proyecto

Base de laboratorio para una red CAN real entre dos a cuatro placas S32K312. La aplicación usa CAN clásico (no CAN FD), identificadores estándar de 11 bits y un bitrate nominal configurado de 500 kbit/s.

La aplicación reserva tres Message Buffers:

- MB0: transmisión.
- MB1: recepción del primer ID suscrito por el nodo.
- MB2: recepción del segundo ID suscrito por el nodo.

La selección del nodo, el transceiver, los filtros y los modos de prueba está centralizada en [`src/can_app_config.h`](src/can_app_config.h).

Estado actual de los defines en este workspace:

- `NODE_ID = 3` (`POWERTRAIN`).
- `CAN_HW_EXTERNAL` activo.
- `CAN_DEBUG_MODE` activo: aceptación abierta para depuración.
- `CAN_USE_RUNTIME_BITTIMING_PATCH = 1`: se mantiene el ajuste manual como respaldo.
- `ARBITRATION_TEST` desactivado; `INVALID_ID_TEST` activo para habilitar SW2.

En operación normal no hay transmisión periódica: cada pulsación estable de
SW3 (PTB26) genera una trama con `NODE_TX_ID`; cada pulsación de SW2 (PTB19)
genera una trama de prueba con ID `0x555`. `ARBITRATION_TEST` conserva el envío
periódico de 10 ms para la prueba específica de arbitraje. Los LEDs indican la
recepción por 5 segundos: Nodo 1 verde, Nodo 2 azul, Nodo 3 rojo y Nodo 4 blanco.

## 2. Arquitectura de hardware

| Define | Instancia FlexCAN | Pines MCU | Transceiver / conexión |
|---|---|---|---|
| `CAN_HW_INTERNAL` | FLEXCAN0 | CAN0_RX = PTA6, CAN0_TX = PTA7 | Transceiver integrado de MINI-EVB y terminales CANH/CANL de la placa |
| `CAN_HW_EXTERNAL` | FLEXCAN1 | CAN1_RX = PTA12, CAN1_TX = PTA11 | TJA1051 externo, alimentado a 5 V; pin S a GND según el montaje confirmado |

Conectar CANH con CANH y CANL con CANL, mantener una referencia GND común y colocar terminación únicamente en los dos extremos físicos del bus. Revisar si las placas ya incorporan terminación antes de añadir resistores.

Para `CAN_HW_EXTERNAL`, conectar el MCU al TJA1051 así: **PTA11 / CAN1_TX -> TXD** y **PTA12 / CAN1_RX <- RXD**. Conectar GND común, alimentar VCC del TJA1051 con 5 V, y mantener su pin S en GND para modo normal. Conectar CANH/CANL del transceiver al bus. No aplicar 5 V a PTA11 ni PTA12; la alimentación de 5 V del transceiver no sustituye la alimentación necesaria para el MCU de la tarjeta.

El pinmux generado configura ambos pares: CAN0/PTA6-PTA7 y CAN1/PTA11-PTA12.
También configura SW3/PTB26 y SW2/PTB19 como entradas activas en alto, y los
canales RGB PTA29 (rojo), PTA30 (verde) y PTA31 (azul) como salidas activas en
bajo. [`src/can_hw.c`](src/can_hw.c) inicializa el array de pines; el build activa
únicamente la instancia indicada por `CAN_HW_INTERNAL` o `CAN_HW_EXTERNAL`.

El `.mex` configura clocks de 24 MHz y pinmux para FLEXCAN0 y FLEXCAN1. El PBcfg genera `FlexCAN_Config0` y `FlexCAN_State0`; el RTD recibe la instancia seleccionada por separado y registra el state pointer para esa instancia. Este proyecto usa una sola instancia por firmware: FLEXCAN0 para interno o FLEXCAN1 para externo.

## 3. Configuración central

Editar [`src/can_app_config.h`](src/can_app_config.h) al preparar cada placa:

- `NODE_ID`: selecciona el rol y los IDs de transmisión/recepción; valores válidos: 1 a 4.
- `CAN_HW_INTERNAL` / `CAN_HW_EXTERNAL`: activar exactamente uno. Son defines de presencia; para desactivar uno, comentarlo, no asignarle `0`.
- `CAN_DEBUG_MODE`: si está definido, `CAN_RX_FILTER_MASK` es `0x000`; comentarlo para el filtrado estricto de evaluación.
- `ARBITRATION_TEST`: si está definido, transmite periódicamente cada 10 ms.
- `INVALID_ID_TEST`: si está definido, habilita SW2/PTB19 para transmitir una trama con `TEST_INVALID_ID` (`0x555`). SW3 sigue transmitiendo `NODE_TX_ID`.
- `NODE_TX_ID`, `NODE_RX_ID_1`, `NODE_RX_ID_2`: se seleccionan automáticamente según `NODE_ID`; no hace falta duplicar los IDs en `can_hw.c`.
- `CAN_BUTTON_INPUT_REGISTER`/`CAN_BUTTON_INPUT_MASK`: SW3/PTB26 (GPDI58).
- `CAN_INVALID_ID_BUTTON_REGISTER`/`CAN_INVALID_ID_BUTTON_MASK`: SW2/PTB19 (GPDI51).
- `CAN_BUTTON_ACTIVE_LEVEL`: ambos pulsadores son activos en alto.
- `CAN_BUTTON_DEBOUNCE_MS`: tiempo de estabilidad para aceptar una pulsación; actualmente 50 ms.
- `CAN_RX_LED_ON_TIME_MS`: tiempo de indicación por recepción; actualmente 5000 ms.
- `CAN_RX_FILTER_MASK`: máscara de comparación: `0x000` en debug (no comparar el ID) o `0x7FF` en evaluación (comparar los 11 bits). RXIMR usa la misma polaridad: `1` compara y `0` ignora; `can_hw.c` escribe los 11 bits directamente en el campo del ID estándar.
- `CAN_USE_RUNTIME_BITTIMING_PATCH`: define numérico; `1` mantiene la sobreescritura manual de timing y `0` usa los valores de `FlexCAN_Config0` generados por RTD. El valor actual es `1`.

`ARBITRATION_TEST` e `INVALID_ID_TEST` no pueden habilitarse juntos: el encabezado produce un error de compilación si ambos están definidos.

## 4. Configuración de nodos

| Nodo (`NODE_ID`) | Nombre | TX ID | RX ID 1 | RX ID 2 |
|---:|---|---:|---:|---:|
| 1 | `MASTER` | `0x100` | `0x200` | `0x300` |
| 2 | `DASHBOARD` | `0x200` | `0x100` | `0x400` |
| 3 | `POWERTRAIN` | `0x300` | `0x100` | `0x200` |
| 4 | `BODY_CTRL` | `0x400` | `0x100` | `0x300` |

Compilar una variante por placa con el `NODE_ID` correspondiente. Los IDs mostrados caben en el formato estándar de 11 bits.

## 5. Estructura del proyecto

### Código de aplicación

- [`src/main.c`](src/main.c): llama a `CanHw_Init()`, después a `CanApp_Init()` y ejecuta `CanApp_MainFunction()` en el bucle principal.
- [`src/can_app.c`](src/can_app.c): antirrebote de ambos pulsadores, envío por evento en modo normal, temporización de arbitraje, construcción del payload, procesamiento RX y temporizador de LEDs. El payload contiene NODE_ID, estado presionado, contador, el TX ID configurado, el último ID recibido y `0xA5, 0x5A`.
- [`src/can_app.h`](src/can_app.h): interfaz de aplicación y variables de diagnóstico exportadas.
- [`src/can_hw.c`](src/can_hw.c): inicializa reloj y pines, configura FlexCAN, MBs, máscaras, callbacks, TX/RX y contadores de error.
- [`src/can_hw.h`](src/can_hw.h): interfaz de hardware, estados y canales RX lógicos.
- [`src/can_app_config.h`](src/can_app_config.h): selección central de nodo, hardware y pruebas.

No hay salida UART de diagnóstico en esta aplicación. Durante el laboratorio, observar con el debugger `CanApp_ButtonPressCount`, `CanApp_InvalidIdPressCount`, `CanApp_RxCount`, `CanApp_TxCount`, `CanApp_LastRxId`, `CanApp_LastRxData`, `CanApp_LastHwStatus`, `CanHw_LastFlexcanStatus`, `CanHw_LastErrorStatus`, `CanHw_TxErrorCounter` y `CanHw_RxErrorCounter`.

### Configuración generada y RTD

- [`CAN.mex`](CAN.mex): fuente editable de la configuración de S32 Config Tools.
- [`generate/src/FlexCAN_Ip_Sa_PBcfg.c`](generate/src/FlexCAN_Ip_Sa_PBcfg.c): define `FlexCAN_Config0` y `FlexCAN_State0`, incluyendo timing generado.
- [`generate/include/FlexCAN_Ip_Cfg.h`](generate/include/FlexCAN_Ip_Cfg.h): capacidades y opciones del driver; que RTD tenga soporte para FD no significa que la aplicación esté usando CAN FD.
- [`generate/src/Clock_Ip_Cfg.c`](generate/src/Clock_Ip_Cfg.c): selección y división de clocks.
- [`board/Siul2_Port_Ip_Cfg.c`](board/Siul2_Port_Ip_Cfg.c): array de pinmux que consume `CanHw_Init()`, incluyendo ambos pulsadores y los tres pines RGB.
- [`RTD/src/FlexCAN_Ip.c`](RTD/src/FlexCAN_Ip.c) y [`RTD/include/FlexCAN_Ip_HwAccess.h`](RTD/include/FlexCAN_Ip_HwAccess.h): inicialización RTD y escritura de los campos de timing.
- `Project_Settings/Startup_Code/`: arranque y tabla de excepciones. El servicio actual llama al dispatcher de FlexCAN desde el bucle principal porque el vector externo del proyecto dirige interrupciones a `undefined_handler`; no habilitar además otro despacho sin revisar ambos caminos.

## 6. Message Buffers

La asignación está en `src/can_hw.c`:

| MB | Uso | Configuración |
|---:|---|---|
| 0 | TX | `CanHw_Transmit()` envía tramas estándar clásicas. |
| 1 | RX_ID_1 | Recibe el ID configurado como `NODE_RX_ID_1`; máscara individual. |
| 2 | RX_ID_2 | Recibe el ID configurado como `NODE_RX_ID_2`; máscara individual. |

En debug se arma MB1 como buzón monitor de todos los IDs estándar y MB2 queda sin armar para evitar dos buzones wildcard compitiendo por las mismas tramas. En evaluación se arman MB1 y MB2 para los dos IDs suscritos. Después de procesar una recepción, la aplicación vuelve a armar ese mismo MB.

## 7. Bit timing y bitrate

El bitrate nominal esperado es **500 kbit/s**.

### Reloj

El clock de referencia es FIRC a 48 MHz, seleccionado para FLEXCANA y dividido entre 2. El clock resultante para FlexCAN0/1 es **24 MHz**, según `CAN.mex` y `generate/src/Clock_Ip_Cfg.c`.

### Configuración RTD

Los valores de timing normal en el `.mex` son:

- PropSeg = 8 TQ
- PhaseSeg1 = 8 TQ
- PhaseSeg2 = 7 TQ
- Prescaler = 2
- SJW = 2 TQ

El PBcfg guarda los campos codificados de CTRL1 como `propSeg=7`, `phaseSeg1=7`, `phaseSeg2=6`, `preDivider=1`, `rJumpwidth=1`. En estos campos, el valor de registro representa el valor efectivo menos uno. Hay 24 TQ por bit incluyendo SyncSeg:

`24 MHz / (2 × (1 + 8 + 8 + 7)) = 500 000 bit/s`

### Configuración runtime de respaldo

Con `CAN_USE_RUNTIME_BITTIMING_PATCH = 1`, `can_hw.c` asigna directamente los mismos campos codificados `7, 7, 6, 1, 1` antes de llamar a `FlexCAN_Ip_Init()`. Con el define en `0`, se conservan los valores del `FlexCAN_Config0` generado.

El parche manual se conserva deliberadamente como respaldo; **no eliminarlo sin validación en hardware**. No se ha medido el bitrate del bus con analizador en esta documentación. Si se regenera la configuración, comprobar que el PBcfg siga coincidiendo con el `.mex` antes de seleccionar el modo RTD puro.

La aplicación fuerza `fd_enable = FALSE`, `bitRateSwitch = FALSE` y `FLEXCAN_NORMAL_MODE`; por tanto, usa CAN clásico aunque el driver RTD anuncie soporte para FD.

## 8. Filtros

La aplicación programa máscaras de hardware individuales. RXIMR usa `1` para comparar el bit correspondiente del ID y `0` para ignorarlo:

- **Modo debug (`CAN_DEBUG_MODE` definido):** `CAN_RX_FILTER_MASK = 0x000`; los bits de ID de RXIMR quedan en cero, así que no compara el ID y MB1 acepta cualquier ID estándar de datos. Solo MB1 se arma como monitor.
- **Modo evaluación (`CAN_DEBUG_MODE` comentado):** `CAN_RX_FILTER_MASK = 0x7FF`; RXIMR recibe `0x1FFC0000` en los bits de ID y compara los 11 bits del ID configurado en cada MB. Se arman MB1 y MB2.

Para la entrega, usar el modo estricto: comentar `CAN_DEBUG_MODE`, seleccionar el `NODE_ID` correcto y dejar desactivadas las pruebas temporales. La máscara lógica es `0x7FF`; su representación en RXIMR tiene los 11 bits del ID activados.

La máscara de aceptación no controla el ACK del protocolo. Un nodo puede reconocer eléctricamente una trama y emitir ACK aunque el filtro de aceptación no la entregue a MB1/MB2.

## 9. Procedimiento de pruebas

### ETAPA 1 — Prueba física básica

1. Empezar con `CAN_DEBUG_MODE` definido y `ARBITRATION_TEST` / `INVALID_ID_TEST` desactivados.
2. Seleccionar el hardware y el `NODE_ID` adecuados en cada placa.
3. Conectar al menos dos nodos al bus real, con CANH/CANL correctos, GND común y terminación apropiada en los extremos.
4. Confirmar con analizador que hay tramas clásicas estándar a 500 kbit/s y que otro nodo activo genera ACK.
5. Presionar SW3/PTB26 en un nodo. Debe generar una sola solicitud de envío por pulsación estable, no un envío periódico. Revisar `CanApp_ButtonPressCount` y `CanApp_TxCount`.
6. En debug, los nodos aceptan cualquier trama de datos estándar y encienden su indicador asignado por 5 segundos. El analizador puede mostrar retransmisiones si el controlador no recibe ACK; eso no implica que la aplicación haya vuelto a solicitar la trama.

### ETAPA 2 — Prueba de filtros

1. Comentar `CAN_DEBUG_MODE` y compilar las placas con sus `NODE_ID` correctos.
2. Mantener desactivados los dos defines de prueba.
3. Confirmar que cada nodo recibe solo sus dos IDs suscritos en MB1/MB2 y enciende su color por 5 segundos. Los demás IDs no deben actualizar `CanApp_RxCount` ni encender el LED.
4. Si el bus tiene ACK pero la aplicación no recibe, revisar los IDs y la máscara antes de modificar el bit timing.

### ETAPA 3 — Prueba de ID inválido

1. Mantener el modo estricto: `CAN_DEBUG_MODE` debe estar comentado y `INVALID_ID_TEST` definido en el transmisor de prueba.
2. Dejar `ARBITRATION_TEST` desactivado y presionar SW2/PTB19. El analizador debe decodificar el ID estándar `0x555`; es un ID válido de 11 bits pero no está suscrito por ningún nodo.
3. Los receptores estrictos no deben incrementar `CanApp_RxCount` ni encender el LED por esa trama. Que aparezca ACK dominante es normal: el ACK confirma recepción a nivel CAN y no implica aceptación por el filtro de aplicación.
4. SW3/PTB26 debe seguir transmitiendo el ID normal del nodo.
5. Desactivar `INVALID_ID_TEST` cuando ya no se requiera SW2 como transmisor de prueba.

### ETAPA 4 — Prueba de arbitraje

1. Dejar `INVALID_ID_TEST` desactivado y habilitar `ARBITRATION_TEST` en los nodos contendientes.
2. Las transmisiones se intentan cada 10 ms. Preparar una contienda simultánea; si los arranques de los nodos quedan desfasados, las tramas pueden no competir en el mismo instante.
3. Capturar al menos dos IDs que soliciten el bus simultáneamente. El identificador numéricamente menor gana, por ejemplo `0x100` frente a `0x200` o `0x300`.
4. Confirmar que el perdedor reintenta según el protocolo y que no hay errores de bit ni corrupción del payload.
5. Desactivar el define al finalizar.

### ETAPA 5 — Integración de más nodos

1. Agregar los nodos uno por uno, asignando `NODE_ID` 1–4 y el transceiver correcto en cada build.
2. Confirmar que cada nodo transmite su ID y recibe los dos definidos en la tabla de la sección 4.
3. Conservar un bus lineal; terminar solo en los dos extremos y comprobar que las placas no agreguen terminaciones duplicadas.
4. Repetir primero en debug y luego en modo estricto de evaluación.

## 10. Troubleshooting

| Síntoma | Revisar |
|---|---|
| Pulsar un switch no genera trama | Revisar los contadores `CanApp_ButtonPressCount` / `CanApp_InvalidIdPressCount`; comprobar PTB26/GPDI58 para SW3 o PTB19/GPDI51 para SW2, el pinmux y `INVALID_ID_TEST`. `CanApp_TxCount` aumenta al completar TX; revisar MB0 y ACK de otro nodo activo. |
| ID `0x555` tiene ACK pero no enciende LED en modo estricto | Es el resultado esperado: los nodos pueden ACKear la trama CAN aunque el filtro hardware no la entregue a la aplicación. |
| No aparece ACK | Verificar que haya otro nodo alimentado y conectado al mismo bus; modo normal; bitrate y clock iguales; CANH/CANL, GND, alimentación y estado del transceiver. Revisar `CanHw_TxErrorCounter`, `CanHw_LastErrorStatus` y `CanHw_LastFlexcanStatus`. Un filtro estricto no debería impedir el ACK de una trama válida. |
| Bus-Off | Revisar ausencia de ACK, bitrate incorrecto, cortos, polaridad CANH/CANL y terminación. Observar `CanHw_LastErrorStatus`, contadores TX/RX y `CanApp_LastHwStatus`. La recuperación automática habilitada en la configuración RTD no corrige la causa física. |
| RX no recibe | Verificar que MB1/MB2 estén armados y rearmados, que `CanHw_Service()` siga ejecutándose y que el ID del emisor coincida con `NODE_RX_ID_1/2`. Comprobar también el pinmux del hardware seleccionado: CAN0/PTA6-PTA7 para interno o CAN1/PTA11-PTA12 para externo. |
| Filtros bloquean tráfico | Revisar `CAN_DEBUG_MODE`, `CAN_RX_FILTER_MASK`, `NODE_ID` y los IDs de suscripción. `0x7FF` es exacto para IDs estándar de 11 bits; `0x000` es modo de depuración. |
| Bitrate incorrecto | Confirmar FIRC 48 MHz / divisor 2 = 24 MHz, los campos normales del `.mex` y el PBcfg, y el valor de `CAN_USE_RUNTIME_BITTIMING_PATCH`. Con el parche en `1`, revisar también las asignaciones en `can_hw.c`; con `0`, el timing efectivo proviene de `FlexCAN_Config0`. Medir en el bus: el cálculo de configuración no sustituye la validación con analizador. |

## 11. Elementos críticos

### ARCHIVOS QUE NO DEBEN MODIFICARSE SIN REVISIÓN

- `RTD/**`: código del driver NXP; no editar para resolver problemas de aplicación.
- `generate/**`: archivos generados; cualquier cambio manual puede perderse al regenerar. Modificar primero `CAN.mex` en Config Tools y regenerar; después comparar el resultado.
- `board/Siul2_Port_Ip_Cfg.*`: pinmux generado. No cambiar solo el comentario YAML ni copiar una entrada sin validar su mux y selector de entrada.
- `Project_Settings/Startup_Code/**`: la ruta de IRQ actual afecta al despacho periódico de `CanHw_Service()`. Cambiarla requiere revisar que no haya despacho doble ni handlers sin conectar.
- Los defines de `src/can_app_config.h` y los índices de MB en `src/can_hw.c`: cambiar uno puede alterar roles, filtros, prioridades de arbitraje o la recepción completa.

### ARCHIVOS DONDE NORMALMENTE SE HARÁN CAMBIOS

- `src/can_app_config.h`: nodo, hardware, modos de prueba y selección del bitrate.
- `src/can_app.c`: política de transmisión, periodo, payload y procesamiento de mensajes.
- `src/can_hw.c` / `src/can_hw.h`: configuración FlexCAN, filtros, callbacks y asignación de MBs.
- `CAN.mex`: clocks, periféricos y pinmux mediante S32 Config Tools; regenerar los archivos asociados después de editarlo.
- `README.md`: mantener esta guía sincronizada con cualquier cambio validado de arquitectura o procedimiento de laboratorio.

## Checklist antes de flashear — Nodo 3 externo

- **Archivos modificados:** `src/can_app_config.h`, `src/can_hw.c` y este `README.md`. No se modifican los archivos generados ni la arquitectura de los otros nodos.
- **Defines:** `NODE_ID = 3`; `CAN_HW_EXTERNAL` y `CAN_DEBUG_MODE` definidos; `CAN_HW_INTERNAL` y `ARBITRATION_TEST` comentados; `INVALID_ID_TEST` definido; `CAN_USE_RUNTIME_BITTIMING_PATCH = 1U` sin cambios.
- **IDs resultantes:** `NODE_TX_ID = 0x300`, `NODE_RX_ID_1 = 0x100`, `NODE_RX_ID_2 = 0x200`; SW2 transmite `TEST_INVALID_ID = 0x555`.
- **Instancia:** FLEXCAN1 (`CAN_FLEXCAN_INSTANCE = 1U`).
- **Pines al TJA1051:** PTA11 / CAN1_TX -> TXD; PTA12 / CAN1_RX <- RXD; GND común; VCC del TJA1051 a 5 V; S a GND.
- **Bus:** CANH del TJA1051 a CANH y CANL a CANL; usar terminación solo en los dos extremos físicos del bus.
- **Message Buffers:** MB0 TX, MB1 RX_ID_1, MB2 RX_ID_2.
- **Pulsadores:** SW3/PTB26 transmite el ID normal `0x300`; SW2/PTB19 transmite `0x555` con `INVALID_ID_TEST`. Ambos activos en alto con debounce de 50 ms.
- **Indicador de recepción:** Nodo 3 rojo, activo en bajo, encendido 5 segundos tras cada trama aceptada.
- **Bitrate esperado:** 500 kbit/s nominales con clock FLEXCAN1 de 24 MHz. El parche runtime permanece habilitado.
- **Frecuencia de transmisión esperada:** una solicitud por pulsación; `ARBITRATION_TEST` activa el modo periódico de 10 ms.
- **Filtros:** máscara abierta `0x000` para esta prueba física, porque `CAN_DEBUG_MODE` está activo.
