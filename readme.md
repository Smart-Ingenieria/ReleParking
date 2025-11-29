# Control de relés 4 canales en Arduino Yun con API HTTP

Este repositorio incluye un sketch robusto para Arduino Yun que expone un endpoint HTTP que controla un módulo de relés de 4 canales conectado a los pines 2, 3, 4 y 5. El diseño contempla indicadores luminosos y acústicos, validaciones y límites de seguridad para operar portones de entrada y salida de un parqueadero.

## Resumen rápido del API
- IP objetivo: **192.168.0.220** (configurada en el sistema Linux del Yun; ver pasos). 
- Puerto del servidor: **5555**.
- Endpoint: `http://192.168.0.220:5555/relay?channel=<1-4>&action=<on|off|pulse>&pulse=<ms>`
  - `channel`: número de relé (1 a 4) asociado a los pines 2–5.
  - `action`: `on`, `off` o `pulse` (pulso seguro por defecto de 500 ms).
  - `pulse` (opcional): duración en milisegundos para pulsos, limitado a 5000 ms.
- Indicadores: LED 13 (estado/latido y éxito), LED 12 (errores) y zumbador en pin 8.

## Hardware esperado
- Módulo de 4 relés activo en nivel bajo (LOW enciende el relé). Si tu módulo es activo en nivel alto, invierte `RELAY_ON/RELAY_OFF` en el sketch.
- LED adicional en pin 12 para indicar errores (opcional pero recomendable).
- Zumbador piezoeléctrico en pin 8 para feedback audible (opcional pero soportado).

## Pasos para fijar la IP estática en el Yun (192.168.0.220)
> Estos pasos se ejecutan en la consola del Linux integrado del Arduino Yun (SSH). Asegúrate de que la interfaz de red correcta sea `eth1` o `wlan0` según uses cable o Wi-Fi.

1. **Conectarse por SSH**
   ```sh
   ssh root@arduino.local  # o la IP actual del Yun
   ```
2. **Configurar la IP estática**
   ```sh
   uci set network.lan.proto='static'
   uci set network.lan.ipaddr='192.168.0.220'
   uci set network.lan.netmask='255.255.255.0'
   uci set network.lan.gateway='192.168.0.1'
   uci set network.lan.dns='8.8.8.8'
   uci commit network
   /etc/init.d/network restart
   ```
3. **Verificar la IP**
   ```sh
   ifconfig br-lan  # o la interfaz configurada
   ```

## Pasos detallados para subir el sketch

1. **Instalar dependencias de IDE**: usa el IDE de Arduino (1.8.x o 2.x) con el paquete de tarjetas Arduino AVR instalado (Arduino Yun). Las librerías `Bridge`, `BridgeServer` y `BridgeClient` vienen con el soporte oficial del Yun.
2. **Abrir el proyecto**: copia `arduino_yun_relay.ino` a una carpeta llamada `arduino_yun_relay` y ábrela con el IDE.
3. **Seleccionar placa y puerto**:
   - Placa: `Arduino Yun`.
   - Puerto: `Arduino Yun at <puerto>` (puede aparecer como puerto de red si está en la misma LAN).
4. **Verificar el código**: usa el botón **Verify/Check** para compilar y asegurar que el IDE reconoce las librerías.
5. **Subir el sketch**: presiona **Upload**. Si el IDE no muestra la placa, conecta por USB y selecciona el puerto serie; el IDE cargará el código y reiniciará el lado Linux automáticamente.
6. **Prueba rápida**: desde un equipo en la misma red, ejecuta:
   ```sh
   curl "http://192.168.0.220:5555/relay?channel=1&action=pulse&pulse=700"
   ```
   El relé 1 debe activarse durante ~700 ms, el LED 13 debe parpadear y el zumbador emitir un tono corto.

## Buenas prácticas y seguridad incorporada
- **Validación de parámetros**: rechaza canales fuera de rango o acciones no soportadas con códigos HTTP descriptivos.
- **Límites de pulsos**: los pulsos se limitan a 5 segundos para evitar activaciones prolongadas accidentales.
- **Indicadores de estado**: latido en LED 13 cada segundo, patrones de éxito y error (LEDs y zumbador).
- **Relés inicializados a apagado**: al arrancar se fuerzan a estado seguro (`HIGH` en módulos activos en bajo).
- **Escucha en LAN**: `server.noListenOnLocalhost()` expone el endpoint a la red local; protege la red según tus políticas de firewall.

## Extensiones sugeridas
- Agregar autenticación HTTP básica (por ejemplo, verificando un token en la cadena de consulta) si la red no está segmentada.
- Integrar un watchdog por hardware para rearmar el Yun en caso de cuelgues.
- Registrar eventos en la memoria del lado Linux usando la clase `Process` si necesitas trazabilidad.
