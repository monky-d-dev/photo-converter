# Temas y contraste

La interfaz dispone de **Claro** y **Oscuro**, elegibles desde la barra superior. La preferencia se guarda en un archivo local de la aplicación. Los tests emplean un archivo temporal independiente para no modificar la preferencia del usuario.

## Paletas

| Función | Claro | Oscuro |
|---|---|---|
| Mesa de trabajo | `#e7edf3` | `#111b25` |
| Panel principal | `#ffffff` | `#1b2a38` |
| Campo editable | `#ffffff` | `#12202d` |
| Texto principal | `#172b3d` | `#f2f6fa` |
| Texto secundario | `#465c70` | `#b4c4d3` |
| Acento | `#135d8a` | `#82caff` |

Los colores de estados, selección, avisos, errores, foco, deshabilitado y papel están centralizados en `src/ThemePalette.qml`. Los checkboxes, selectores, margen y deslizadores dibujan expresamente sus fondos y textos para evitar mezclas con la paleta del escritorio.

## Comprobaciones

Se utilizan como referencia 4.5:1 para texto normal y 3:1 para los bordes y pistas de controles. El umbral de texto corresponde al criterio [Contraste mínimo de W3C](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum); para los controles se usa la referencia de [Contraste no textual de W3C](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast).

La suite verifica 15 combinaciones de texto por tema, incluidos texto secundario, selección, botones principales, deshabilitados, advertencias, errores y etiquetas sobre papel. También comprueba cuatro combinaciones de bordes/pistas por tema. Los valores se calculan con luminancia relativa sRGB sin redondear para decidir si pasan el umbral.

Se conduce la interfaz real con ratón y teclado en ambos temas: casillas marcadas y desmarcadas, cambio de tema, edición del margen, giro, menú desplegable, ayuda, exportación y estado sin fotos. Se inspeccionan capturas de la ventana normal y la mínima de 1080 × 740. Los diálogos extensos incluyen desplazamiento.

La hoja permanece blanca en ambos temas. Una comparación de la imagen antes y después de cambiar de tema confirma que la exportación no cambia sus píxeles.
