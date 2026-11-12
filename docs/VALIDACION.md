# Validación de la versión 0.1.0

Actualizado: 11 de noviembre de 2026.

## Entorno comprobado

Linux x86_64, Qt 6.12.0, GCC 16.2.1. Interfaz Qt Quick ejecutada con plataforma offscreen y renderizado por software. No se cambió la configuración del escritorio.

## Resultados

`bin/build`: compilación correcta.

`bin/test`: **35 resultados satisfactorios, 0 fallos** (incluye inicialización y limpieza). Los escenarios funcionales comprueban:

- Hoja de 2400 × 3000 px y metadatos de 300 ppp; siete espacios sin solaparse y área blanca superior derecha de 2 × 4 pulgadas.
- Recortes con proporciones correctas y límites de ampliación; advertencias de resolución efectiva.
- Márgenes de 0, 0.1, 0.2, 0.3, 0.4, 0.5, 1, 2, 5 y 10 mm por lado solo en líneas interiores; conversión física a píxeles, conservación de las dimensiones de hoja y recuadros, y rechazo de valores fuera de rango o sin pasos de 0.1 mm.
- Persistencia del margen entre sesiones: se cierra y reconstruye el controlador con el mismo archivo de preferencias; se comprueban los valores decimales 0.2, 0.3 y 0.5 mm sin redondearlos a milímetros enteros, deshacer y conservación del tema.
- Comprobación de los lados y esquinas exteriores de las fotografías: alcanzan las orillas de la hoja, sin marco blanco. La vista de encuadre usa la posición real de su foto de referencia para respetar los lados interiores y exteriores.
- Margen global con giros independientes por foto: ambos archivos se reexportan con el nuevo margen, sin perder sus giros; deshacer/rehacer y selector QML de 0.1 en 0.1 mm comprobados.
- Temas claro y oscuro: persistencia de la elección, conmutación desde el selector, colores de los tres checkboxes y del valor de margen, menús desplegables, diálogo de ayuda, controles deshabilitados y capturas en ventana mínima.
- Comprobación numérica de 15 combinaciones de texto por tema (al menos 4.5:1) y cuatro combinaciones de bordes/pistas por tema (al menos 3:1). La imagen de impresión se compara antes y después del cambio de tema y conserva los mismos píxeles.
- Orientación EXIF de un JPEG de prueba.
- Valores iniciales y restablecidos: 3 × 2 y 4 × 3 a 90° a la izquierda, 4 × 6 sin giro; verificados al abrir fotos y al vaciar y volver a cargar el lote.
- Giros de 0°, 90°, 180° y 270° comprobados con marcadores de color; independencia entre los tres tamaños y repetición correcta de todas las copias.
- Giro conservado al cambiar el encuadre, deshacer/rehacer, restablecer, aplicar al lote e importar nuevas fotografías; advertencias de resolución calculadas después de girar.
- Búsqueda recursiva, estructura de subcarpetas, deduplicación e imágenes dañadas.
- Regresión reproducida antes de corregir: exportar PDF en la carpeta del original, quitar la foto e intentar volver a agregarla dejaba el lote vacío. Ahora se vuelve a agregar y el segundo PDF se genera con sufijo, conservando el primero; también se comprueban Vaciar lote y duplicados presentes.
- Carpeta de salida recordada entre sesiones, con espacios y caracteres Unicode; se reutiliza al exportar y al abrir el selector. Una carpeta eliminada no se activa y genera un mensaje.
- La búsqueda por carpetas excluye los archivos de salida, mientras que una imagen seleccionada individualmente en ese directorio sigue siendo válida. Las restricciones de carpetas de origen se retiran al quitar sus últimas fotos.
- Aviso visible de carga fallida, con motivo, comprobado en los temas claro y oscuro.
- Exportación JPG, PNG y PDF; fallos de escritura; conservación de los originales y de exportaciones anteriores.
- Opción **Adjuntar 8x10**, desactivada inicialmente: añade una foto vertical de 2400 × 3000 px a 300 ppp en JPG, PNG o PDF sin modificar la composición habitual. Se comprueban orientación EXIF, los cuatro giros, recorte sin deformación, posición y ampliación tomadas de 4 × 6, subcarpetas, sufijos sin sobrescritura, contador de archivos, pendientes al cambiar la opción y bloqueo durante la exportación. Activación y exportación por clic en ambos temas y a tamaño mínimo de ventana.
- Cancelación con adjuntos: conserva los dos archivos de cada fotografía terminada y deja las siguientes pendientes.
- Cancelación con conservación de archivos terminados.
- Procesamiento de **50 fotografías**, con 50 archivos de salida correctos.
- Progreso creciente y resúmenes de exportación correcta, con errores parciales y con todos los archivos fallidos; contadores y aviso final comprobados.
- Exportación desde la interfaz de un lote de **50 fotografías en cada tema**, con spinner, barra, porcentaje y contador; bloqueo de clics, atajos, Escape y cierre de ventana. Cancelación desde el diálogo, conservación de los archivos terminados, aviso final y recuperación de los controles. Capturas de progreso, cancelación y éxito revisadas visualmente en ambos temas; sin advertencias QML.
- Encuadres comunes, correcciones individuales, deshacer/rehacer y continuación de pendientes.
- Rechazo de carpetas de salida dentro del árbol de origen.
- Interfaz QML real: clic en tamaño, selección de giro, independencia del selector entre tamaños, movimiento de control de ampliación, deshacer por teclado y exportación por clic, con comparación de los archivos resultantes. Captura a tamaño mínimo de ventana de 1080 × 740; sin advertencias QML.

Comprobaciones adicionales de la CLI: JSON válido, carpetas recursivas, nombre `José.png`, errores con código de salida 1, instalación explícita de la habilidad y rechazo de reemplazo de una habilidad existente.

`pdfinfo`: una página de **576 × 720 puntos**, equivalente a **8 × 10 pulgadas**, sin rotación. Renderizado con `pdftoppm` e inspección visual del archivo de prueba. Captura de la interfaz revisada visualmente.

## Pendiente en Windows

El script PowerShell y el workflow de GitHub Actions están preparados para compilar, ejecutar esta suite y generar un ZIP portátil con Qt 6.8.3 / MSVC 2022 x64. **No se han ejecutado en Windows desde este entorno.** No se incluye ni se afirma haber validado un `.exe` de Windows.

Antes de distribuir a producción: ejecutar el workflow o el script en Windows y revisar una impresión física al 100 %, con papel de 8 × 10 y sin expansión de impresión sin bordes. Los encuadres permanecen en memoria y no se guardan entre sesiones en esta versión.

La CLI también se probó con `--margin-mm 0`, `1` y `2`: las separaciones medidas fueron 0, 24 y 48 píxeles entre fotos, respectivamente, a 300 ppp (dos márgenes por separación). Se aceptan márgenes por décimas, incluidos 0.1, 0.2, 0.3 y 0.4 mm; se rechazan precisiones no admitidas como 0.05 o 0.25 mm. Las pruebas de exportación PDF mantienen la página de 576 × 720 puntos.
