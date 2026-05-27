🫀 Monitor ECG — AD8232 · ESP32 · Node.js · MySQL
Sistema de monitoreo cardíaco en tiempo real con transmisión cifrada desde un ESP32 + AD8232, almacenamiento seguro en MySQL, visualización web profesional y exportación de datos.
Usa Node.js + WebSocket para la comunicación, cifrado en tránsito (AES-128-CBC), cifrado en reposo (AES-256-GCM) e integridad HMAC-SHA256 por cada registro.

📁 Estructura del proyecto
text
ecg-monitor/
├── public/
│   └── index.html          ← Interfaz web (gráfica ECG, controles, exportación)
├── server.js               ← Servidor Node.js + WebSocket + API REST
├── esquema_bd.sql          ← (opcional) Script de creación de tablas
└── README.md
⚙️ Cómo funciona
text
ESP32+AD8232 ──(AES-128-CBC)──> WebSocket ──> server.js
                                      │
                                Descifra tránsito
                                      │
                          ┌───────────┴───────────┐
                          │                       │
                    Cifra en reposo         Broadcast a navegadores
                    (AES-256-GCM)           (datos en claro)
                          │                       │
                      MySQL                   Interfaz web
                   (muestras + HMAC)         (Chart.js, FC, export)
ESP32 lee el sensor AD8232 a 200 Hz y cifra cada bloque de muestras con AES-128-CBC.

Envía por WebSocket el paquete binario IV (16 bytes) + datos cifrados.

El servidor descifra, aplica padding PKCS#7 y obtiene las muestras (valores ADC).

Cifra cada muestra por separado con AES-256-GCM, calcula un HMAC-SHA256 del registro completo y lo guarda en MySQL.

Retransmite las muestras en claro a todos los navegadores conectados para la visualización en tiempo real.

La interfaz web dibuja la señal ECG, calcula frecuencia cardíaca, permite ajustar la vista, pausar, y exportar datos (desde la BD o desde el buffer local).

🧱 Dependencias
Node.js (servidor)
bash
npm install express ws mysql2 exceljs
ws es el módulo WebSocket, mysql2 para MySQL, exceljs para exportar Excel.

Frontend
Chart.js (CDN)

SheetJS (CDN, para exportación local)

Fuentes de Google Fonts

No se requieren dependencias Python. Todo el procesamiento de voz no aplica aquí.

🗄️ Configuración de la base de datos
Crea una base de datos MySQL (por ejemplo ecg_db) y ejecuta las siguientes tablas:

sql
CREATE TABLE sesiones (
  id INT AUTO_INCREMENT PRIMARY KEY,
  paciente VARCHAR(100) DEFAULT 'Anónimo',
  notas TEXT,
  inicio TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  fin TIMESTAMP NULL,
  activa BOOLEAN DEFAULT TRUE
);

CREATE TABLE lecturas (
  id INT AUTO_INCREMENT PRIMARY KEY,
  sesion_id INT NOT NULL,
  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  muestra_cifrada TEXT NOT NULL,
  hmac VARCHAR(128) NOT NULL DEFAULT 'pendiente',
  FOREIGN KEY (sesion_id) REFERENCES sesiones(id)
);

CREATE TABLE audit_log (
  id INT AUTO_INCREMENT PRIMARY KEY,
  evento VARCHAR(50),
  sesion_id INT,
  detalle TEXT,
  ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
El servidor se conecta usando las variables de entorno o los valores por defecto definidos en server.js. Ajusta DB_HOST, DB_USER, DB_PASSWORD y DB_NAME según tu instalación.

🚀 Ejecutar el servidor
Clona o copia los archivos del proyecto.

Instala las dependencias de Node.

Asegúrate de que MySQL esté corriendo y las tablas creadas.

Inicia el servidor:

bash
node server.js
Abre http://localhost:3000 en el navegador. Permite el acceso al micrófono (no se usa, pero el navegador puede pedirlo por WebSocket, aunque aquí no hay captura de audio).

🌐 Endpoints REST
Método	Ruta	Descripción
POST	/api/sesion/abrir	Abre una nueva sesión. Body: { paciente, notas }
POST	/api/sesion/cerrar	Cierra la sesión actual.
GET	/api/sesiones	Lista las últimas 50 sesiones.
GET	/api/verificar-integridad	Verifica HMAC de los últimos registros. ?limit=n
GET	/api/export/excel	Descarga Excel con datos históricos. ?limit=n&from=&to=
GET	/api/audit	Consulta el log de auditoría. ?limit=n
📡 Protocolo WebSocket
Identificación del ESP32: el ESP32 envía el texto ESP32. El servidor lo marca y abre una sesión automáticamente.

Datos binarios: el ESP32 envía paquetes con la estructura:

text
[IV 16 bytes] [datos cifrados con AES-128-CBC]
Los datos descifrados son un array de enteros de 16 bits (little-endian) que representan lecturas del ADC (0-4095).

Broadcast a navegadores: el servidor envía a todos los clientes no-ESP32 un JSON:

json
{
  "type": "ecg",
  "data": [2048, 2056, 2034, ...]
}
También envía {"type":"status","connected":true/false} al cambiar el estado del ESP32.

🔐 Capas de seguridad
Capa	Algoritmo	Clave / Derivación	Propósito
Tránsito	AES-128-CBC	Clave fija de 16 bytes (debe coincidir con el firmware del ESP32)	Evitar escuchas en la red WiFi
Reposo	AES-256-GCM	Derivada con scryptSync de una passphrase (DB_PASSPHRASE)	Cada muestra se guarda cifrada e íntegra en MySQL
Integridad	HMAC-SHA256	Derivada con scryptSync de otra passphrase (HMAC_PASSPHRASE)	Detecta modificaciones en cualquier campo de la fila
El HMAC se calcula sobre la concatenación id|timestamp|sesion_id|muestra_cifrada.
La verificación se puede hacer con el endpoint /api/verificar-integridad.

📊 Interfaz web (index.html)
Gráfica de línea en tiempo real con Chart.js, fondo oscuro estilo monitor médico.

Ajustes: ventana de tiempo (1 s a 10 s), escala vertical, grosor de trazo.

Indicadores de: conexión del ESP32, frecuencia cardíaca (calculada con detección de picos), amplitud pico-pico, último valor ADC.

Pausa / reanudación de la gráfica.

Exportación de datos:

Primero intenta descargar desde la base de datos usando el endpoint REST.

Si falla (sin BD, error), exporta automáticamente los datos del buffer local con SheetJS.

Barra de estado con hora, método de exportación y capas de seguridad.

🔌 Conexión del AD8232 al ESP32
El sensor AD8232 (módulo típico de 3 electrodos) se conecta al ESP32 de la siguiente manera:

Pin AD8232	Pin ESP32
GND	GND
3.3V	3.3V
OUTPUT	GPIO34 (ADC)
LO+	GPIO32 (opcional, detección de electrodo)
LO-	GPIO33 (opcional)
Los electrodos se colocan según la derivación I de Einthoven (brazo derecho, brazo izquierdo, pierna derecha).
La señal analógica se muestrea a 200 Hz y se transmiten bloques de muestras.

🧠 Firmware del ESP32 (consideraciones)
Debe conectarse al mismo WiFi que el servidor.

IP del servidor configurable (por ejemplo en secrets.h).

Clave AES-128 fija igual a AES_TRANSIT_KEY del servidor.

Al iniciar, enviar el texto ESP32 por WebSocket.

Leer el ADC en un timer a 200 Hz, acumular N muestras, cifrarlas en modo CBC con un IV aleatorio y enviar el paquete binario.

El servidor descifrará y procesará.

📝 Notas finales
El servidor genera automáticamente una nueva sesión en MySQL al conectar un ESP32 si no hay una activa.

Los datos se almacenan con marcas de tiempo UTC.

Se mantiene un log de auditoría de eventos importantes (conexiones, exportaciones, errores).

Las claves de cifrado están hardcodeadas como ejemplo; en producción deben venir de variables de entorno seguras.

El idioma de la interfaz es español, pero la lógica es independiente del idioma.

📄 Licencia
Uso educativo y experimental. No apto para diagnóstico médico sin las certificaciones correspondientes.
